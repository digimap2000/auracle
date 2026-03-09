#include "discovery.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "discovery";

#define REPORT_RING_SIZE   CONFIG_AURACLE_MSG_RING_SIZE
#define DEVICE_TABLE_SIZE  32

typedef struct {
    bool    in_use;
    char    addr[DISCOVERY_ADDR_MAX];
    char    addr_type[DISCOVERY_ADDR_TYPE_MAX];
    uint32_t seen_count;
    int64_t first_seen_us;
    int64_t last_seen_us;
    int     last_rssi;
    int     strongest_rssi;
    int     adv_type;
    int     adv_props;
    int     interval;
    int     sid;
    int     primary_phy;
    int     secondary_phy;
    int     data_len;
    char    data_hex[DISCOVERY_HEX_MAX + 1];
    bool    local_name_complete;
    char    local_name[DISCOVERY_NAME_MAX];
    bool    has_manufacturer_id;
    uint16_t manufacturer_id;
    bool    has_service_data_uuid16;
    uint16_t service_data_uuid16;
    bool    has_broadcast_audio_announcement;
    bool    payload_parse_error;
} discovery_device_t;

static discovery_report_t s_reports[REPORT_RING_SIZE];
static discovery_device_t s_devices[DEVICE_TABLE_SIZE];
static uint64_t s_next_seq;
static uint64_t s_total_reports;
static SemaphoreHandle_t s_mutex;

static void copy_str(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static bool copy_json_str(char *dst, size_t dst_size, const cJSON *obj, const char *key)
{
    const cJSON *item = cJSON_GetObjectItem(obj, key);
    if (!cJSON_IsString(item) || !item->valuestring) {
        return false;
    }
    copy_str(dst, dst_size, item->valuestring);
    return true;
}

static bool get_json_int(const cJSON *obj, const char *key, int *out)
{
    const cJSON *item = cJSON_GetObjectItem(obj, key);
    if (!cJSON_IsNumber(item) || !out) {
        return false;
    }
    *out = (int)item->valuedouble;
    return true;
}

static int hex_nibble(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + (ch - 'a');
    }
    if (ch >= 'A' && ch <= 'F') {
        return 10 + (ch - 'A');
    }
    return -1;
}

static size_t hex_to_bytes(const char *hex, uint8_t *dst, size_t dst_size)
{
    size_t hex_len;

    if (!hex || !dst) {
        return 0;
    }

    hex_len = strlen(hex);
    if ((hex_len & 1u) != 0u || (hex_len / 2u) > dst_size) {
        return 0;
    }

    for (size_t i = 0; i < hex_len / 2u; i++) {
        int hi = hex_nibble(hex[i * 2u]);
        int lo = hex_nibble(hex[i * 2u + 1u]);
        if (hi < 0 || lo < 0) {
            return 0;
        }
        dst[i] = (uint8_t)((hi << 4) | lo);
    }

    return hex_len / 2u;
}

static void bytes_to_hex(const uint8_t *src, size_t src_len, char *dst, size_t dst_size)
{
    static const char hex_chars[] = "0123456789abcdef";
    size_t needed = (src_len * 2u) + 1u;

    if (!dst || dst_size == 0) {
        return;
    }
    if (!src || needed > dst_size) {
        dst[0] = '\0';
        return;
    }

    for (size_t i = 0; i < src_len; i++) {
        dst[i * 2u] = hex_chars[(src[i] >> 4) & 0x0f];
        dst[i * 2u + 1u] = hex_chars[src[i] & 0x0f];
    }
    dst[src_len * 2u] = '\0';
}

static const char *adv_type_name(int adv_type)
{
    switch (adv_type) {
    case 0x00: return "adv_ind";
    case 0x01: return "adv_direct_ind";
    case 0x02: return "adv_scan_ind";
    case 0x03: return "adv_nonconn_ind";
    case 0x04: return "scan_rsp";
    case 0x05: return "ext_adv";
    default:   return "unknown";
    }
}

static const char *phy_name(int phy)
{
    switch (phy) {
    case 0x00: return "none";
    case 0x01: return "1m";
    case 0x02: return "2m";
    case 0x03: return "coded";
    default:   return "unknown";
    }
}

static const char *ad_type_name(uint8_t ad_type)
{
    switch (ad_type) {
    case 0x01: return "flags";
    case 0x02: return "uuid16_incomplete";
    case 0x03: return "uuid16_complete";
    case 0x06: return "uuid128_incomplete";
    case 0x07: return "uuid128_complete";
    case 0x08: return "local_name_short";
    case 0x09: return "local_name_complete";
    case 0x0a: return "tx_power";
    case 0x16: return "service_data_16";
    case 0x20: return "service_data_32";
    case 0x21: return "service_data_128";
    case 0x19: return "appearance";
    case 0xff: return "manufacturer_data";
    default:   return "unknown";
    }
}

static void decode_text_field(char *dst, size_t dst_size, const uint8_t *src, size_t src_len)
{
    size_t n = src_len < (dst_size - 1u) ? src_len : (dst_size - 1u);

    for (size_t i = 0; i < n; i++) {
        dst[i] = isprint(src[i]) ? (char)src[i] : '.';
    }
    dst[n] = '\0';
}

static void report_parse_payload_summary(discovery_report_t *report)
{
    uint8_t payload[DISCOVERY_HEX_MAX / 2];
    size_t payload_len = hex_to_bytes(report->data_hex, payload, sizeof(payload));
    size_t off = 0;

    if (payload_len == 0 && report->data_len != 0) {
        report->payload_parse_error = true;
        return;
    }

    while (off < payload_len) {
        uint8_t field_len = payload[off];
        if (field_len == 0) {
            break;
        }
        if ((off + 1u + field_len) > payload_len) {
            report->payload_parse_error = true;
            break;
        }

        uint8_t ad_type = payload[off + 1u];
        const uint8_t *field = &payload[off + 2u];
        size_t field_data_len = field_len - 1u;

        switch (ad_type) {
        case 0x01:
            if (field_data_len >= 1u) {
                report->has_flags = true;
                report->flags = field[0];
            }
            break;
        case 0x08:
        case 0x09:
            if (report->local_name[0] == '\0' || ad_type == 0x09) {
                decode_text_field(report->local_name, sizeof(report->local_name),
                                  field, field_data_len);
                report->local_name_complete = (ad_type == 0x09);
            }
            break;
        case 0x02:
        case 0x03:
            for (size_t i = 0; i + 1u < field_data_len; i += 2u) {
                uint16_t uuid16 = (uint16_t)(field[i] | (field[i + 1u] << 8));
                if (uuid16 == 0x1852u) {
                    report->has_broadcast_audio_announcement = true;
                }
            }
            break;
        case 0x16:
            if (field_data_len >= 2u) {
                uint16_t uuid16 = (uint16_t)(field[0] | (field[1u] << 8));
                report->has_service_data_uuid16 = true;
                report->service_data_uuid16 = uuid16;
                if (uuid16 == 0x1852u) {
                    report->has_broadcast_audio_announcement = true;
                }
            }
            break;
        case 0xff:
            if (field_data_len >= 2u) {
                report->has_manufacturer_id = true;
                report->manufacturer_id = (uint16_t)(field[0] | (field[1u] << 8));
            }
            break;
        default:
            break;
        }

        off += 1u + field_len;
    }
}

static void add_report_common_json(cJSON *obj, const discovery_report_t *report)
{
    cJSON_AddNumberToObject(obj, "seq", (double)report->seq);
    cJSON_AddNumberToObject(obj, "received_us", (double)report->received_us);
    cJSON_AddNumberToObject(obj, "source_ts_ms", (double)report->source_ts_ms);
    cJSON_AddStringToObject(obj, "addr", report->addr);
    cJSON_AddStringToObject(obj, "addr_type", report->addr_type);
    cJSON_AddNumberToObject(obj, "rssi", report->rssi);
    cJSON_AddNumberToObject(obj, "tx_power", report->tx_power);
    cJSON_AddNumberToObject(obj, "adv_type", report->adv_type);
    cJSON_AddStringToObject(obj, "adv_type_name", adv_type_name(report->adv_type));
    cJSON_AddNumberToObject(obj, "adv_props", report->adv_props);
    cJSON_AddNumberToObject(obj, "sid", report->sid);
    cJSON_AddNumberToObject(obj, "interval", report->interval);
    cJSON_AddNumberToObject(obj, "primary_phy", report->primary_phy);
    cJSON_AddStringToObject(obj, "primary_phy_name", phy_name(report->primary_phy));
    cJSON_AddNumberToObject(obj, "secondary_phy", report->secondary_phy);
    cJSON_AddStringToObject(obj, "secondary_phy_name", phy_name(report->secondary_phy));
    cJSON_AddNumberToObject(obj, "data_len", report->data_len);
    cJSON_AddStringToObject(obj, "data_hex", report->data_hex);

    if (report->has_flags) {
        cJSON_AddNumberToObject(obj, "flags", report->flags);
    }
    if (report->local_name[0] != '\0') {
        cJSON_AddStringToObject(obj, "local_name", report->local_name);
        cJSON_AddBoolToObject(obj, "local_name_complete", report->local_name_complete);
    }
    if (report->has_manufacturer_id) {
        cJSON_AddNumberToObject(obj, "manufacturer_id", report->manufacturer_id);
    }
    if (report->has_service_data_uuid16) {
        cJSON_AddNumberToObject(obj, "service_data_uuid16", report->service_data_uuid16);
    }
    if (report->has_broadcast_audio_announcement) {
        cJSON_AddBoolToObject(obj, "broadcast_audio_announcement", true);
    }
    if (report->payload_parse_error) {
        cJSON_AddBoolToObject(obj, "payload_parse_error", true);
    }
}

static void add_ad_structures_json(cJSON *arr, const discovery_report_t *report)
{
    uint8_t payload[DISCOVERY_HEX_MAX / 2];
    size_t payload_len = hex_to_bytes(report->data_hex, payload, sizeof(payload));
    size_t off = 0;

    if (payload_len == 0 && report->data_len != 0) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "parse_error", "invalid_hex");
        cJSON_AddItemToArray(arr, item);
        return;
    }

    while (off < payload_len) {
        uint8_t field_len = payload[off];
        if (field_len == 0) {
            break;
        }
        if ((off + 1u + field_len) > payload_len) {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "parse_error", "truncated_field");
            cJSON_AddNumberToObject(item, "offset", (double)off);
            cJSON_AddItemToArray(arr, item);
            break;
        }

        uint8_t ad_type = payload[off + 1u];
        const uint8_t *field = &payload[off + 2u];
        size_t field_data_len = field_len - 1u;
        char field_hex[(DISCOVERY_HEX_MAX + 1)];
        cJSON *item = cJSON_CreateObject();

        bytes_to_hex(field, field_data_len, field_hex, sizeof(field_hex));
        cJSON_AddNumberToObject(item, "offset", (double)off);
        cJSON_AddNumberToObject(item, "length", field_len);
        cJSON_AddNumberToObject(item, "type", ad_type);
        cJSON_AddStringToObject(item, "type_name", ad_type_name(ad_type));
        cJSON_AddStringToObject(item, "data_hex", field_hex);

        switch (ad_type) {
        case 0x01:
            if (field_data_len >= 1u) {
                cJSON_AddNumberToObject(item, "flags", field[0]);
            }
            break;
        case 0x08:
        case 0x09: {
            char text[DISCOVERY_NAME_MAX];
            decode_text_field(text, sizeof(text), field, field_data_len);
            cJSON_AddStringToObject(item, "text", text);
            cJSON_AddBoolToObject(item, "complete", ad_type == 0x09);
            break;
        }
        case 0x16:
            if (field_data_len >= 2u) {
                uint16_t uuid16 = (uint16_t)(field[0] | (field[1u] << 8));
                cJSON_AddNumberToObject(item, "uuid16", uuid16);
                cJSON_AddBoolToObject(item, "broadcast_audio_announcement", uuid16 == 0x1852u);
            }
            break;
        case 0xff:
            if (field_data_len >= 2u) {
                uint16_t company_id = (uint16_t)(field[0] | (field[1u] << 8));
                cJSON_AddNumberToObject(item, "company_id", company_id);
            }
            break;
        default:
            break;
        }

        cJSON_AddItemToArray(arr, item);
        off += 1u + field_len;
    }
}

static int find_device_slot(const char *addr)
{
    for (int i = 0; i < DEVICE_TABLE_SIZE; i++) {
        if (s_devices[i].in_use && strcmp(s_devices[i].addr, addr) == 0) {
            return i;
        }
    }
    return -1;
}

static int alloc_device_slot(void)
{
    int oldest_idx = 0;
    int64_t oldest_seen = INT64_MAX;

    for (int i = 0; i < DEVICE_TABLE_SIZE; i++) {
        if (!s_devices[i].in_use) {
            return i;
        }
        if (s_devices[i].last_seen_us < oldest_seen) {
            oldest_seen = s_devices[i].last_seen_us;
            oldest_idx = i;
        }
    }

    return oldest_idx;
}

static void update_device_from_report(const discovery_report_t *report)
{
    int idx = find_device_slot(report->addr);
    discovery_device_t *dev;

    if (idx < 0) {
        idx = alloc_device_slot();
        memset(&s_devices[idx], 0, sizeof(s_devices[idx]));
        s_devices[idx].in_use = true;
        copy_str(s_devices[idx].addr, sizeof(s_devices[idx].addr), report->addr);
        copy_str(s_devices[idx].addr_type, sizeof(s_devices[idx].addr_type), report->addr_type);
        s_devices[idx].first_seen_us = report->received_us;
        s_devices[idx].strongest_rssi = report->rssi;
    }

    dev = &s_devices[idx];
    dev->seen_count++;
    dev->last_seen_us = report->received_us;
    dev->last_rssi = report->rssi;
    if (report->rssi > dev->strongest_rssi) {
        dev->strongest_rssi = report->rssi;
    }
    dev->adv_type = report->adv_type;
    dev->adv_props = report->adv_props;
    dev->interval = report->interval;
    dev->sid = report->sid;
    dev->primary_phy = report->primary_phy;
    dev->secondary_phy = report->secondary_phy;
    dev->data_len = report->data_len;
    copy_str(dev->data_hex, sizeof(dev->data_hex), report->data_hex);

    if (report->local_name[0] != '\0' && (!dev->local_name_complete || report->local_name_complete)) {
        copy_str(dev->local_name, sizeof(dev->local_name), report->local_name);
        dev->local_name_complete = report->local_name_complete;
    }
    if (report->has_manufacturer_id) {
        dev->has_manufacturer_id = true;
        dev->manufacturer_id = report->manufacturer_id;
    }
    if (report->has_service_data_uuid16) {
        dev->has_service_data_uuid16 = true;
        dev->service_data_uuid16 = report->service_data_uuid16;
    }
    if (report->has_broadcast_audio_announcement) {
        dev->has_broadcast_audio_announcement = true;
    }
    if (report->payload_parse_error) {
        dev->payload_parse_error = true;
    }
}

static void store_report(const discovery_report_t *report)
{
    size_t idx = (size_t)(s_next_seq % REPORT_RING_SIZE);

    s_reports[idx] = *report;
    s_reports[idx].seq = ++s_next_seq;
    s_total_reports++;

    update_device_from_report(&s_reports[idx]);
}

esp_err_t discovery_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        ESP_LOGE(TAG, "Failed to create discovery mutex");
        return ESP_ERR_NO_MEM;
    }

    memset(s_reports, 0, sizeof(s_reports));
    memset(s_devices, 0, sizeof(s_devices));
    s_next_seq = 0;
    s_total_reports = 0;

    ESP_LOGI(TAG, "Discovery initialized (reports=%d devices=%d)",
             REPORT_RING_SIZE, DEVICE_TABLE_SIZE);
    return ESP_OK;
}

esp_err_t discovery_ingest_adv_json(const cJSON *root)
{
    discovery_report_t report = {0};
    int value = 0;

    if (!root) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!copy_json_str(report.addr, sizeof(report.addr), root, "addr")) {
        return ESP_ERR_INVALID_ARG;
    }

    (void)copy_json_str(report.addr_type, sizeof(report.addr_type), root, "addr_type");
    if (get_json_int(root, "ts", &value)) {
        report.source_ts_ms = (uint32_t)value;
    }
    if (get_json_int(root, "sid", &value)) {
        report.sid = value;
    }
    if (get_json_int(root, "rssi", &value)) {
        report.rssi = value;
    }
    if (get_json_int(root, "tx_power", &value)) {
        report.tx_power = value;
    }
    if (get_json_int(root, "adv_type", &value)) {
        report.adv_type = value;
    }
    if (get_json_int(root, "adv_props", &value)) {
        report.adv_props = value;
    }
    if (get_json_int(root, "interval", &value)) {
        report.interval = value;
    }
    if (get_json_int(root, "primary_phy", &value)) {
        report.primary_phy = value;
    }
    if (get_json_int(root, "secondary_phy", &value)) {
        report.secondary_phy = value;
    }
    if (get_json_int(root, "data_len", &value)) {
        report.data_len = value;
    }

    if (!copy_json_str(report.data_hex, sizeof(report.data_hex), root, "data_hex")) {
        report.data_hex[0] = '\0';
    }

    report.received_us = esp_timer_get_time();
    report_parse_payload_summary(&report);

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    store_report(&report);
    xSemaphoreGive(s_mutex);

    return ESP_OK;
}

cJSON *discovery_json_create_summary(void)
{
    cJSON *root = cJSON_CreateObject();
    int devices_tracked = 0;
    uint64_t latest_seq = 0;
    int64_t latest_received_us = 0;

    if (!root) {
        return NULL;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    latest_seq = s_next_seq;
    for (int i = 0; i < DEVICE_TABLE_SIZE; i++) {
        if (s_devices[i].in_use) {
            devices_tracked++;
        }
    }
    if (s_next_seq > 0) {
        size_t idx = (size_t)((s_next_seq - 1u) % REPORT_RING_SIZE);
        latest_received_us = s_reports[idx].received_us;
    }

    cJSON_AddNumberToObject(root, "total_reports", (double)s_total_reports);
    cJSON_AddNumberToObject(root, "retained_reports",
                            (double)((s_total_reports < REPORT_RING_SIZE)
                                     ? s_total_reports : REPORT_RING_SIZE));
    cJSON_AddNumberToObject(root, "devices_tracked", devices_tracked);
    cJSON_AddNumberToObject(root, "latest_seq", (double)latest_seq);
    if (latest_received_us > 0) {
        cJSON_AddNumberToObject(root, "latest_received_us", (double)latest_received_us);
    }

    xSemaphoreGive(s_mutex);
    return root;
}

cJSON *discovery_json_create_reports(size_t limit, bool include_ad_structures)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *arr;
    uint64_t latest_seq;
    size_t retained;

    if (!root) {
        return NULL;
    }

    arr = cJSON_AddArrayToObject(root, "reports");
    if (!arr) {
        cJSON_Delete(root);
        return NULL;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    latest_seq = s_next_seq;
    retained = (size_t)((s_total_reports < REPORT_RING_SIZE)
                        ? s_total_reports : REPORT_RING_SIZE);
    if (limit == 0 || limit > retained) {
        limit = retained;
    }

    cJSON_AddNumberToObject(root, "total_reports", (double)s_total_reports);
    cJSON_AddNumberToObject(root, "retained_reports", (double)retained);
    cJSON_AddNumberToObject(root, "latest_seq", (double)latest_seq);

    for (size_t i = 0; i < limit; i++) {
        uint64_t seq = latest_seq - i;
        size_t idx = (size_t)((seq - 1u) % REPORT_RING_SIZE);
        const discovery_report_t *report = &s_reports[idx];
        cJSON *obj;

        if (report->seq != seq) {
            continue;
        }

        obj = cJSON_CreateObject();
        add_report_common_json(obj, report);
        if (include_ad_structures) {
            cJSON *ad_arr = cJSON_AddArrayToObject(obj, "ad_structures");
            add_ad_structures_json(ad_arr, report);
        }
        cJSON_AddItemToArray(arr, obj);
    }

    xSemaphoreGive(s_mutex);
    return root;
}

cJSON *discovery_json_create_devices(size_t limit)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *arr;
    bool emitted[DEVICE_TABLE_SIZE] = {0};
    size_t count = 0;

    if (!root) {
        return NULL;
    }

    arr = cJSON_AddArrayToObject(root, "devices");
    if (!arr) {
        cJSON_Delete(root);
        return NULL;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    for (int i = 0; i < DEVICE_TABLE_SIZE; i++) {
        if (s_devices[i].in_use) {
            count++;
        }
    }
    if (limit == 0 || limit > count) {
        limit = count;
    }

    cJSON_AddNumberToObject(root, "devices_tracked", (double)count);

    for (size_t i = 0; i < limit; i++) {
        int best_idx = -1;
        cJSON *obj = cJSON_CreateObject();

        for (int j = 0; j < DEVICE_TABLE_SIZE; j++) {
            if (!s_devices[j].in_use || emitted[j]) {
                continue;
            }
            if (best_idx < 0 || s_devices[j].last_seen_us > s_devices[best_idx].last_seen_us) {
                best_idx = j;
            }
        }
        if (best_idx < 0) {
            cJSON_Delete(obj);
            break;
        }

        emitted[best_idx] = true;

        cJSON_AddStringToObject(obj, "addr", s_devices[best_idx].addr);
        cJSON_AddStringToObject(obj, "addr_type", s_devices[best_idx].addr_type);
        cJSON_AddNumberToObject(obj, "seen_count", (double)s_devices[best_idx].seen_count);
        cJSON_AddNumberToObject(obj, "first_seen_us", (double)s_devices[best_idx].first_seen_us);
        cJSON_AddNumberToObject(obj, "last_seen_us", (double)s_devices[best_idx].last_seen_us);
        cJSON_AddNumberToObject(obj, "last_rssi", s_devices[best_idx].last_rssi);
        cJSON_AddNumberToObject(obj, "strongest_rssi", s_devices[best_idx].strongest_rssi);
        cJSON_AddNumberToObject(obj, "adv_type", s_devices[best_idx].adv_type);
        cJSON_AddStringToObject(obj, "adv_type_name", adv_type_name(s_devices[best_idx].adv_type));
        cJSON_AddNumberToObject(obj, "adv_props", s_devices[best_idx].adv_props);
        cJSON_AddNumberToObject(obj, "sid", s_devices[best_idx].sid);
        cJSON_AddNumberToObject(obj, "interval", s_devices[best_idx].interval);
        cJSON_AddNumberToObject(obj, "primary_phy", s_devices[best_idx].primary_phy);
        cJSON_AddStringToObject(obj, "primary_phy_name", phy_name(s_devices[best_idx].primary_phy));
        cJSON_AddNumberToObject(obj, "secondary_phy", s_devices[best_idx].secondary_phy);
        cJSON_AddStringToObject(obj, "secondary_phy_name", phy_name(s_devices[best_idx].secondary_phy));
        cJSON_AddNumberToObject(obj, "data_len", s_devices[best_idx].data_len);
        cJSON_AddStringToObject(obj, "data_hex", s_devices[best_idx].data_hex);

        if (s_devices[best_idx].local_name[0] != '\0') {
            cJSON_AddStringToObject(obj, "local_name", s_devices[best_idx].local_name);
            cJSON_AddBoolToObject(obj, "local_name_complete",
                                  s_devices[best_idx].local_name_complete);
        }
        if (s_devices[best_idx].has_manufacturer_id) {
            cJSON_AddNumberToObject(obj, "manufacturer_id",
                                    s_devices[best_idx].manufacturer_id);
        }
        if (s_devices[best_idx].has_service_data_uuid16) {
            cJSON_AddNumberToObject(obj, "service_data_uuid16",
                                    s_devices[best_idx].service_data_uuid16);
        }
        if (s_devices[best_idx].has_broadcast_audio_announcement) {
            cJSON_AddBoolToObject(obj, "broadcast_audio_announcement", true);
        }
        if (s_devices[best_idx].payload_parse_error) {
            cJSON_AddBoolToObject(obj, "payload_parse_error", true);
        }

        cJSON_AddItemToArray(arr, obj);
    }

    xSemaphoreGive(s_mutex);
    return root;
}
