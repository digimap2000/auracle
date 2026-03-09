#pragma once

#include "esp_err.h"

/**
 * Start the REST API + SSE HTTP server.
 * Call only after WiFi STA is connected.
 */
esp_err_t http_server_start(void);

/**
 * Stop the HTTP server.
 */
esp_err_t http_server_stop(void);
