#pragma once

#include "esp_err.h"
#include <stddef.h>

/**
 * Initialize UART hardware and start the RX task.
 * Must be called before wire_proto_init().
 */
esp_err_t wire_uart_init(void);

/**
 * Send data to the nRF5340 over UART.
 * Thread-safe (ESP-IDF UART driver handles internal locking).
 */
esp_err_t wire_uart_send(const char *data, size_t len);
