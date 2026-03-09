#pragma once

#include "esp_err.h"

#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"

#define UART_BRIDGE_RX_CHUNK_SIZE 128

esp_err_t uart_bridge_init(void);
esp_err_t uart_bridge_write(const uint8_t *data, size_t len);
int uart_bridge_read(uint8_t *buf, size_t buf_size, TickType_t wait_ticks);
void uart_bridge_reset_rx(void);

