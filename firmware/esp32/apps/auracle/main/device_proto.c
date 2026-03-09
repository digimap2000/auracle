#include "device_proto.h"
#include "uart_proto.h"

#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"
#include "esp_log.h"

static const char *TAG = "device_proto";

/* ── Configuration ────────────────────────────────────────────── */

#define REQUEST_TIMEOUT_MS  1000u

/* ── Private types ────────────────────────────────────────────── */

/*
 * Startup discovery state machine.
 *
 *   IDLE ──► WAITING_INFO ──► WAITING_CAPS ──► DONE
 *                │                  │
 *                └──────────────────┴──► FAILED  (timeout or ok=false)
 */
typedef enum {
    STARTUP_IDLE,
    STARTUP_WAITING_INFO,
    STARTUP_WAITING_CAPS,
    STARTUP_DONE,
    STARTUP_FAILED,
} startup_state_t;

/*
 * Tracks the one in-flight request. Only one request may be pending at a
 * time. Extend to an array if concurrent requests become necessary.
 */
typedef struct {
    int      id;                    /* Request ID echoed in the response */
    char     cmd[DEVICE_STR_MAX];   /* Command name, for logging + dispatch */
    uint32_t sent_at_ms;            /* Timestamp for timeout detection */
    bool     in_use;
} PendingRequest;

/* ── Module state ─────────────────────────────────────────────── */

static DeviceInfo     s_info;
static PendingRequest s_pending;
static int            s_next_id;
static startup_state_t s_startup_state = STARTUP_IDLE;

/* ── Helpers ──────────────────────────────────────────────────── */

static inline uint32_t now_ms(void)
{
    return (uint32_t)((uint64_t)xTaskGetTickCount() * portTICK_PERIOD_MS);
}

/*
 * Copy a cJSON string field into a fixed-size destination.
 * No-op if item is absent or not a string.
 */
static void copy_str_field(char *dst, size_t dst_size, const cJSON *item)
{
    if (cJSON_IsString(item) && item->valuestring) {
        strncpy(dst, item->valuestring, dst_size - 1);
        dst[dst_size - 1] = '\0';
    }
}

/* ── Request sending ──────────────────────────────────────────── */

static esp_err_t protocol_send_request(const char *cmd)
{
    if (s_pending.in_use) {
        ESP_LOGW(TAG, "Cannot send '%s': request id=%d ('%s') still pending",
                 cmd, s_pending.id, s_pending.cmd);
        return ESP_ERR_INVALID_STATE;
    }

    s_next_id++;

    char buf[80];
    int n = snprintf(buf, sizeof(buf), "{\"id\":%d,\"cmd\":\"%s\"}", s_next_id, cmd);
    if (n < 0 || n >= (int)sizeof(buf)) {
        ESP_LOGE(TAG, "TX: command string too long for cmd='%s'", cmd);
        return ESP_ERR_INVALID_SIZE;
    }

    esp_err_t err = uart_proto_send_json_line(buf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "TX: UART send failed for cmd='%s': %s", cmd, esp_err_to_name(err));
        return err;
    }

    s_pending.id = s_next_id;
    strncpy(s_pending.cmd, cmd, sizeof(s_pending.cmd) - 1);
    s_pending.cmd[sizeof(s_pending.cmd) - 1] = '\0';
    s_pending.sent_at_ms = now_ms();
    s_pending.in_use = true;

    ESP_LOGI(TAG, "TX request  id=%d  cmd=%s", s_pending.id, cmd);
    return ESP_OK;
}

/* ── Response handlers ────────────────────────────────────────── */

static void handle_get_info_response(const cJSON *root, bool ok)
{
    if (!ok) {
        ESP_LOGE(TAG, "get_info: device returned ok=false");
        s_startup_state = STARTUP_FAILED;
        return;
    }

    /*
     * Parse flexibly — copy any identity fields that are present.
     * We do not require all fields; a device may omit any of them.
     */
    copy_str_field(s_info.device, sizeof(s_info.device),
                   cJSON_GetObjectItem(root, "device"));
    copy_str_field(s_info.model,  sizeof(s_info.model),
                   cJSON_GetObjectItem(root, "model"));
    copy_str_field(s_info.fw,     sizeof(s_info.fw),
                   cJSON_GetObjectItem(root, "fw"));

    const cJSON *proto = cJSON_GetObjectItem(root, "protocol");
    if (cJSON_IsNumber(proto)) {
        s_info.protocol = (int)proto->valuedouble;
    }

    s_info.info_ok = true;

    ESP_LOGI(TAG, "Device info  device='%s'  model='%s'  fw='%s'  protocol=%d",
             s_info.device, s_info.model, s_info.fw, s_info.protocol);

    /* Advance startup sequence: send get_capabilities */
    if (protocol_send_request("get_capabilities") == ESP_OK) {
        s_startup_state = STARTUP_WAITING_CAPS;
    } else {
        ESP_LOGE(TAG, "Failed to advance to get_capabilities");
        s_startup_state = STARTUP_FAILED;
    }
}

static void handle_get_capabilities_response(const cJSON *root, bool ok)
{
    if (!ok) {
        ESP_LOGE(TAG, "get_capabilities: device returned ok=false");
        s_startup_state = STARTUP_FAILED;
        return;
    }

    const cJSON *caps = cJSON_GetObjectItem(root, "capabilities");
    if (!cJSON_IsObject(caps)) {
        /*
         * Response was ok=true but lacks a "capabilities" object.
         * Accept it — the device may not support all optional fields.
         */
        ESP_LOGW(TAG, "get_capabilities: 'capabilities' object absent; accepting");
        s_info.capabilities_ok = true;
        s_startup_state = STARTUP_DONE;
        ESP_LOGI(TAG, "Startup discovery complete (no capability detail)");
        return;
    }

    const cJSON *has_bat = cJSON_GetObjectItem(caps, "has_battery");
    if (cJSON_IsBool(has_bat)) {
        s_info.has_battery = cJSON_IsTrue(has_bat);
    }

    const cJSON *max_vol = cJSON_GetObjectItem(caps, "max_volume");
    if (cJSON_IsNumber(max_vol)) {
        s_info.max_volume = (int)max_vol->valuedouble;
    }

    /* commands array */
    const cJSON *cmds_arr = cJSON_GetObjectItem(caps, "commands");
    if (cJSON_IsArray(cmds_arr)) {
        const cJSON *item;
        cJSON_ArrayForEach(item, cmds_arr) {
            if (!cJSON_IsString(item)) continue;
            if (s_info.command_count >= DEVICE_MAX_COMMANDS) {
                ESP_LOGW(TAG, "get_capabilities: commands list truncated at %d",
                         DEVICE_MAX_COMMANDS);
                break;
            }
            strncpy(s_info.commands[s_info.command_count], item->valuestring,
                    DEVICE_STR_MAX - 1);
            s_info.commands[s_info.command_count][DEVICE_STR_MAX - 1] = '\0';
            s_info.command_count++;
        }
    }

    /* events array */
    const cJSON *evts_arr = cJSON_GetObjectItem(caps, "events");
    if (cJSON_IsArray(evts_arr)) {
        const cJSON *item;
        cJSON_ArrayForEach(item, evts_arr) {
            if (!cJSON_IsString(item)) continue;
            if (s_info.event_count >= DEVICE_MAX_EVENTS) {
                ESP_LOGW(TAG, "get_capabilities: events list truncated at %d",
                         DEVICE_MAX_EVENTS);
                break;
            }
            strncpy(s_info.events[s_info.event_count], item->valuestring,
                    DEVICE_STR_MAX - 1);
            s_info.events[s_info.event_count][DEVICE_STR_MAX - 1] = '\0';
            s_info.event_count++;
        }
    }

    s_info.capabilities_ok = true;
    s_startup_state = STARTUP_DONE;

    ESP_LOGI(TAG, "Device capabilities  commands=%d  events=%d  max_vol=%d  battery=%s",
             s_info.command_count, s_info.event_count,
             s_info.max_volume, s_info.has_battery ? "yes" : "no");
    ESP_LOGI(TAG, "Startup discovery complete — device is known");
}

/* ── Message dispatch ─────────────────────────────────────────── */

/*
 * Handle an incoming request from the device to the ESP32.
 *
 * This path exists so the device can send commands to us (e.g., for
 * bidirectional control). For now all commands are unknown; add cases below
 * as the protocol grows.
 */
static void protocol_handle_request(const cJSON *root, int id, const char *cmd)
{
    ESP_LOGI(TAG, "RX request from device  id=%d  cmd=%s", id, cmd);

    /* TODO: add cases for future incoming commands from the device, e.g.:
     *   if (strcmp(cmd, "notify_ready") == 0) { ... }
     */

    /* Return unknown_cmd for anything we don't recognise */
    char resp[96];
    snprintf(resp, sizeof(resp),
             "{\"id\":%d,\"ok\":false,\"error\":\"unknown_cmd\"}", id);
    uart_proto_send_json_line(resp);
    ESP_LOGW(TAG, "TX unknown_cmd  id=%d  cmd=%s", id, cmd);
}

static void protocol_handle_response(const cJSON *root, int id, bool ok)
{
    if (!s_pending.in_use) {
        ESP_LOGW(TAG, "RX response id=%d but no request is pending — ignoring", id);
        return;
    }

    if (s_pending.id != id) {
        ESP_LOGW(TAG, "RX response id=%d does not match pending id=%d — ignoring",
                 id, s_pending.id);
        return;
    }

    ESP_LOGI(TAG, "RX response  id=%d  ok=%s  cmd=%s",
             id, ok ? "true" : "false", s_pending.cmd);

    /*
     * Copy the matched command name and clear in_use BEFORE calling the
     * specific handler, so the handler can issue the next request without
     * hitting the "pending request already in use" guard.
     */
    char matched_cmd[DEVICE_STR_MAX];
    strncpy(matched_cmd, s_pending.cmd, sizeof(matched_cmd) - 1);
    matched_cmd[sizeof(matched_cmd) - 1] = '\0';
    s_pending.in_use = false;

    if (strcmp(matched_cmd, "get_info") == 0) {
        handle_get_info_response(root, ok);
    } else if (strcmp(matched_cmd, "get_capabilities") == 0) {
        handle_get_capabilities_response(root, ok);
    } else {
        /* TODO: add response handlers for future commands:
         *   get_status, set_volume, set_mute, start_stream, stop_stream
         */
        ESP_LOGW(TAG, "No response handler for cmd='%s' — ignoring", matched_cmd);
    }
}

static void protocol_handle_event(const cJSON *root, const char *event_name)
{
    ESP_LOGI(TAG, "RX event: %s", event_name);

    if (strcmp(event_name, "ready") == 0) {
        s_info.ready_seen = true;

        const cJSON *dev  = cJSON_GetObjectItem(root, "device");
        const cJSON *fw   = cJSON_GetObjectItem(root, "fw");
        const cJSON *prot = cJSON_GetObjectItem(root, "protocol");

        ESP_LOGI(TAG, "  ready  device='%s'  fw='%s'  protocol=%d",
                 cJSON_IsString(dev)  ? dev->valuestring  : "?",
                 cJSON_IsString(fw)   ? fw->valuestring   : "?",
                 cJSON_IsNumber(prot) ? (int)prot->valuedouble : -1);

        /* Log it — do not treat it as a signal to delay sending get_info */
        return;
    }

    /* TODO: add handlers for device-specific events, e.g.:
     *   button, battery, stream_state
     */
    ESP_LOGW(TAG, "Unhandled event '%s' — ignoring", event_name);
}

/* ── Timeout check ────────────────────────────────────────────── */

static void check_timeout(void)
{
    if (!s_pending.in_use) {
        return;
    }

    uint32_t elapsed = now_ms() - s_pending.sent_at_ms;
    if (elapsed < REQUEST_TIMEOUT_MS) {
        return;
    }

    ESP_LOGE(TAG, "Request timeout  id=%d  cmd='%s'  elapsed=%" PRIu32 "ms",
             s_pending.id, s_pending.cmd, elapsed);
    s_pending.in_use = false;

    if (s_startup_state == STARTUP_WAITING_INFO ||
        s_startup_state == STARTUP_WAITING_CAPS) {
        s_startup_state = STARTUP_FAILED;
        ESP_LOGE(TAG, "Startup discovery failed — device did not respond in time");
    }
}

/* ── Public protocol interface ────────────────────────────────── */

void protocol_handle_line(const char *line)
{
    if (!line || line[0] == '\0') {
        return;
    }

    cJSON *root = cJSON_Parse(line);
    if (!root) {
        /*
         * Malformed JSON — log and ignore. Do not try to extract an "id"
         * because we cannot do so reliably without a valid parse.
         */
        ESP_LOGW(TAG, "RX: JSON parse failed: %.80s", line);
        return;
    }

    const cJSON *cmd_item   = cJSON_GetObjectItem(root, "cmd");
    const cJSON *id_item    = cJSON_GetObjectItem(root, "id");
    const cJSON *ok_item    = cJSON_GetObjectItem(root, "ok");
    const cJSON *event_item = cJSON_GetObjectItem(root, "event");

    if (cJSON_IsString(cmd_item)) {
        /* Incoming request from the device to the ESP32 */
        if (!cJSON_IsNumber(id_item)) {
            ESP_LOGW(TAG, "RX: request has no 'id' — ignoring");
        } else {
            protocol_handle_request(root, (int)id_item->valuedouble,
                                    cmd_item->valuestring);
        }
    } else if (cJSON_IsNumber(id_item) && cJSON_IsBool(ok_item)) {
        /* Response to one of our requests */
        protocol_handle_response(root, (int)id_item->valuedouble,
                                 cJSON_IsTrue(ok_item));
    } else if (cJSON_IsString(event_item)) {
        /* Unsolicited event from the device */
        protocol_handle_event(root, event_item->valuestring);
    } else {
        ESP_LOGW(TAG, "RX: unrecognised message shape — ignoring");
    }

    cJSON_Delete(root);
}

void protocol_query_startup_info(void)
{
    /* Reset all device state before starting */
    memset(&s_info,    0, sizeof(s_info));
    memset(&s_pending, 0, sizeof(s_pending));
    s_next_id      = 0;
    s_startup_state = STARTUP_IDLE;

    if (protocol_send_request("get_info") == ESP_OK) {
        s_startup_state = STARTUP_WAITING_INFO;
        ESP_LOGI(TAG, "Startup discovery started");
    } else {
        s_startup_state = STARTUP_FAILED;
        ESP_LOGE(TAG, "Startup discovery failed — could not send get_info");
    }
}

/* ── Application API ──────────────────────────────────────────── */

bool attached_device_is_known(void)
{
    return s_info.info_ok && s_info.capabilities_ok;
}

const DeviceInfo *attached_device_get_info(void)
{
    return &s_info;
}

void attached_device_start_discovery(void)
{
    ESP_LOGI(TAG, "Starting device discovery");
    protocol_query_startup_info();
}

void attached_device_process(void)
{
    /* Drain received lines — dispatches to protocol_handle_line() */
    uart_proto_poll_rx();

    /* Check whether the pending request has timed out */
    check_timeout();
}
