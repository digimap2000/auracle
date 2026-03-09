#include "http_server.h"
#include "wire_proto.h"
#include "wifi_manager.h"
#include "status.h"

#include <string.h>
#include <stdio.h>
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

/* ── GET /api/status ──────────────────────────────────────────── */

static esp_err_t handle_status(httpd_req_t *req)
{
    set_cors_headers(req);

    auracle_status_t st;
    status_get(&st);

    int64_t now = esp_timer_get_time();
    int64_t uptime_sec = (now - st.boot_time_us) / 1000000;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "uptime_seconds", (double)uptime_sec);

    /* nRF5340 */
    cJSON *nrf = cJSON_AddObjectToObject(root, "nrf5340");
    cJSON_AddBoolToObject(nrf, "connected", st.nrf_connected);
    if (st.nrf_connected) {
        cJSON_AddStringToObject(nrf, "firmware", st.nrf_firmware);
        cJSON_AddStringToObject(nrf, "version", st.nrf_version);
        cJSON_AddStringToObject(nrf, "role", st.nrf_role);
    }

    /* WiFi */
    cJSON *wifi = cJSON_AddObjectToObject(root, "wifi");
    const char *state_str = "unknown";
    switch (st.wifi_state) {
    case WIFI_STATE_DISCONNECTED:  state_str = "disconnected"; break;
    case WIFI_STATE_CONNECTING:    state_str = "connecting"; break;
    case WIFI_STATE_CONNECTED:     state_str = "connected"; break;
    case WIFI_STATE_PROVISIONING:  state_str = "provisioning"; break;
    }
    cJSON_AddStringToObject(wifi, "state", state_str);
    cJSON_AddStringToObject(wifi, "ssid", st.wifi_ssid);
    cJSON_AddStringToObject(wifi, "ip", st.wifi_ip);
    cJSON_AddNumberToObject(wifi, "rssi", st.wifi_rssi);
    cJSON_AddNumberToObject(wifi, "channel", st.wifi_channel);

    /* Wire protocol stats */
    cJSON *wire = cJSON_AddObjectToObject(root, "wire");
    cJSON_AddNumberToObject(wire, "messages_received", (double)wire_proto_get_latest_seq());

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
    free(json_str);
    return err;
}

/* ── POST /api/command ────────────────────────────────────────── */

static esp_err_t handle_command(httpd_req_t *req)
{
    set_cors_headers(req);

    char body[512] = {0};
    int received = httpd_req_recv(req, body, sizeof(body) - 1);
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return ESP_FAIL;
    }
    body[received] = '\0';

    /* Validate it's valid JSON */
    cJSON *parsed = cJSON_Parse(body);
    if (!parsed) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    cJSON_Delete(parsed);

    /* Forward to nRF5340 */
    esp_err_t err = wire_proto_send_raw(body);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "UART send failed");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* ── GET /api/events (SSE) ────────────────────────────────────── */

static esp_err_t handle_events(httpd_req_t *req)
{
    set_cors_headers(req);

    httpd_resp_set_type(req, "text/event-stream");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Connection", "keep-alive");

    /* Send initial comment to establish the connection */
    const char *hello = ": connected to auracle event stream\n\n";
    esp_err_t err = httpd_resp_send_chunk(req, hello, strlen(hello));
    if (err != ESP_OK) {
        return ESP_OK;  /* Client disconnected immediately */
    }

    /* Start from the current position (don't replay history) */
    size_t read_seq = wire_proto_get_latest_seq();

    ESP_LOGI(TAG, "SSE client connected, starting from seq %zu", read_seq);

    while (true) {
        size_t latest = wire_proto_get_latest_seq();

        /* Send any new messages */
        while (read_seq < latest) {
            read_seq++;
            const wire_msg_t *msg = wire_proto_get_msg(read_seq);
            if (!msg) {
                /* Message evicted from ring — skip to latest available */
                continue;
            }

            /* Format as SSE: "data: <json>\n\n" */
            char prefix[32];
            snprintf(prefix, sizeof(prefix), "data: ");
            err = httpd_resp_send_chunk(req, prefix, strlen(prefix));
            if (err != ESP_OK) goto done;

            err = httpd_resp_send_chunk(req, msg->json_str, strlen(msg->json_str));
            if (err != ESP_OK) goto done;

            err = httpd_resp_send_chunk(req, "\n\n", 2);
            if (err != ESP_OK) goto done;
        }

        /* Sleep before next poll */
        vTaskDelay(pdMS_TO_TICKS(CONFIG_AURACLE_SSE_POLL_INTERVAL_MS));
    }

done:
    ESP_LOGI(TAG, "SSE client disconnected");
    /* Send empty chunk to signal end (may fail if client already gone) */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* ── POST /api/tap/{name}/enable and /disable ─────────────────── */

static esp_err_t handle_tap(httpd_req_t *req)
{
    set_cors_headers(req);

    /* Parse tap name and action from URI: /api/tap/{name}/{action} */
    const char *uri = req->uri;
    char tap_name[16] = {0};
    bool enable = false;

    /* Skip "/api/tap/" prefix (9 chars) */
    const char *after_prefix = uri + 9;
    const char *slash = strchr(after_prefix, '/');
    if (!slash || (slash - after_prefix) >= (int)sizeof(tap_name)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid tap path");
        return ESP_FAIL;
    }
    memcpy(tap_name, after_prefix, slash - after_prefix);
    tap_name[slash - after_prefix] = '\0';

    if (strcmp(slash + 1, "enable") == 0) {
        enable = true;
    } else if (strcmp(slash + 1, "disable") == 0) {
        enable = false;
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Action must be 'enable' or 'disable'");
        return ESP_FAIL;
    }

    /* Validate tap name */
    if (strcmp(tap_name, "adv") != 0 && strcmp(tap_name, "sync") != 0 &&
        strcmp(tap_name, "bis") != 0 && strcmp(tap_name, "log") != 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                            "Unknown tap (must be: adv, sync, bis, log)");
        return ESP_FAIL;
    }

    esp_err_t err = wire_proto_send_tap(tap_name, enable);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "UART send failed");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    char resp[64];
    snprintf(resp, sizeof(resp), "{\"tap\":\"%s\",\"enabled\":%s}",
             tap_name, enable ? "true" : "false");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* ── POST /api/scan/start and /stop ───────────────────────────── */

static esp_err_t handle_scan(httpd_req_t *req)
{
    set_cors_headers(req);

    const char *uri = req->uri;
    bool start = false;

    if (strstr(uri, "/start")) {
        start = true;
    } else if (strstr(uri, "/stop")) {
        start = false;
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Must be /start or /stop");
        return ESP_FAIL;
    }

    esp_err_t err = wire_proto_send_scan(start);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "UART send failed");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    char resp[48];
    snprintf(resp, sizeof(resp), "{\"scanning\":%s}", start ? "true" : "false");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
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
    case WIFI_STATE_CONNECTING:    state_str = "connecting"; break;
    case WIFI_STATE_CONNECTED:     state_str = "connected"; break;
    case WIFI_STATE_PROVISIONING:  state_str = "provisioning"; break;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "state", state_str);
    cJSON_AddStringToObject(root, "ssid", st.wifi_ssid);
    cJSON_AddStringToObject(root, "ip", st.wifi_ip);
    cJSON_AddNumberToObject(root, "rssi", st.wifi_rssi);
    cJSON_AddNumberToObject(root, "channel", st.wifi_channel);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
    free(json_str);
    return err;
}

/* ── POST /api/wifi/configure ─────────────────────────────────── */

static esp_err_t handle_wifi_configure(httpd_req_t *req)
{
    set_cors_headers(req);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"provisioning\",\"message\":"
                    "\"Restarting in provisioning mode. Connect to the "
                    "Auracle-Setup access point to configure WiFi.\"}", HTTPD_RESP_USE_STRLEN);

    /* Trigger re-provisioning on a separate task to not block the HTTP response */
    /* For now, just log — full implementation would need a deferred task */
    ESP_LOGW(TAG, "WiFi re-provisioning requested via API (restart required)");

    return ESP_OK;
}

/* ── OPTIONS handler for CORS preflight ───────────────────────── */

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
    config.max_uri_handlers = 20;
    config.max_open_sockets = CONFIG_AURACLE_HTTP_MAX_SSE_CLIENTS + 4;
    config.lru_purge_enable = true;
    config.server_port = CONFIG_AURACLE_HTTP_PORT;
    /* Increase stack for SSE handlers that run long */
    config.stack_size = 8192;

    esp_err_t err = httpd_start(&s_httpd, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
        return err;
    }

    /* Register URI handlers */
    const httpd_uri_t uris[] = {
        { "/api/status",         HTTP_GET,  handle_status,         NULL },
        { "/api/command",        HTTP_POST, handle_command,        NULL },
        { "/api/events",         HTTP_GET,  handle_events,         NULL },
        { "/api/tap/*",          HTTP_POST, handle_tap,            NULL },
        { "/api/scan/start",     HTTP_POST, handle_scan,           NULL },
        { "/api/scan/stop",      HTTP_POST, handle_scan,           NULL },
        { "/api/wifi/status",    HTTP_GET,  handle_wifi_status,    NULL },
        { "/api/wifi/configure", HTTP_POST, handle_wifi_configure, NULL },
        /* CORS preflight */
        { "/api/*",              HTTP_OPTIONS, handle_options,     NULL },
    };

    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        httpd_register_uri_handler(s_httpd, &uris[i]);
    }

    ESP_LOGI(TAG, "HTTP server started on port %d (%d max SSE clients)",
             CONFIG_AURACLE_HTTP_PORT, CONFIG_AURACLE_HTTP_MAX_SSE_CLIENTS);
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
