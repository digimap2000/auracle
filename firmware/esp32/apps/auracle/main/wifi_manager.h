#pragma once

#include "esp_err.h"
#include "status.h"

/**
 * Initialize WiFi subsystem and attempt connection.
 *
 * If NVS contains stored credentials, attempts STA connection.
 * If no credentials exist or STA fails, starts SoftAP provisioning
 * and blocks until credentials are submitted and STA connects.
 *
 * On return, WiFi STA is connected and the IP is available via status_get().
 */
esp_err_t wifi_manager_init(void);

/**
 * Force re-provisioning.
 * Disconnects STA, clears NVS credentials, starts SoftAP + captive portal.
 * Blocks until new credentials are submitted and STA connects.
 */
esp_err_t wifi_manager_start_provisioning(void);

/**
 * Get current WiFi state.
 */
wifi_state_t wifi_manager_get_state(void);
