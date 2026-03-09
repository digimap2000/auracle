#include "wifi_manager.h"
#include "captive_portal.h"
#include "status.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "lwip/ip4_addr.h"

static const char *TAG = "wifi_mgr";

#define NVS_NAMESPACE   "wifi"
#define NVS_KEY_SSID    "ssid"
#define NVS_KEY_PASS    "password"

/* Event group bits */
#define WIFI_CONNECTED_BIT   BIT0
#define WIFI_FAIL_BIT        BIT1
#define WIFI_PROVISIONED_BIT BIT2

static EventGroupHandle_t s_wifi_event_group;
static esp_netif_t       *s_sta_netif;
static esp_netif_t       *s_ap_netif;
static wifi_state_t       s_state = WIFI_STATE_DISCONNECTED;
static int                s_retry_count;

#define MAX_STA_RETRIES 5

/* ── NVS helpers ──────────────────────────────────────────────── */

static bool nvs_read_credentials(char *ssid, size_t ssid_len,
                                 char *password, size_t pass_len)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }

    size_t s_len = ssid_len;
    size_t p_len = pass_len;
    bool ok = (nvs_get_str(handle, NVS_KEY_SSID, ssid, &s_len) == ESP_OK &&
               nvs_get_str(handle, NVS_KEY_PASS, password, &p_len) == ESP_OK &&
               s_len > 1);  /* Must have at least 1 char SSID */

    nvs_close(handle);
    return ok;
}

esp_err_t wifi_manager_save_credentials(const char *ssid, const char *password)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    nvs_set_str(handle, NVS_KEY_SSID, ssid);
    nvs_set_str(handle, NVS_KEY_PASS, password ? password : "");
    err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

static esp_err_t nvs_clear_credentials(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    nvs_erase_all(handle);
    err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

/* ── Event handlers ───────────────────────────────────────────── */

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_state = WIFI_STATE_DISCONNECTED;
        status_set_wifi(WIFI_STATE_DISCONNECTED, NULL, "0.0.0.0", 0, 0);

        if (s_retry_count < MAX_STA_RETRIES) {
            s_retry_count++;
            ESP_LOGW(TAG, "STA disconnected, retry %d/%d", s_retry_count, MAX_STA_RETRIES);
            esp_wifi_connect();
        } else {
            ESP_LOGE(TAG, "STA connection failed after %d retries", MAX_STA_RETRIES);
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        char ip_str[16];
        esp_ip4addr_ntoa(&event->ip_info.ip, ip_str, sizeof(ip_str));

        /* Get current connection info */
        wifi_ap_record_t ap_info;
        int8_t rssi = 0;
        uint8_t channel = 0;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            rssi = ap_info.rssi;
            channel = ap_info.primary;
        }

        s_state = WIFI_STATE_CONNECTED;
        s_retry_count = 0;
        status_set_wifi(WIFI_STATE_CONNECTED, NULL, ip_str, rssi, channel);

        ESP_LOGI(TAG, "Connected — IP: %s, RSSI: %d, Ch: %d", ip_str, rssi, channel);
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/* ── STA connection ───────────────────────────────────────────── */

static esp_err_t try_sta_connect(const char *ssid, const char *password,
                                 uint32_t timeout_ms)
{
    ESP_LOGI(TAG, "Connecting to \"%s\"...", ssid);

    s_state = WIFI_STATE_CONNECTING;
    s_retry_count = 0;
    status_set_wifi(WIFI_STATE_CONNECTING, ssid, "0.0.0.0", 0, 0);

    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (password && password[0]) {
        strncpy((char *)wifi_config.sta.password, password,
                sizeof(wifi_config.sta.password) - 1);
    }

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(timeout_ms));

    if (bits & WIFI_CONNECTED_BIT) {
        return ESP_OK;
    }

    ESP_LOGW(TAG, "STA connection timed out or failed");
    esp_wifi_stop();
    return ESP_FAIL;
}

/* ── SoftAP provisioning ─────────────────────────────────────── */

static esp_err_t run_provisioning(void)
{
    ESP_LOGI(TAG, "Starting SoftAP provisioning: \"%s\"", CONFIG_AURACLE_SOFTAP_SSID);

    s_state = WIFI_STATE_PROVISIONING;
    status_set_wifi(WIFI_STATE_PROVISIONING, CONFIG_AURACLE_SOFTAP_SSID, "192.168.4.1", 0, 0);

    wifi_config_t ap_config = {
        .ap = {
            .channel = CONFIG_AURACLE_SOFTAP_CHANNEL,
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
        },
    };
    strncpy((char *)ap_config.ap.ssid, CONFIG_AURACLE_SOFTAP_SSID,
            sizeof(ap_config.ap.ssid) - 1);
    ap_config.ap.ssid_len = strlen(CONFIG_AURACLE_SOFTAP_SSID);

    if (sizeof(CONFIG_AURACLE_SOFTAP_PASSWORD) > 1) {
        strncpy((char *)ap_config.ap.password, CONFIG_AURACLE_SOFTAP_PASSWORD,
                sizeof(ap_config.ap.password) - 1);
        ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    }

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    esp_wifi_start();

    /* Start captive portal HTTP server */
    esp_err_t err = captive_portal_start(s_wifi_event_group);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start captive portal");
        return err;
    }

    ESP_LOGI(TAG, "Captive portal running — connect to \"%s\" and open http://192.168.4.1",
             CONFIG_AURACLE_SOFTAP_SSID);

    /* Block until credentials are submitted */
    xEventGroupWaitBits(s_wifi_event_group, WIFI_PROVISIONED_BIT,
                        pdTRUE, pdFALSE, portMAX_DELAY);

    /* Stop captive portal and SoftAP */
    captive_portal_stop();
    esp_wifi_stop();

    ESP_LOGI(TAG, "Provisioning complete, switching to STA mode");
    return ESP_OK;
}

/* ── Public API ───────────────────────────────────────────────── */

esp_err_t wifi_manager_init(void)
{
    s_wifi_event_group = xEventGroupCreate();
    if (!s_wifi_event_group) {
        return ESP_ERR_NO_MEM;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    s_sta_netif = esp_netif_create_default_wifi_sta();
    s_ap_netif  = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);

    /* Try stored credentials first */
    char ssid[33] = {0};
    char password[65] = {0};
    if (nvs_read_credentials(ssid, sizeof(ssid), password, sizeof(password))) {
        esp_err_t err = try_sta_connect(ssid, password,
                                        CONFIG_AURACLE_STA_CONNECT_TIMEOUT_MS);
        if (err == ESP_OK) {
            return ESP_OK;
        }
        ESP_LOGW(TAG, "Stored credentials failed, falling through to provisioning");
    } else {
        ESP_LOGI(TAG, "No stored credentials found");
    }

    /* Run provisioning loop until we get a successful STA connection */
    while (true) {
        esp_err_t err = run_provisioning();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Provisioning failed, retrying...");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        /* Read newly provisioned credentials */
        if (nvs_read_credentials(ssid, sizeof(ssid), password, sizeof(password))) {
            err = try_sta_connect(ssid, password, CONFIG_AURACLE_STA_CONNECT_TIMEOUT_MS);
            if (err == ESP_OK) {
                return ESP_OK;
            }
            ESP_LOGW(TAG, "Provisioned credentials failed, re-provisioning");
            nvs_clear_credentials();
        }
    }
}

esp_err_t wifi_manager_start_provisioning(void)
{
    ESP_LOGI(TAG, "Re-provisioning requested");
    esp_wifi_stop();
    nvs_clear_credentials();

    while (true) {
        esp_err_t err = run_provisioning();
        if (err != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        char ssid[33] = {0};
        char password[65] = {0};
        if (nvs_read_credentials(ssid, sizeof(ssid), password, sizeof(password))) {
            err = try_sta_connect(ssid, password, CONFIG_AURACLE_STA_CONNECT_TIMEOUT_MS);
            if (err == ESP_OK) {
                return ESP_OK;
            }
        }
        nvs_clear_credentials();
    }
}

wifi_state_t wifi_manager_get_state(void)
{
    return s_state;
}
