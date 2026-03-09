#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    WIRE_MSG_HELLO,
    WIRE_MSG_ADV,
    WIRE_MSG_SYNC,
    WIRE_MSG_BIS,
    WIRE_MSG_LOG,
    WIRE_MSG_UNKNOWN,
} wire_msg_type_t;

typedef struct {
    wire_msg_type_t type;
    char           *json_str;      /* Heap-allocated raw NDJSON line */
    int64_t         received_us;   /* esp_timer_get_time() when received */
    size_t          seq;           /* Monotonic sequence number */
} wire_msg_t;

/**
 * Initialize the wire protocol parser and ring buffer.
 * Must be called after wire_uart_init().
 */
esp_err_t wire_proto_init(void);

/**
 * Feed a complete NDJSON line from UART RX.
 * Called by wire_uart RX task. Parses type, stores in ring buffer.
 */
void wire_proto_feed_line(const char *line);

/**
 * Get the current head sequence number (latest message written).
 * Returns 0 if no messages have been received yet.
 */
size_t wire_proto_get_latest_seq(void);

/**
 * Read a message by sequence number.
 * Returns a pointer to internal storage (valid until overwritten by ring wrap).
 * Returns NULL if seq has been evicted or is not yet written.
 *
 * Caller must NOT free the returned pointer or its json_str.
 * Copy json_str if you need it beyond the next ring wrap.
 */
const wire_msg_t *wire_proto_get_msg(size_t seq);

/**
 * Send a raw command JSON string to the nRF5340.
 * Appends '\n' and transmits via wire_uart_send().
 * The json_cmd must NOT include a trailing newline.
 */
esp_err_t wire_proto_send_raw(const char *json_cmd);

/**
 * Send a tap_enable or tap_disable command.
 * tap_name: "adv", "sync", "bis", or "log".
 */
esp_err_t wire_proto_send_tap(const char *tap_name, bool enable);

/**
 * Send scan_start or scan_stop command.
 */
esp_err_t wire_proto_send_scan(bool start);
