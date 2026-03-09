#include "wire_proto.h"
#include "wire_uart.h"
#include "status.h"

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "wire_proto";

#define RING_SIZE CONFIG_AURACLE_MSG_RING_SIZE

/* Ring buffer of parsed messages */
static wire_msg_t s_ring[RING_SIZE];
static size_t     s_write_seq;  /* Next sequence number to write (1-based) */
static SemaphoreHandle_t s_ring_mutex;

/* Determine message type from the raw JSON line using fast string search */
static wire_msg_type_t classify_line(const char *line)
{
    /* Look for "type":"<value>" pattern */
    const char *type_key = strstr(line, "\"type\"");
    if (!type_key) {
        return WIRE_MSG_UNKNOWN;
    }

    /* Find the value after the key */
    const char *colon = strchr(type_key + 6, ':');
    if (!colon) {
        return WIRE_MSG_UNKNOWN;
    }

    /* Skip whitespace and opening quote */
    const char *val = colon + 1;
    while (*val == ' ' || *val == '\t') val++;
    if (*val == '"') val++;

    if (strncmp(val, "hello", 5) == 0)  return WIRE_MSG_HELLO;
    if (strncmp(val, "adv", 3) == 0)    return WIRE_MSG_ADV;
    if (strncmp(val, "sync", 4) == 0)   return WIRE_MSG_SYNC;
    if (strncmp(val, "bis", 3) == 0)    return WIRE_MSG_BIS;
    if (strncmp(val, "log", 3) == 0)    return WIRE_MSG_LOG;

    return WIRE_MSG_UNKNOWN;
}

/* Parse hello message and update shared status */
static void handle_hello(const char *line)
{
    cJSON *root = cJSON_Parse(line);
    if (!root) {
        ESP_LOGW(TAG, "Failed to parse hello JSON");
        return;
    }

    const cJSON *firmware = cJSON_GetObjectItem(root, "firmware");
    const cJSON *version  = cJSON_GetObjectItem(root, "version");
    const cJSON *role     = cJSON_GetObjectItem(root, "role");

    status_set_nrf_hello(
        cJSON_IsString(firmware) ? firmware->valuestring : "unknown",
        cJSON_IsString(version)  ? version->valuestring  : "unknown",
        cJSON_IsString(role)     ? role->valuestring     : "unknown"
    );

    cJSON_Delete(root);
}

esp_err_t wire_proto_init(void)
{
    s_ring_mutex = xSemaphoreCreateMutex();
    if (!s_ring_mutex) {
        ESP_LOGE(TAG, "Failed to create ring buffer mutex");
        return ESP_ERR_NO_MEM;
    }

    memset(s_ring, 0, sizeof(s_ring));
    s_write_seq = 0;

    ESP_LOGI(TAG, "Wire protocol initialized (ring size=%d)", RING_SIZE);
    return ESP_OK;
}

void wire_proto_feed_line(const char *line)
{
    if (!line || line[0] == '\0') {
        return;
    }

    wire_msg_type_t type = classify_line(line);

    /* Handle hello specially — update shared status */
    if (type == WIRE_MSG_HELLO) {
        handle_hello(line);
    }

    /* Allocate a copy of the JSON line */
    size_t len = strlen(line);
    char *json_copy = malloc(len + 1);
    if (!json_copy) {
        ESP_LOGW(TAG, "Failed to allocate message copy (%zu bytes)", len + 1);
        return;
    }
    memcpy(json_copy, line, len + 1);

    xSemaphoreTake(s_ring_mutex, portMAX_DELAY);

    /* Calculate ring index */
    size_t idx = s_write_seq % RING_SIZE;

    /* Free previous entry at this slot if it exists */
    if (s_ring[idx].json_str) {
        free(s_ring[idx].json_str);
    }

    /* Write new entry */
    s_ring[idx].type        = type;
    s_ring[idx].json_str    = json_copy;
    s_ring[idx].received_us = esp_timer_get_time();
    s_ring[idx].seq         = s_write_seq + 1;  /* 1-based sequence */

    s_write_seq++;

    xSemaphoreGive(s_ring_mutex);
}

size_t wire_proto_get_latest_seq(void)
{
    xSemaphoreTake(s_ring_mutex, portMAX_DELAY);
    size_t seq = s_write_seq;  /* s_write_seq is the count, latest seq = s_write_seq */
    xSemaphoreGive(s_ring_mutex);
    return seq;
}

const wire_msg_t *wire_proto_get_msg(size_t seq)
{
    if (seq == 0) {
        return NULL;
    }

    xSemaphoreTake(s_ring_mutex, portMAX_DELAY);

    /* Check if seq is still in the ring */
    size_t oldest_seq = (s_write_seq > RING_SIZE) ? (s_write_seq - RING_SIZE + 1) : 1;
    if (seq < oldest_seq || seq > s_write_seq) {
        xSemaphoreGive(s_ring_mutex);
        return NULL;
    }

    size_t idx = (seq - 1) % RING_SIZE;
    const wire_msg_t *msg = &s_ring[idx];

    /* Verify the slot contains the expected sequence */
    if (msg->seq != seq || !msg->json_str) {
        xSemaphoreGive(s_ring_mutex);
        return NULL;
    }

    xSemaphoreGive(s_ring_mutex);
    return msg;
}

esp_err_t wire_proto_send_raw(const char *json_cmd)
{
    if (!json_cmd) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t len = strlen(json_cmd);
    /* Send the JSON followed by newline */
    esp_err_t err = wire_uart_send(json_cmd, len);
    if (err != ESP_OK) {
        return err;
    }
    return wire_uart_send("\n", 1);
}

esp_err_t wire_proto_send_tap(const char *tap_name, bool enable)
{
    char buf[128];
    int n = snprintf(buf, sizeof(buf),
                     "{\"type\":\"cmd\",\"action\":\"%s\",\"tap\":\"%s\"}",
                     enable ? "tap_enable" : "tap_disable",
                     tap_name);
    if (n < 0 || n >= (int)sizeof(buf)) {
        return ESP_ERR_INVALID_SIZE;
    }
    return wire_proto_send_raw(buf);
}

esp_err_t wire_proto_send_scan(bool start)
{
    char buf[64];
    int n = snprintf(buf, sizeof(buf),
                     "{\"type\":\"cmd\",\"action\":\"%s\"}",
                     start ? "scan_start" : "scan_stop");
    if (n < 0 || n >= (int)sizeof(buf)) {
        return ESP_ERR_INVALID_SIZE;
    }
    return wire_proto_send_raw(buf);
}
