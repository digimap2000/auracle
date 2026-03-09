#pragma once

#include <stdbool.h>
#include <stdint.h>

/*
 * device_proto — JSON Lines v1 protocol logic
 *
 * Handles request/response/event dispatch for the attached device UART
 * protocol. Runs a two-step startup discovery (get_info -> get_capabilities)
 * and stores a compact device identity/capability snapshot.
 *
 * Threading model:
 *   All protocol state is accessed only inside attached_device_process(),
 *   which drains the UART RX queue and checks timeouts. Call it from a
 *   single dedicated task; do not call it from multiple tasks concurrently.
 *
 * Extending:
 *   - New outgoing commands: add a protocol_send_<cmd>() static helper in
 *     device_proto.c and a TODO comment in protocol_handle_response().
 *   - New incoming events: add a case in protocol_handle_event().
 *   - New incoming commands from the device: add a case in
 *     protocol_handle_request().
 */

#define DEVICE_STR_MAX      32
#define DEVICE_MAX_CAPABILITIES 8

/**
 * Information about the attached device gathered during startup discovery.
 * All string fields are null-terminated. Count fields reflect populated
 * entries; unpopulated entries are zero-initialised.
 */
typedef struct {
    /* Startup query status */
    bool info_ok;          /* get_info response received and parsed */
    bool capabilities_ok;  /* get_capabilities response received and parsed */
    bool ready_seen;       /* Unsolicited "ready" event received */

    /* From get_info */
    char name[DEVICE_STR_MAX];
    char version[DEVICE_STR_MAX];

    /* From get_capabilities */
    char capabilities[DEVICE_MAX_CAPABILITIES][DEVICE_STR_MAX];
    int  capability_count;
} DeviceInfo;

/* ── Protocol interface (called by uart_proto) ─────────────────── */

/**
 * Parse a complete received JSON line and dispatch it to the appropriate
 * handler. Called by uart_proto_poll_rx() for each dequeued line.
 * Must run in the same task context as attached_device_process().
 */
void protocol_handle_line(const char *line);

/**
 * Send the get_info and get_capabilities startup requests.
 * Resets all device state before starting. Safe to call again to retry.
 * Called by attached_device_start_discovery(); may also be called directly.
 */
void protocol_query_startup_info(void);

/* ── Application API ───────────────────────────────────────────── */

/**
 * Returns true when both get_info and get_capabilities have completed
 * successfully. False during discovery, on timeout, or before it starts.
 */
bool attached_device_is_known(void);

/**
 * Returns true if startup discovery timed out or the device returned an
 * error. False while discovery is in progress or if it succeeded.
 */
bool attached_device_startup_failed(void);

/**
 * Returns a pointer to the device info struct. Always valid (never NULL).
 * Check info_ok / capabilities_ok before reading protocol fields.
 */
const DeviceInfo *attached_device_get_info(void);

/**
 * Reset device state and begin startup discovery (get_info → get_capabilities).
 * Internally calls protocol_query_startup_info().
 */
void attached_device_start_discovery(void);

/**
 * Drive the protocol: dequeue received lines, dispatch them, and check for
 * request timeouts. Call this in a tight loop with a short delay, e.g.:
 *
 *   while (true) {
 *       attached_device_process();
 *       vTaskDelay(pdMS_TO_TICKS(10));
 *   }
 */
void attached_device_process(void);
