# 005 — Wire Protocol

Structured communication between nRF5340 firmware and Auracle desktop over USB CDC ACM serial.

## Principle

The nRF is a dumb radio. It does not interpret, filter, or decode data — it reports raw observations and executes commands. All intelligence lives on the desktop.

## Transport

- USB CDC ACM serial (appears as `/dev/tty.usbmodem*` on macOS)
- NDJSON: one JSON object per `\n` (0x0A). No embedded newlines.
- Both directions use the same framing.
- Baud rate irrelevant (USB virtual serial), but set 115200 for tool compatibility.

## Message Structure

Every message has a `type` field. Upstream messages (device → desktop) describe observations. Downstream messages (desktop → device) are commands.

### Upstream (device → desktop)

```json
{"type":"adv","ts":12345,"addr":"24:5D:FC:03:C1:01","addr_type":"random","rssi":-67,"data":"0201061bff4c00..."}
```

| Field | Type | Description |
|-------|------|-------------|
| `type` | string | Message type identifier |
| `ts` | integer | Device uptime in milliseconds (`k_uptime_get()`) |

Additional fields depend on `type`. The `data` fields carry raw bytes as lowercase hex strings — no decoding on-device.

#### `adv` — BLE advertisement received

| Field | Type | Description |
|-------|------|-------------|
| `addr` | string | Advertiser address, colon-separated hex |
| `addr_type` | string | `"public"` or `"random"` |
| `rssi` | integer | RSSI in dBm |
| `data` | string | Raw advertisement data as hex |
| `scan_rsp` | string? | Raw scan response data as hex, if available |

#### `sync` — Periodic advertising sync state change

| Field | Type | Description |
|-------|------|-------------|
| `state` | string | `"synced"`, `"lost"` |
| `broadcast_id` | string | 24-bit broadcast ID as `"0xNNNNNN"` |
| `addr` | string | Broadcaster address |

#### `bis` — BIS stream state change

| Field | Type | Description |
|-------|------|-------------|
| `state` | string | `"streaming"`, `"stopped"` |
| `broadcast_id` | string | Broadcast ID |
| `bis_index` | integer | BIS index within the BIG |

#### `log` — Firmware log entry

| Field | Type | Description |
|-------|------|-------------|
| `level` | string | `"err"`, `"wrn"`, `"inf"`, `"dbg"` |
| `module` | string | Source module name |
| `msg` | string | Log message text |

### Downstream (desktop → device)

```json
{"type":"cmd","action":"tap_enable","tap":"adv"}
```

| Field | Type | Description |
|-------|------|-------------|
| `type` | string | Always `"cmd"` |
| `action` | string | Command action |

#### Actions

| Action | Parameters | Description |
|--------|-----------|-------------|
| `tap_enable` | `tap`: string | Start reporting a data stream |
| `tap_disable` | `tap`: string | Stop reporting a data stream |
| `scan_start` | | Begin BLE scanning |
| `scan_stop` | | Stop BLE scanning |
| `set_target` | `broadcast_id`: string | Set target broadcast ID for sync |

#### Taps

Taps control which upstream message types the device emits. All taps are off by default — the device is silent until told to report.

| Tap | Upstream type | Description |
|-----|--------------|-------------|
| `adv` | `adv` | Raw BLE advertisements |
| `sync` | `sync` | PA sync state changes |
| `bis` | `bis` | BIS stream state changes |
| `log` | `log` | Firmware log output |

## Startup

On USB connect, the device sends a single identification message:

```json
{"type":"hello","firmware":"auracle-nrf5340","version":"0.1.0","role":"sink"}
```

All taps are off. The device waits for commands.

## Status: Draft

This will churn. Implement the minimum, see what it looks like, iterate.
