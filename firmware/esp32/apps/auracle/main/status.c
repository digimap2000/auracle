#include "status.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "status";

static auracle_status_t s_status;
static SemaphoreHandle_t s_mutex;

esp_err_t status_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        ESP_LOGE(TAG, "Failed to create status mutex");
        return ESP_ERR_NO_MEM;
    }

    memset(&s_status, 0, sizeof(s_status));
    s_status.boot_time_us = esp_timer_get_time();
    s_status.wifi_state = WIFI_STATE_DISCONNECTED;

    return ESP_OK;
}

void status_get(auracle_status_t *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    memcpy(out, &s_status, sizeof(auracle_status_t));
    xSemaphoreGive(s_mutex);
}

void status_set_nrf_hello(const char *firmware, const char *version, const char *role)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_status.nrf_connected = true;
    s_status.nrf_hello_time_us = esp_timer_get_time();
    strncpy(s_status.nrf_firmware, firmware, sizeof(s_status.nrf_firmware) - 1);
    s_status.nrf_firmware[sizeof(s_status.nrf_firmware) - 1] = '\0';
    strncpy(s_status.nrf_version, version, sizeof(s_status.nrf_version) - 1);
    s_status.nrf_version[sizeof(s_status.nrf_version) - 1] = '\0';
    strncpy(s_status.nrf_role, role, sizeof(s_status.nrf_role) - 1);
    s_status.nrf_role[sizeof(s_status.nrf_role) - 1] = '\0';
    xSemaphoreGive(s_mutex);

    ESP_LOGI(TAG, "nRF5340 hello: %s v%s (%s)", firmware, version, role);
}

void status_set_nrf_disconnected(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_status.nrf_connected = false;
    xSemaphoreGive(s_mutex);

    ESP_LOGW(TAG, "nRF5340 disconnected");
}

void status_set_wifi(wifi_state_t state, const char *ssid, const char *ip,
                     int8_t rssi, uint8_t channel)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_status.wifi_state = state;

    if (ssid) {
        strncpy(s_status.wifi_ssid, ssid, sizeof(s_status.wifi_ssid) - 1);
        s_status.wifi_ssid[sizeof(s_status.wifi_ssid) - 1] = '\0';
    }
    if (ip) {
        strncpy(s_status.wifi_ip, ip, sizeof(s_status.wifi_ip) - 1);
        s_status.wifi_ip[sizeof(s_status.wifi_ip) - 1] = '\0';
    }
    s_status.wifi_rssi = rssi;
    s_status.wifi_channel = channel;
    xSemaphoreGive(s_mutex);
}
