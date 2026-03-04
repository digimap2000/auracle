# Claude Code Prompt: Feature 002 Refinements

Paste the following into Claude Code:

---

There are three issues to fix in the BLE device discovery implementation. Before making changes, re-read:
1. `.skills/auracle-rust/SKILL.md` — particularly the updated Code Quality section on DRY and mutex lock scope
2. `.skills/auracle-frontend/SKILL.md` — particularly the new "Live-Updating Lists" section
3. `docs/features/002-ble-device-discovery.md` — the updated sort order requirement

## Fix 1: Stable device list sort order (Devices.tsx)

**Problem**: The device list sorts by RSSI (`b.rssi - a.rssi`) on every render. Since RSSI fluctuates with every BLE advertisement, devices constantly jump positions in the list. You can't click on a device because it moves before your finger lands.

**Fix**: In `src/pages/Devices.tsx`, change the `filteredDevices` sort in the `useMemo` to sort by name alphabetically as the primary sort, with "Unknown" devices pushed to the bottom, and RSSI as a tiebreaker only:

```typescript
return [...filtered].sort((a, b) => {
  const nameA = a.name === "Unknown" ? "\uffff" : a.name;
  const nameB = b.name === "Unknown" ? "\uffff" : b.name;
  const nameCmp = nameA.localeCompare(nameB);
  return nameCmp !== 0 ? nameCmp : b.rssi - a.rssi;
});
```

## Fix 2: Extract peripheral-to-BleDevice conversion (scanner.rs)

**Problem**: The code that extracts device data from a btleplug peripheral (name, rssi, services, manufacturer data, etc.) is duplicated verbatim between the event-stream path (lines ~117-143) and the polling fallback path (lines ~189-214). This is a maintenance hazard.

**Fix**: Extract a helper function in `src-tauri/src/ble/scanner.rs`:

```rust
async fn extract_device(
    id: String,
    peripheral: &btleplug::platform::Peripheral,
) -> Option<BleDevice> {
    let props = peripheral.properties().await.ok()??;
    // ... extract all fields, return Some(BleDevice { ... })
}
```

Call it from both the event-stream and polling paths.

## Fix 3: Consolidate mutex lock scopes (scanner.rs)

**Problem**: In the event-stream path, the scanner locks the mutex to insert a device (line ~145), drops the lock, then immediately re-locks to read all devices for the emit (line ~153). This is two lock acquisitions where one would do.

**Fix**: In the debounce check block, combine the insert and the emit read into a single lock scope. The insert already happens under a lock — hold that lock, check whether it's time to emit, and if so collect and emit within the same scope.

---

After making all three fixes:
- Run `cargo build` — zero warnings
- Run `npm run build` — zero TypeScript errors
- Update `docs/frontend.md` if the Devices page sort description needs changing
