# Feature 002: BLE Device Discovery

## Summary

Replace the mock BLE scan with real continuous scanning via btleplug. Devices appear in a live-updating list as they're discovered. The Devices page becomes a functional BLE explorer — a radar for nearby Bluetooth LE devices.

## Motivation

With the adapter detected (Feature 001), we can now scan for nearby BLE peripherals. This is the primary discovery mechanism for finding nRF5340 boards and monitoring the Bluetooth environment during Auracast testing. It also gives us immediate value while waiting for test hardware — any BLE device in range (phones, headphones, beacons) will appear.

## Design Decisions

- **Continuous scanning**: Click Start to begin, devices appear in real-time, click Stop to finish. Not a one-shot timed scan.
- **Essential list + detail pane**: The device list shows the key info (name, address, RSSI, last seen). Selecting a device shows a detail panel on the right with all available advertisement data. This detail pane is intentionally designed to grow — we'll add more fields as we learn what matters during hardware testing.
- **RSSI auto-updates**: While scanning, RSSI values update live for already-discovered devices. Devices that haven't been seen for 30 seconds get visually dimmed (stale).

## Requirements

### Backend (Rust)

#### Updated type: `BleDevice`

File: `src-tauri/src/ble/mod.rs`

Expand the existing struct with additional advertisement fields:

```rust
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BleDevice {
    pub id: String,                        // MAC address or UUID
    pub name: String,                      // Advertised name, or "Unknown" if none
    pub rssi: i16,                         // Signal strength in dBm
    pub is_connected: bool,                // Whether currently connected
    pub tx_power: Option<i16>,             // Transmit power level if advertised
    pub services: Vec<String>,             // Advertised service UUIDs as strings
    pub manufacturer_data: Vec<ManufacturerData>, // Manufacturer-specific data
    pub last_seen: String,                 // ISO 8601 timestamp of last advertisement
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ManufacturerData {
    pub company_id: u16,                   // Bluetooth SIG company identifier
    pub data: Vec<u8>,                     // Raw data bytes
}
```

#### New Tauri commands: `start_ble_scan`, `stop_ble_scan`

File: `src-tauri/src/lib.rs`

```rust
#[tauri::command]
async fn start_ble_scan(app: tauri::AppHandle, state: tauri::State<'_, BleScanner>) -> Result<(), AuracleError>

#[tauri::command]
async fn stop_ble_scan(state: tauri::State<'_, BleScanner>) -> Result<(), AuracleError>
```

These replace the old `scan_ble_devices` command (remove it).

#### New managed state: `BleScanner`

File: `src-tauri/src/ble/mod.rs` (or `src-tauri/src/ble/scanner.rs` if mod.rs gets too large)

```rust
pub struct BleScanner {
    inner: Arc<tokio::sync::Mutex<ScannerState>>,
}

struct ScannerState {
    scanning: bool,
    devices: HashMap<String, BleDevice>,  // keyed by device ID
    // btleplug handles
}
```

The scanner:
- Calls `adapter.start_scan(ScanFilter::default())` on start
- Spawns a tokio task that polls `adapter.peripherals()` periodically (every ~1 second) or listens to btleplug events
- For each discovered peripheral, extracts: id, name (or "Unknown"), rssi, services, manufacturer data, tx power
- Emits a Tauri event `ble-devices-updated` with the full device list each time it changes
- Calls `adapter.stop_scan()` on stop
- Updates `last_seen` timestamp on each device every time it's seen in an advertisement

Register as managed state: `.manage(BleScanner::new())`

#### Event: `ble-devices-updated`

Payload: `Vec<BleDevice>` — the complete current device list, not a delta. This keeps the frontend simple (just replace the list).

#### Remove old command

Remove `scan_ble_devices` from `lib.rs` and `generate_handler!`. The old mock `scan()` function can be removed from `ble/mod.rs`.

### Frontend (TypeScript)

#### Updated type: `BleDevice`

File: `src/lib/tauri.ts`

```typescript
export interface BleDevice {
  id: string;
  name: string;
  rssi: number;
  is_connected: boolean;
  tx_power: number | null;
  services: string[];
  manufacturer_data: ManufacturerData[];
  last_seen: string;
}

export interface ManufacturerData {
  company_id: number;
  data: number[];
}
```

#### New invoke wrappers

File: `src/lib/tauri.ts`

```typescript
export async function startBleScan(): Promise<void> {
  return invoke<void>("start_ble_scan");
}

export async function stopBleScan(): Promise<void> {
  return invoke<void>("stop_ble_scan");
}
```

Remove `scanBleDevices()`.

#### Updated hook: `useDevices`

File: `src/hooks/useDevices.ts`

Replace the `scan()` function with `startScan()` and `stopScan()`. Add a `scanning` boolean that reflects whether a scan is actively running.

Listen to the `ble-devices-updated` Tauri event to receive live device updates:

```typescript
useEffect(() => {
  const unlisten = listen<BleDevice[]>("ble-devices-updated", (event) => {
    setState(prev => ({ ...prev, bleDevices: event.payload }));
  });
  return () => { unlisten.then(fn => fn()); };
}, []);
```

The hook should also stop scanning on unmount (cleanup).

Return value adds: `startScan`, `stopScan` (replacing `scan`).

#### Devices page redesign

File: `src/pages/Devices.tsx`

**Layout**: Two-panel. Device list on the left (~60% width), detail pane on the right (~40% width). On narrow windows, detail pane collapses below the list.

**Toolbar**:
- Start/Stop button: toggles between "Start Scan" (with `Play` icon) and "Stop Scan" (with `Square` icon). Blue primary style when idle, destructive when scanning.
- Filter input: filters the visible device list by name or ID (client-side, instant)
- Device count badge: "N devices" in muted text

**Device list** (left panel):
- ScrollArea containing device rows
- Each row shows:
  - Device name (or "Unknown" in muted italic if no name)
  - RSSI as a signal strength indicator (e.g. a simple bar or the dBm value with colour: green > -50, yellow -50 to -70, red < -70)
  - Relative time since last seen (e.g. "2s ago", "15s ago") — dims to muted after 30s
- **No device ID in list rows** — the peripheral ID is not useful as a user-facing identifier (on macOS it's a CoreBluetooth-assigned UUID, not a hardware address). The ID is shown only in the detail pane.
- Rows are sorted by **name alphabetically** by default (with "Unknown" devices sorted to the bottom), using RSSI as a secondary sort. This keeps the list stable — RSSI fluctuates constantly and sorting by it makes the list unusable during a live scan. RSSI is displayed per-row as a visual indicator but does not drive the sort order.
- Clicking a row selects it and shows detail in the right pane
- Selected row has a subtle `bg-accent` highlight

**Detail pane** (right panel):
- Shows when a device is selected, otherwise shows an empty state: "Select a device to view details"
- Header: device name (large) and peripheral ID (monospace, muted) with a platform-aware label: "Peripheral ID (macOS)" on macOS, "MAC Address" on Linux/Windows
- Sections (each in a small card or grouped area):
  - **Signal**: RSSI value, TX power (if available)
  - **Services**: List of advertised service UUIDs. Each entry always shows the UUID in monospace. If the UUID resolves to a known name (SIG standard or custom), show the name as a badge alongside the UUID. Known services include both Bluetooth SIG standard UUIDs (e.g., "Device Information", "Battery Service") and custom UUIDs registered by our products (e.g., "Focal Naim Auracast"). **Do not deduplicate** — if the device advertises the same service UUID multiple times, show it multiple times. This is a test tool and duplicate advertisements may indicate a firmware issue.
    - Known custom service UUIDs:
      - `3e1d50cd-7e3e-427d-8e1c-b78aa87fe624` → "Focal Naim Auracast"
  - **Manufacturer Data**: Company ID and hex dump of data bytes in monospace. If company ID maps to a known name (Nordic = 0x0059, Apple = 0x004C), show it.
  - **Timing**: Last seen timestamp, time since first seen
- This pane is intentionally extensible — designed to accept new sections as we learn what data matters

**Empty state** (no scan run yet):
- Replaces the current empty card
- "Click Start Scan to discover nearby Bluetooth devices"

**Scanning active state**:
- A subtle pulsing border or indicator on the page showing the scan is live

### IPC Contract Changes

Remove:
| `scanBleDevices()` | `scan_ble_devices` | none | `BleDevice[]` |

Add:
| TypeScript function | Tauri command | Parameters | Return type |
|--------------------|---------------|------------|-------------|
| `startBleScan()` | `start_ble_scan` | none | `void` |
| `stopBleScan()` | `stop_ble_scan` | none | `void` |

New event:
| Event name | Payload | Direction |
|-----------|---------|-----------|
| `ble-devices-updated` | `BleDevice[]` | Backend → Frontend |

### Spec Updates Required

After implementation, update:
- `docs/data-models.md` — update `BleDevice` type, add `ManufacturerData`, remove old IPC entry, add new commands and event
- `docs/backend.md` — update BLE module section, add `BleScanner` state, new commands, remove `scan_ble_devices`
- `docs/frontend.md` — update `useDevices` hook, update Devices page section

## Acceptance Criteria

1. `cargo build` completes with zero warnings
2. `npm run build` completes with zero TypeScript errors
3. Clicking Start Scan begins real BLE scanning — nearby devices appear in the list within seconds
4. RSSI values update live while scanning is active
5. Clicking Stop Scan stops scanning, the device list remains visible
6. The filter input narrows the visible list by name or ID
7. Selecting a device shows its full advertisement data in the detail pane
8. Devices not seen for 30+ seconds are visually dimmed
9. The scanner cleans up properly (stop_scan called) when navigating away from the page
10. Error states (adapter unavailable, permission denied) display actionable messages
11. All `docs/` specs updated to reflect the changes
