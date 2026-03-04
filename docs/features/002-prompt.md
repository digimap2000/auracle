# Claude Code Prompt: BLE Device Discovery

Paste the following into Claude Code when working in the Auracle repo:

---

Implement BLE device discovery as specified in `docs/features/002-ble-device-discovery.md`.

Before writing any code, read these files in this order:
1. `.skills/auracle-devices/SKILL.md` — BLE/btleplug patterns, adapter access, error mapping
2. `.skills/auracle-rust/SKILL.md` — Rust backend patterns, Tauri state management, event emission
3. `.skills/auracle-frontend/SKILL.md` — React patterns, hooks, design system tokens, shadcn usage
4. `docs/features/002-ble-device-discovery.md` — the feature spec (source of truth for all requirements)

Then read every source file you'll be modifying to understand the current state before changing anything:
- `src-tauri/src/ble/mod.rs` — current BleDevice struct and mock scan()
- `src-tauri/src/lib.rs` — current command registrations
- `src-tauri/src/error.rs` — AuracleError and From impls
- `src/lib/tauri.ts` — current types and invoke wrappers
- `src/hooks/useDevices.ts` — current hook
- `src/pages/Devices.tsx` — current page (will be substantially rewritten)
- `src/App.tsx` — how hooks are wired to pages

## Implementation order

### 1. Rust backend (do this first, verify with `cargo build`)

**BLE module restructure** (`src-tauri/src/ble/`):
- Update `BleDevice` struct with the expanded fields from the spec: `tx_power`, `services`, `manufacturer_data`, `last_seen`. Add the `ManufacturerData` struct.
- Create a `BleScanner` struct with `Arc<tokio::sync::Mutex<ScannerState>>` that manages the scan lifecycle and device map. This is Tauri managed state — registered via `.manage()` in `lib.rs`.
- Implement `start_scan()` and `stop_scan()` methods on `BleScanner`.
- For scanning, prefer btleplug's event stream (`adapter.events()`) over polling `adapter.peripherals()`. The event stream gives you `CentralEvent::DeviceDiscovered` and `CentralEvent::DeviceUpdated` which are more efficient and lower-latency than polling. Fall back to periodic polling only if the event stream isn't available on the platform.
- When a peripheral is discovered or updated, extract ALL available data: name (from properties or local_name), RSSI, service UUIDs, manufacturer data (company ID + bytes), TX power level. Use `peripheral.properties().await` to get these.
- Emit a `ble-devices-updated` Tauri event with the full `Vec<BleDevice>` each time any device's data changes. Don't flood the frontend — debounce to at most ~2 emissions per second.
- The `last_seen` field should be an ISO 8601 string (`chrono::Utc::now().to_rfc3339()`). Add `chrono` to Cargo.toml dependencies if not present.
- If the mod.rs file is getting large (>150 lines), split into `ble/mod.rs` (types, public API) and `ble/scanner.rs` (BleScanner implementation).

**Command changes** in `lib.rs`:
- Remove the old `scan_ble_devices` command entirely.
- Add `start_ble_scan` and `stop_ble_scan` commands that take `AppHandle` and `State<'_, BleScanner>`.
- Register `BleScanner::new()` as managed state.

**Error handling**:
- If scanning is started when already running, return `Ok(())` silently (idempotent).
- If scanning is stopped when not running, return `Ok(())` silently.
- Adapter-not-found and permission errors should produce the same actionable messages as Feature 001.

### 2. Frontend types and wrappers

**Types** (`src/lib/tauri.ts`):
- Update `BleDevice` interface with the new fields.
- Add `ManufacturerData` interface.
- Replace `scanBleDevices()` with `startBleScan()` and `stopBleScan()`.

### 3. Hook update (`src/hooks/useDevices.ts`)

- Replace `scan()` with `startScan()` and `stopScan()`.
- Listen to the `ble-devices-updated` event using `import { listen } from "@tauri-apps/api/event"`.
- Clean up the listener on unmount.
- Call `stopBleScan()` on unmount as cleanup.
- The `scanning` boolean should reflect whether a scan is actively running (set true on startScan, false on stopScan).

### 4. Devices page rewrite (`src/pages/Devices.tsx`)

This is the biggest UI change. The page becomes a two-panel BLE explorer.

**Layout**: Use CSS grid — `grid-cols-[1fr_minmax(320px,0.4fr)]` for the two-panel layout. On small screens, stack vertically.

**Toolbar**:
- Start/Stop toggle button. When idle: "Start Scan" with `Play` icon, primary variant. When scanning: "Stop" with `Square` icon, destructive variant.
- Filter input (the existing one, but wire it up): filters the device list client-side by name or ID. Use `useState` for the filter string, `useMemo` for the filtered list.
- Device count: "{N} devices" as a muted text span next to the filter.

**Device list (left panel)**:
- `ScrollArea` containing rows.
- Each row is a clickable card-like element showing:
  - **Name**: `text-sm font-medium`. If no name, show "Unknown" in `italic text-muted-foreground`.
  - **ID**: `font-mono text-[10px] text-muted-foreground`
  - **RSSI indicator**: colour-coded — use `text-green-500` for > -50 dBm, `text-yellow-500` for -50 to -70, `text-red-500` for < -70. Show the dBm value alongside (e.g. "-42 dBm" in `font-mono text-[10px]`).
  - **Last seen**: relative time like "2s ago", "45s ago". Compute this client-side from the ISO timestamp. Dim the entire row (reduce opacity to 50%) when stale (>30 seconds).
- Sort by RSSI (strongest first).
- Selected row gets `bg-accent/50` background and a left border accent.
- Use a proper `selectedDeviceId` state. Update the detail pane reactively.

**Detail pane (right panel)**:
- Empty state when nothing selected: centered text "Select a device to view details" with a `Bluetooth` icon above it.
- When a device is selected, show a scrollable detail view:
  - **Header**: Device name (`text-lg font-medium`) and ID (`font-mono text-xs text-muted-foreground`).
  - **Signal section**: RSSI with the colour indicator, TX power if available, or "Not advertised" in muted if null.
  - **Services section**: List service UUIDs. Resolve well-known UUIDs to human-readable names where possible (at minimum: Generic Access 0x1800, Generic Attribute 0x1801, Heart Rate 0x180D, Battery 0x180F, Device Information 0x180A, Audio Stream Control 0x184E, Broadcast Audio Scan 0x184F, Published Audio Capabilities 0x1850, Basic Audio Profile 0x1851). Create a lookup map for these. Show unknown UUIDs in monospace.
  - **Manufacturer Data section**: For each entry, show company name if known (at minimum: Nordic 0x0059, Apple 0x004C, Google 0x00E0, Microsoft 0x0006, Samsung 0x0075, Qualcomm 0x000A, Bose 0x009E, Sony 0x012D). Show company ID in hex and the data bytes as a hex dump in monospace (e.g. `0x004C: 02 15 A4 95 ...`).
  - **Timing section**: "First seen" and "Last seen" timestamps in `font-mono text-[11px]`.
  - Use `Separator` components between sections. Each section has a small heading in `text-[11px] font-medium uppercase tracking-wider text-muted-foreground`.
- The detail pane should reactively update as new scan data arrives — if the selected device gets an RSSI update, it should reflect immediately.

**Scanning indicator**: When scanning is active, add a subtle pulsing ring or border around the page content area, and show a small "Scanning..." badge with a pulsing dot next to the device count.

### 5. Wire up in App.tsx

- Pass `startScan` and `stopScan` (instead of `scan`) to the Devices page.
- Update the Devices page props interface.

### 6. Update specifications

Update these docs to reflect all changes:
- `docs/data-models.md` — updated BleDevice, new ManufacturerData, removed old IPC entry, new commands and event
- `docs/backend.md` — updated BLE module docs, BleScanner state, new commands, removed scan_ble_devices
- `docs/frontend.md` — updated useDevices hook, substantially updated Devices page section

### 7. Build verification

- Run `cargo build` — zero warnings
- Run `npm run build` — zero TypeScript errors
- Run `cargo clippy` if available — zero warnings

## Quality expectations

- The BLE scanner must use btleplug's event stream, not naive polling. This is important for battery life and responsiveness.
- The well-known UUID and company ID lookup maps should be proper const HashMaps or match expressions, not inline if/else chains.
- The detail pane must update reactively — don't require re-selecting a device to see updated data.
- The filter must be instant (client-side `useMemo`, not a backend call).
- Use the existing shadcn components (Card, Badge, ScrollArea, Separator, Skeleton, Tooltip) — don't create custom styled divs when a component exists.
- All text sizes, fonts, and colours must follow `docs/design-system.md` and the auracle-frontend skill. No raw hex colours.
- The event debouncing on the Rust side is important — without it, a busy Bluetooth environment will flood the frontend with updates. 2 emissions per second maximum.
