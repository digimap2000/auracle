#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

/* Bit set in the event group when credentials are submitted */
#define WIFI_PROVISIONED_BIT BIT2

/**
 * Start the captive portal HTTP server and DNS redirect.
 * Serves a WiFi configuration form on the SoftAP interface.
 *
 * @param event_group  Event group to signal WIFI_PROVISIONED_BIT on credential submission.
 */
esp_err_t captive_portal_start(EventGroupHandle_t event_group);

/**
 * Stop the captive portal HTTP server and DNS redirect.
 */
void captive_portal_stop(void);
