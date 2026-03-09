#pragma once

#include "esp_err.h"

/*
 * uart_proto — UART JSON Lines transport layer
 *
 * Owns the UART peripheral. Accumulates bytes into lines (delimited by '\n'),
 * enforces the 256-byte maximum line length from the JSON Lines v1 spec, and
 * exposes a polling function that dispatches complete lines to the protocol
 * layer via protocol_handle_line().
 *
 * Thread safety:
 *   uart_proto_send_json_line() is thread-safe (UART driver handles locking).
 *   uart_proto_poll_rx() must be called from a single task (no locking needed
 *   on the dequeue path since only one consumer is expected).
 */

/**
 * Initialise the UART peripheral and start the RX accumulation task.
 *
 * Uses Kconfig values:
 *   CONFIG_AURACLE_UART_PORT_NUM   UART peripheral number (default 1)
 *   CONFIG_AURACLE_UART_TX_PIN     TX GPIO (default 20)
 *   CONFIG_AURACLE_UART_RX_PIN     RX GPIO (default 21)
 *   CONFIG_AURACLE_UART_BAUD_RATE  baud rate (default 115200)
 *   CONFIG_AURACLE_UART_RX_BUF_SIZE  driver ring buffer (default 2048)
 */
esp_err_t uart_proto_init(void);

/**
 * Send a compact JSON object as a single line.
 * Appends '\n'. json must not contain embedded newlines.
 * Thread-safe — may be called from any task.
 */
esp_err_t uart_proto_send_json_line(const char *json);

/**
 * Dequeue all received complete lines and dispatch each to
 * protocol_handle_line(). Returns immediately if no lines are waiting.
 * Call this periodically from your application task.
 */
void uart_proto_poll_rx(void);
