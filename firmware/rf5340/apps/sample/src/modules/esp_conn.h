/*
 * Auracle ESP32 Connection — JSON Lines v1 over physical UART
 *
 * Device (server) side of the JSON Lines v1 protocol.
 * The ESP32 bridge is the controller (client); the nRF5340 is the device
 * (server). This module:
 *   - Sends a "ready" event when esp_conn_announce_ready() is called.
 *   - Responds to get_info and get_capabilities requests.
 *   - Returns unknown_cmd for any other request.
 *   - Provides esp_conn_send_event() for future unsolicited events.
 *
 * See firmware/esp32/apps/auracle for the protocol specification.
 */

#ifndef _ESP_CONN_H_
#define _ESP_CONN_H_

#include <stddef.h>
#include <stdint.h>

#include <zephyr/bluetooth/bluetooth.h>

/** Initialise the ESP32 connection (UART2).
 *  Configures the UART, enables async RX, and starts the RX
 *  processing thread. Does NOT send the "ready" event — call
 *  esp_conn_announce_ready() after all application subsystems are up.
 *  @return 0 on success, negative errno on failure.
 */
int esp_conn_init(void);

/** Send the "ready" event to the ESP32.
 *  Call this at the very end of application initialisation, after all
 *  subsystems (BT, audio, USB) are running and deferred logs have been
 *  flushed. This signals to the ESP32 that the device is ready to accept
 *  protocol commands.
 */
void esp_conn_announce_ready(void);

/** Send an unsolicited event to the ESP32.
 *  @param event_name  Event name string (e.g. "stream_state").
 *  @param extra_json  Optional extra key-value pairs to include in the
 *                     event object, as a raw JSON fragment without braces
 *                     (e.g. "\"state\":\"streaming\""). Pass NULL or "" for
 *                     no extra fields.
 */
void esp_conn_send_event(const char *event_name, const char *extra_json);

/** Send one raw BLE advertisement report to the ESP32.
 *  The payload is forwarded without nRF-side decoding. Metadata from the scan
 *  callback is included so the host can reconstruct what was observed on air.
 *  Payload bytes are hex encoded so the host can recover the exact bytes
 *  without any nRF-side BLE parsing.
 *
 *  @param info      Zephyr scan report metadata.
 *  @param data      Raw advertisement payload bytes.
 *  @param data_len  Raw payload length in bytes.
 */
void esp_conn_send_adv_report(const struct bt_le_scan_recv_info *info,
			      const uint8_t *data, size_t data_len);

#endif /* _ESP_CONN_H_ */
