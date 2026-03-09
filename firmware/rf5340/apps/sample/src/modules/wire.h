/*
 * Auracle Wire Protocol — NDJSON over USB CDC ACM
 *
 * Dumb radio pipe: device reports raw observations, desktop does all thinking.
 * All taps are off by default. Desktop sends commands to enable them.
 *
 * See docs/features/005-wire-protocol.md for the protocol specification.
 */

#ifndef _WIRE_H_
#define _WIRE_H_

#include <stdint.h>
#include <zephyr/bluetooth/addr.h>

/** Initialise the wire protocol transport (USB CDC ACM).
 *  Starts the RX processing thread. Sends hello when host connects (DTR).
 *  @return 0 on success, negative errno on failure.
 */
int wire_init(void);

/** Report a BLE advertisement. No-op if adv tap is disabled.
 *  @param addr      Advertiser address.
 *  @param rssi      RSSI in dBm.
 *  @param data      Raw advertisement data bytes.
 *  @param data_len  Length of data.
 */
void wire_send_adv(const bt_addr_le_t *addr, int8_t rssi,
		   const uint8_t *data, uint16_t data_len);

/** Report a PA sync state change. No-op if sync tap is disabled.
 *  @param state        "synced" or "lost".
 *  @param broadcast_id 24-bit broadcast ID.
 *  @param addr         Broadcaster address.
 */
void wire_send_sync(const char *state, uint32_t broadcast_id,
		    const bt_addr_le_t *addr);

/** Report a BIS stream state change. No-op if bis tap is disabled.
 *  @param state        "streaming" or "stopped".
 *  @param broadcast_id 24-bit broadcast ID.
 *  @param bis_index    BIS index within the BIG.
 */
void wire_send_bis(const char *state, uint32_t broadcast_id,
		   uint8_t bis_index);

#endif /* _WIRE_H_ */
