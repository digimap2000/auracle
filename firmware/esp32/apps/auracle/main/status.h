#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum {
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_PROVISIONING,
} wifi_state_t;

typedef struct {
    /* nRF5340 info (populated from hello message) */
    bool     nrf_connected;
    char     nrf_firmware[32];
    char     nrf_version[16];
    char     nrf_role[16];
    int64_t  nrf_hello_time_us;   /* esp_timer_get_time() when hello received */

    /* WiFi */
    wifi_state_t wifi_state;
    char     wifi_ssid[33];
    char     wifi_ip[16];
    int8_t   wifi_rssi;
    uint8_t  wifi_channel;

    /* System */
    int64_t  boot_time_us;
} auracle_status_t;

esp_err_t status_init(void);

/* Thread-safe accessors — copy the full struct */
void status_get(auracle_status_t *out);

/* Partial updaters (each acquires mutex internally) */
void status_set_nrf_hello(const char *firmware, const char *version, const char *role);
void status_set_nrf_disconnected(void);
void status_set_wifi(wifi_state_t state, const char *ssid, const char *ip, int8_t rssi, uint8_t channel);
