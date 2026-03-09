#include "http_server.h"
#include "discovery.h"
#include "uart_proto.h"
#include "device_proto.h"
#include "wifi_manager.h"
#include "status.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "http_srv";

static httpd_handle_t s_httpd;

/* ── CORS helper ──────────────────────────────────────────────── */

static void set_cors_headers(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
}

static esp_err_t send_json_root(httpd_req_t *req, cJSON *root)
{
    char *json_str;
    esp_err_t err;

    if (!root) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON allocation failed");
        return ESP_ERR_NO_MEM;
    }

    json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json_str) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON render failed");
        return ESP_ERR_NO_MEM;
    }

    httpd_resp_set_type(req, "application/json");
    err = httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
    free(json_str);
    return err;
}

static size_t get_limit_query_param(httpd_req_t *req, size_t default_limit, size_t max_limit)
{
    char query[64];
    char value[16];
    int query_len = httpd_req_get_url_query_len(req);

    if (query_len <= 0 || query_len >= (int)sizeof(query)) {
        return default_limit;
    }
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        return default_limit;
    }
    if (httpd_query_key_value(query, "limit", value, sizeof(value)) != ESP_OK) {
        return default_limit;
    }

    char *endptr = NULL;
    long parsed = strtol(value, &endptr, 10);
    if (!endptr || *endptr != '\0' || parsed <= 0) {
        return default_limit;
    }
    if ((size_t)parsed > max_limit) {
        return max_limit;
    }
    return (size_t)parsed;
}

/* ── GET /api/status ──────────────────────────────────────────── */

static esp_err_t handle_status(httpd_req_t *req)
{
    set_cors_headers(req);

    auracle_status_t st;
    status_get(&st);

    int64_t now_us   = esp_timer_get_time();
    int64_t uptime_s = (now_us - st.boot_time_us) / 1000000;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "uptime_seconds", (double)uptime_s);

    /* Attached device */
    const DeviceInfo *dev = attached_device_get_info();
    cJSON *device = cJSON_AddObjectToObject(root, "device");
    cJSON_AddBoolToObject(device, "known", attached_device_is_known());
    if (dev->ready_seen) {
        cJSON_AddBoolToObject(device, "ready_seen", true);
    }
    if (dev->info_ok) {
        cJSON_AddStringToObject(device, "name",    dev->name);
        cJSON_AddStringToObject(device, "version", dev->version);
    }
    if (dev->capabilities_ok) {
        cJSON *caps = cJSON_AddArrayToObject(device, "capabilities");
        for (int i = 0; i < dev->capability_count; i++) {
            cJSON_AddItemToArray(caps, cJSON_CreateString(dev->capabilities[i]));
        }
    }

    cJSON *discovery = discovery_json_create_summary();
    if (discovery) {
        cJSON_AddItemToObject(root, "discovery", discovery);
    }

    /* WiFi */
    cJSON *wifi = cJSON_AddObjectToObject(root, "wifi");
    const char *wifi_state = "unknown";
    switch (st.wifi_state) {
    case WIFI_STATE_DISCONNECTED:  wifi_state = "disconnected"; break;
    case WIFI_STATE_CONNECTING:    wifi_state = "connecting";   break;
    case WIFI_STATE_CONNECTED:     wifi_state = "connected";    break;
    case WIFI_STATE_PROVISIONING:  wifi_state = "provisioning"; break;
    }
    cJSON_AddStringToObject(wifi, "state",   wifi_state);
    cJSON_AddStringToObject(wifi, "ssid",    st.wifi_ssid);
    cJSON_AddStringToObject(wifi, "ip",      st.wifi_ip);
    cJSON_AddNumberToObject(wifi, "rssi",    st.wifi_rssi);
    cJSON_AddNumberToObject(wifi, "channel", st.wifi_channel);

    return send_json_root(req, root);
}

/* ── POST /api/command ────────────────────────────────────────── */

/*
 * Forward a raw JSON Lines command to the device over UART.
 * Body must be a compact JSON object, max 1023 bytes (protocol line limit
 * minus the appended newline).
 * Returns immediately — the device response is dispatched asynchronously
 * by attached_device_process() and is not returned here.
 */
static esp_err_t handle_command(httpd_req_t *req)
{
    set_cors_headers(req);

    char body[1023] = {0};
    int received = httpd_req_recv(req, body, sizeof(body) - 1);
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return ESP_FAIL;
    }
    body[received] = '\0';

    cJSON *parsed = cJSON_Parse(body);
    if (!parsed) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Body is not valid JSON");
        return ESP_FAIL;
    }
    if (!cJSON_IsObject(parsed)) {
        cJSON_Delete(parsed);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Body must be a JSON object");
        return ESP_FAIL;
    }
    cJSON_Delete(parsed);

    esp_err_t err = uart_proto_send_json_line(body);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "UART send failed");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"queued\"}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* ── POST /api/device/discover ────────────────────────────────── */

static esp_err_t handle_device_discover(httpd_req_t *req)
{
    set_cors_headers(req);
    attached_device_start_discovery();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"discovery_started\"}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* ── GET /api/events ──────────────────────────────────────────── */

/*
 * TODO: Implement as an SSE stream of raw JSON lines received from the
 * device. Requires a ring buffer or event queue in device_proto that SSE
 * clients can tail without dequeuing messages for other subscribers.
 */
static esp_err_t handle_events(httpd_req_t *req)
{
    set_cors_headers(req);
    httpd_resp_set_status(req, "501 Not Implemented");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req,
        "{\"error\":\"events stream not yet implemented\","
        "\"hint\":\"poll GET /api/status or POST /api/command for now\"}",
        HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* ── GET /api/wifi/status ─────────────────────────────────────── */

static esp_err_t handle_wifi_status(httpd_req_t *req)
{
    set_cors_headers(req);

    auracle_status_t st;
    status_get(&st);

    const char *state_str = "unknown";
    switch (st.wifi_state) {
    case WIFI_STATE_DISCONNECTED:  state_str = "disconnected"; break;
    case WIFI_STATE_CONNECTING:    state_str = "connecting";   break;
    case WIFI_STATE_CONNECTED:     state_str = "connected";    break;
    case WIFI_STATE_PROVISIONING:  state_str = "provisioning"; break;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "state",   state_str);
    cJSON_AddStringToObject(root, "ssid",    st.wifi_ssid);
    cJSON_AddStringToObject(root, "ip",      st.wifi_ip);
    cJSON_AddNumberToObject(root, "rssi",    st.wifi_rssi);
    cJSON_AddNumberToObject(root, "channel", st.wifi_channel);

    return send_json_root(req, root);
}

/* ── GET /api/discovery/... ───────────────────────────────────── */

static esp_err_t handle_discovery_summary(httpd_req_t *req)
{
    set_cors_headers(req);
    return send_json_root(req, discovery_json_create_summary());
}

static esp_err_t handle_discovery_reports(httpd_req_t *req)
{
    size_t limit = get_limit_query_param(req, 20, CONFIG_AURACLE_MSG_RING_SIZE);
    set_cors_headers(req);
    return send_json_root(req, discovery_json_create_reports(limit, true));
}

static esp_err_t handle_discovery_devices(httpd_req_t *req)
{
    size_t limit = get_limit_query_param(req, 32, 128);
    set_cors_headers(req);
    return send_json_root(req, discovery_json_create_devices(limit));
}

/* ── POST /api/wifi/configure ─────────────────────────────────── */

static esp_err_t handle_wifi_configure(httpd_req_t *req)
{
    set_cors_headers(req);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req,
        "{\"status\":\"provisioning\",\"message\":"
        "\"Connect to the Naim-Auracle access point to configure WiFi.\"}",
        HTTPD_RESP_USE_STRLEN);
    ESP_LOGW(TAG, "WiFi re-provisioning requested via API");
    return ESP_OK;
}

/* ── OPTIONS preflight ────────────────────────────────────────── */

static esp_err_t handle_options(httpd_req_t *req)
{
    set_cors_headers(req);
    httpd_resp_set_hdr(req, "Access-Control-Max-Age", "86400");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

/* ── Server start/stop ────────────────────────────────────────── */

esp_err_t http_server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;
    config.max_open_sockets = CONFIG_AURACLE_HTTP_MAX_SSE_CLIENTS + 4;
    config.lru_purge_enable = true;
    config.server_port      = CONFIG_AURACLE_HTTP_PORT;
    config.stack_size       = 6144;

    esp_err_t err = httpd_start(&s_httpd, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
        return err;
    }

    const httpd_uri_t uris[] = {
        { "/api/status",          HTTP_GET,     handle_status,          NULL },
        { "/api/discovery/summary", HTTP_GET,   handle_discovery_summary, NULL },
        { "/api/discovery/reports", HTTP_GET,   handle_discovery_reports, NULL },
        { "/api/discovery/devices", HTTP_GET,   handle_discovery_devices, NULL },
        { "/api/command",         HTTP_POST,    handle_command,         NULL },
        { "/api/events",          HTTP_GET,     handle_events,          NULL },
        { "/api/device/discover", HTTP_POST,    handle_device_discover, NULL },
        { "/api/wifi/status",     HTTP_GET,     handle_wifi_status,     NULL },
        { "/api/wifi/configure",  HTTP_POST,    handle_wifi_configure,  NULL },
        { "/api/*",               HTTP_OPTIONS, handle_options,         NULL },
    };

    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        httpd_register_uri_handler(s_httpd, &uris[i]);
    }

    ESP_LOGI(TAG, "HTTP server started on port %d", CONFIG_AURACLE_HTTP_PORT);
    return ESP_OK;
}

esp_err_t http_server_stop(void)
{
    if (s_httpd) {
        httpd_stop(s_httpd);
        s_httpd = NULL;
        ESP_LOGI(TAG, "HTTP server stopped");
    }
    return ESP_OK;
}
