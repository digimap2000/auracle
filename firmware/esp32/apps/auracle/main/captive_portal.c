#include "captive_portal.h"

#include <string.h>
#include <stdlib.h>
#include "esp_http_server.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "portal";

static httpd_handle_t s_httpd;
static EventGroupHandle_t s_event_group;
static TaskHandle_t s_dns_task;
static bool s_dns_running;

/* Forward declaration */
esp_err_t wifi_manager_save_credentials(const char *ssid, const char *password);

/* ── Embedded HTML ────────────────────────────────────────────── */

static const char PORTAL_HTML[] =
    "<!DOCTYPE html>"
    "<html><head>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Auracle Setup</title>"
    "<style>"
    "body{font-family:system-ui,sans-serif;margin:0;padding:20px;"
    "background:#1a1a2e;color:#e0e0e0;}"
    "h1{font-size:1.4em;margin:0 0 4px;color:#fff;}"
    "p.sub{font-size:.85em;color:#888;margin:0 0 24px;}"
    ".card{background:#16213e;border-radius:12px;padding:24px;"
    "max-width:360px;margin:40px auto;}"
    "label{display:block;font-size:.85em;color:#aaa;margin:16px 0 4px;}"
    "input{width:100%;padding:10px 12px;border:1px solid #333;"
    "border-radius:8px;background:#0f3460;color:#fff;font-size:1em;"
    "box-sizing:border-box;}"
    "input:focus{outline:none;border-color:#00b894;}"
    "button{width:100%;padding:12px;margin-top:20px;border:none;"
    "border-radius:8px;background:#00b894;color:#fff;font-size:1em;"
    "font-weight:600;cursor:pointer;}"
    "button:hover{background:#00a884;}"
    ".ok{text-align:center;padding:40px 20px;}"
    ".ok h2{color:#00b894;}"
    "</style></head><body>"
    "<div class='card'>"
    "<h1>Auracle Bridge</h1>"
    "<p class='sub'>ESP32-S3 WiFi Configuration</p>"
    "<form method='POST' action='/configure'>"
    "<label for='s'>Network Name (SSID)</label>"
    "<input id='s' name='ssid' required maxlength='32' autocomplete='off'>"
    "<label for='p'>Password</label>"
    "<input id='p' name='password' type='password' maxlength='64'>"
    "<button type='submit'>Connect</button>"
    "</form></div></body></html>";

static const char PORTAL_SUCCESS_HTML[] =
    "<!DOCTYPE html>"
    "<html><head>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Auracle Setup</title>"
    "<style>"
    "body{font-family:system-ui,sans-serif;margin:0;padding:20px;"
    "background:#1a1a2e;color:#e0e0e0;}"
    ".ok{text-align:center;padding:40px 20px;max-width:360px;margin:40px auto;}"
    ".ok h2{color:#00b894;font-size:1.4em;}"
    ".ok p{color:#888;}"
    "</style></head><body>"
    "<div class='ok'>"
    "<h2>Connecting...</h2>"
    "<p>The Auracle bridge is connecting to your network. "
    "This access point will shut down shortly.</p>"
    "<p>Find the device at <b>auracle.local</b> once connected.</p>"
    "</div></body></html>";

/* ── URL-decode helper ────────────────────────────────────────── */

static void url_decode(const char *src, char *dst, size_t dst_size)
{
    size_t di = 0;
    for (size_t si = 0; src[si] && di < dst_size - 1; si++) {
        if (src[si] == '%' && src[si + 1] && src[si + 2]) {
            char hex[3] = { src[si + 1], src[si + 2], '\0' };
            dst[di++] = (char)strtol(hex, NULL, 16);
            si += 2;
        } else if (src[si] == '+') {
            dst[di++] = ' ';
        } else {
            dst[di++] = src[si];
        }
    }
    dst[di] = '\0';
}

/* Extract a form field value from URL-encoded body */
static bool extract_field(const char *body, const char *name,
                          char *out, size_t out_size)
{
    size_t name_len = strlen(name);
    const char *p = body;
    while ((p = strstr(p, name)) != NULL) {
        /* Check it's the start of a field (beginning of string or after '&') */
        if (p != body && *(p - 1) != '&') {
            p += name_len;
            continue;
        }
        if (p[name_len] != '=') {
            p += name_len;
            continue;
        }
        p += name_len + 1;  /* Skip "name=" */

        /* Find end of value */
        const char *end = strchr(p, '&');
        size_t val_len = end ? (size_t)(end - p) : strlen(p);

        /* Copy and decode */
        char *encoded = malloc(val_len + 1);
        if (!encoded) {
            return false;
        }
        memcpy(encoded, p, val_len);
        encoded[val_len] = '\0';
        url_decode(encoded, out, out_size);
        free(encoded);
        return true;
    }
    return false;
}

/* ── HTTP handlers ────────────────────────────────────────────── */

static esp_err_t handle_root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, PORTAL_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t handle_configure(httpd_req_t *req)
{
    char body[256] = {0};
    int received = httpd_req_recv(req, body, sizeof(body) - 1);
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return ESP_FAIL;
    }
    body[received] = '\0';

    char ssid[33] = {0};
    char password[65] = {0};

    if (!extract_field(body, "ssid", ssid, sizeof(ssid)) || ssid[0] == '\0') {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing SSID");
        return ESP_FAIL;
    }
    extract_field(body, "password", password, sizeof(password));

    ESP_LOGI(TAG, "Credentials received: SSID=\"%s\"", ssid);

    /* Save to NVS */
    esp_err_t err = wifi_manager_save_credentials(ssid, password);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS write failed");
        return ESP_FAIL;
    }

    /* Send success page */
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, PORTAL_SUCCESS_HTML, HTTPD_RESP_USE_STRLEN);

    /* Signal provisioning complete */
    xEventGroupSetBits(s_event_group, WIFI_PROVISIONED_BIT);
    return ESP_OK;
}

/* Captive portal detection endpoints — redirect to root */
static esp_err_t handle_captive_redirect(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    return httpd_resp_send(req, NULL, 0);
}

/* ── DNS redirect task ────────────────────────────────────────── */

/* Minimal DNS server that responds to all A queries with 192.168.4.1 */
static void dns_redirect_task(void *arg)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "DNS socket creation failed");
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(53),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "DNS bind failed");
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    /* Set receive timeout so we can check s_dns_running */
    struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    ESP_LOGI(TAG, "DNS redirect started on port 53");

    uint8_t buf[512];
    while (s_dns_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int len = recvfrom(sock, buf, sizeof(buf), 0,
                           (struct sockaddr *)&client_addr, &client_len);
        if (len < 12) {
            continue;  /* Too short for DNS header, or timeout */
        }

        /*
         * Build a minimal DNS response:
         * - Copy the query transaction ID
         * - Set response flags (0x8180 = response, recursion available)
         * - Set ANCOUNT=1
         * - Append the original question section
         * - Append an A record answer pointing to 192.168.4.1
         */
        uint8_t resp[512];
        size_t resp_len = 0;

        /* Copy header (12 bytes) */
        memcpy(resp, buf, 12);
        resp[2] = 0x81;  /* QR=1, Opcode=0, AA=1 */
        resp[3] = 0x80;  /* RA=1 */
        /* QDCOUNT stays as-is from query */
        resp[6] = 0x00; resp[7] = 0x01;  /* ANCOUNT = 1 */
        resp[8] = 0x00; resp[9] = 0x00;  /* NSCOUNT = 0 */
        resp[10] = 0x00; resp[11] = 0x00; /* ARCOUNT = 0 */
        resp_len = 12;

        /* Copy question section (skip header, find end of QNAME + QTYPE + QCLASS) */
        size_t q_start = 12;
        size_t q_pos = q_start;
        while (q_pos < (size_t)len && buf[q_pos] != 0) {
            q_pos += buf[q_pos] + 1;  /* Skip label */
        }
        if (q_pos >= (size_t)len) {
            continue;  /* Malformed */
        }
        q_pos++;  /* Skip null terminator */
        q_pos += 4;  /* Skip QTYPE (2) + QCLASS (2) */

        size_t q_len = q_pos - q_start;
        if (resp_len + q_len > sizeof(resp)) {
            continue;
        }
        memcpy(resp + resp_len, buf + q_start, q_len);
        resp_len += q_len;

        /* Append answer: pointer to QNAME + A record = 192.168.4.1 */
        uint8_t answer[] = {
            0xC0, 0x0C,             /* Name pointer to offset 12 (QNAME) */
            0x00, 0x01,             /* TYPE = A */
            0x00, 0x01,             /* CLASS = IN */
            0x00, 0x00, 0x00, 0x3C, /* TTL = 60 seconds */
            0x00, 0x04,             /* RDLENGTH = 4 */
            192, 168, 4, 1          /* RDATA = 192.168.4.1 */
        };
        if (resp_len + sizeof(answer) > sizeof(resp)) {
            continue;
        }
        memcpy(resp + resp_len, answer, sizeof(answer));
        resp_len += sizeof(answer);

        sendto(sock, resp, resp_len, 0,
               (struct sockaddr *)&client_addr, client_len);
    }

    close(sock);
    ESP_LOGI(TAG, "DNS redirect stopped");
    vTaskDelete(NULL);
}

/* ── Public API ───────────────────────────────────────────────── */

esp_err_t captive_portal_start(EventGroupHandle_t event_group)
{
    s_event_group = event_group;

    /* Start HTTP server */
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;
    config.lru_purge_enable = true;

    esp_err_t err = httpd_start(&s_httpd, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start portal HTTP server: %s", esp_err_to_name(err));
        return err;
    }

    /* Register handlers */
    const httpd_uri_t uri_root = {
        .uri = "/", .method = HTTP_GET, .handler = handle_root,
    };
    const httpd_uri_t uri_configure = {
        .uri = "/configure", .method = HTTP_POST, .handler = handle_configure,
    };
    const httpd_uri_t uri_generate_204 = {
        .uri = "/generate_204", .method = HTTP_GET, .handler = handle_captive_redirect,
    };
    const httpd_uri_t uri_hotspot = {
        .uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = handle_captive_redirect,
    };

    httpd_register_uri_handler(s_httpd, &uri_root);
    httpd_register_uri_handler(s_httpd, &uri_configure);
    httpd_register_uri_handler(s_httpd, &uri_generate_204);
    httpd_register_uri_handler(s_httpd, &uri_hotspot);

    /* Start DNS redirect */
    s_dns_running = true;
    xTaskCreate(dns_redirect_task, "dns_redir", 4096, NULL, 5, &s_dns_task);

    ESP_LOGI(TAG, "Captive portal started");
    return ESP_OK;
}

void captive_portal_stop(void)
{
    s_dns_running = false;
    /* DNS task will exit on next timeout (1 second) */

    if (s_httpd) {
        httpd_stop(s_httpd);
        s_httpd = NULL;
    }

    /* Wait for DNS task to exit */
    vTaskDelay(pdMS_TO_TICKS(1500));
    s_dns_task = NULL;

    ESP_LOGI(TAG, "Captive portal stopped");
}
