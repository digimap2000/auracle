# Claude Code Prompt: Bluetooth Adapter Detection

Paste the following into Claude Code when working in the Auracle repo:

---

Implement Bluetooth adapter detection as specified in `docs/features/001-bluetooth-adapter-detection.md`.

Before writing any code, read these files:
1. `.skills/auracle-rust/SKILL.md` — Rust backend patterns
2. `.skills/auracle-devices/SKILL.md` — BLE/btleplug patterns and macOS permissions
3. `.skills/auracle-frontend/SKILL.md` — React/TypeScript patterns
4. `docs/features/001-bluetooth-adapter-detection.md` — the feature spec (source of truth)

Then read the current source files you'll be modifying:
- `src-tauri/src/ble/mod.rs` (add BluetoothAdapter struct and get_adapter function)
- `src-tauri/src/error.rs` (add From<btleplug::Error> impl)
- `src-tauri/src/lib.rs` (add get_bluetooth_adapter command)
- `src/lib/tauri.ts` (add BluetoothAdapter interface and invoke wrapper)
- `src/hooks/useDevices.ts` (reference for hook patterns)
- `src/pages/Dashboard.tsx` (add adapter status card)
- `src/components/layout/StatusBar.tsx` (add Bluetooth indicator)

Implementation order:
1. Rust backend first: BluetoothAdapter type, btleplug integration in get_adapter(), error conversion, Tauri command
2. Frontend types and wrapper in tauri.ts
3. useBluetoothAdapter hook
4. Dashboard adapter status card
5. StatusBar Bluetooth indicator
6. Update docs: data-models.md, backend.md, frontend.md with new types, commands, and component changes

Run `cargo build` after the Rust changes and `npm run build` after the TypeScript changes to verify zero warnings and zero errors.

Do not leave any mock data — this feature replaces stubs with real btleplug calls. Do not modify any existing functionality that already works (updater, routing, existing stubs for scan/connect).
