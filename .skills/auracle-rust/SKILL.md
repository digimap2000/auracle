---
name: auracle-rust
description: "Rust/Tauri backend development for Auracle. Use this skill whenever writing, modifying, or reviewing Rust code in the frontend/src-tauri/ directory. Covers Tauri command patterns, module structure, error handling, state management, and plugin registration. Trigger on any backend work: new commands, new modules, error variants, device trait implementations, BLE or serial code, or Cargo dependency changes."
---

# Auracle Rust Backend

This skill encodes the patterns and constraints for all Rust backend work in Auracle. The specs in `docs/backend.md`, `docs/architecture.md`, and `docs/data-models.md` are the source of truth — read them before making changes if you haven't already this session.

## Project Location

The Rust backend lives in `frontend/src-tauri/src/`. The crate is called `auracle_lib` and produces `staticlib`, `cdylib`, and `rlib` targets. Rust edition is 2021.

## Architecture Rules

Tauri commands are a **translation layer only**. They accept parameters from the frontend, delegate to domain modules, and return results. No business logic lives in command handlers. This separation exists so domain logic can be tested independently and reused across commands.

```
Frontend (TS) → invoke() → Tauri Command (thin) → Domain Module (logic) → Hardware/OS
```

Each domain area gets its own module directory:

```
frontend/src-tauri/src/
├── main.rs           # Entry point only: calls auracle_lib::run()
├── lib.rs            # App setup, command registration, command handlers
├── error.rs          # AuracleError enum (thiserror)
├── ble/mod.rs        # BLE scanning and adapter management
├── serial/mod.rs     # Serial port enumeration
└── devices/
    ├── mod.rs        # AuracleDevice trait, Command, Response, ConnectedDevice
    └── nrf5340.rs    # nRF5340 Audio DK implementation
```

When a module grows beyond ~200 lines, split it into submodules within its directory. Prefer one public type or trait per file.

## Adding a Tauri Command

This is the most common backend task. Follow this exact sequence:

1. **Define the command** in `lib.rs` as an async function with `#[tauri::command]`:

```rust
#[tauri::command]
async fn my_new_command(param: String) -> Result<ReturnType, AuracleError> {
    domain_module::do_thing(param).await
}
```

2. **Register it** in the `generate_handler!` macro — order doesn't matter but keep it alphabetical for readability:

```rust
.invoke_handler(tauri::generate_handler![
    connect_device,
    disconnect_device,
    get_connected_devices,
    my_new_command,        // ← add here
    scan_ble_devices,
    scan_serial_ports,
])
```

3. **Add the frontend wrapper** in `frontend/src/lib/tauri.ts` (see the auracle-frontend skill for TypeScript patterns). The frontend wrapper must exist before the command is considered complete.

4. **If new types are needed**, define the Rust struct with derives and a matching TypeScript interface:

```rust
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MyType {
    pub field_name: String,      // → fieldName in TypeScript (Tauri auto-camelCases)
    pub optional_field: Option<u32>, // → optionalField: number | null
}
```

## Error Handling

All fallible commands return `Result<T, AuracleError>`. The error enum lives in `error.rs` and uses `thiserror` for Display derivation. A custom `Serialize` impl converts errors to their Display string, so the frontend receives them as plain string messages.

To add a new error variant:

```rust
#[derive(Debug, thiserror::Error)]
pub enum AuracleError {
    // ... existing variants ...

    #[error("Adapter error: {0}")]
    Adapter(String),
}
```

The Serialize impl handles it automatically — no additional work needed.

Errors should be **specific and actionable**. "BLE error: failed" is useless. "BLE error: no adapter found — check Bluetooth is enabled in System Settings" tells the user what happened and what to do. The frontend will display these strings directly.

## State Management

When a command needs access to shared state (like a device registry or adapter handle), use Tauri's managed state:

```rust
// In lib.rs setup:
.manage(DeviceRegistry::new())

// In command handlers:
#[tauri::command]
async fn my_command(
    state: tauri::State<'_, DeviceRegistry>,
) -> Result<(), AuracleError> {
    let registry = state.inner();
    // ...
}
```

Wrap mutable state in `Arc<Mutex<T>>` or `Arc<tokio::sync::Mutex<T>>` (prefer tokio's Mutex for async code to avoid blocking the runtime). The Tauri State extractor handles the Arc layer.

## Emitting Events to Frontend

For real-time updates (device status changes, log entries, scan progress), use Tauri events rather than polling:

```rust
#[tauri::command]
async fn start_scan(app: tauri::AppHandle) -> Result<(), AuracleError> {
    // Emit progress
    app.emit("scan-progress", ScanProgress { found: 3 })?;
    Ok(())
}
```

Event payloads must implement `Serialize + Clone`. Name events in kebab-case.

## Dependency Conventions

Current Cargo.toml dependencies and their purposes:

| Crate | Version | Purpose | Status |
|-------|---------|---------|--------|
| tauri | 2 | Desktop framework | Active |
| tauri-plugin-opener | 2 | Open URLs/files | Active |
| tauri-plugin-process | 2 | App relaunch | Active |
| tauri-plugin-updater | 2 | Auto-update | Active |
| btleplug | 0.11 | BLE (with serde feature) | Active — adapter detection, BLE scanning |
| serialport | 4 | USB serial | Declared, not yet used |
| chrono | 0.4 | Timestamps (with serde feature) | Active — BLE device last_seen |
| futures | 0.3 | Stream utilities | Active — BLE event streams |
| tokio | 1 (full) | Async runtime | Active |
| serde | 1 | Serialization | Active |
| serde_json | 1 | JSON | Active |
| thiserror | 2 | Error derivation | Active |

When adding a dependency, pin to a major version (e.g., `"1"` not `"1.2.3"`) unless a specific minor version is required. Always add the `features` array explicitly if you need non-default features.

## Code Quality

These are non-negotiable:
- `cargo build` must produce zero warnings
- All public types derive `Debug, Clone, Serialize, Deserialize` unless there's a specific reason not to
- Use `Result<T, AuracleError>` for all fallible operations — never panic in command handlers
- Prefer `String` over `&str` in struct fields that cross the IPC boundary (serialization owns the data)
- Write doc comments on public types and trait methods
- Extract shared logic into helper functions — if the same non-trivial block appears in two code paths (e.g., event-driven and polling fallback), factor it out. Duplicated conversion logic is a maintenance hazard.
- Minimise mutex lock scope — acquire the lock, do the work, release. Don't lock, release, then immediately re-lock for a follow-up read. Combine into one lock scope when the operations are sequential:

```rust
// Bad: two locks in quick succession
{
    let mut state = inner.lock().await;
    state.devices.insert(id, device);
} // released
{
    let state = inner.lock().await; // re-acquired immediately
    let devices: Vec<_> = state.devices.values().cloned().collect();
    app.emit("update", &devices);
}

// Good: single lock scope
{
    let mut state = inner.lock().await;
    state.devices.insert(id, device);
    let devices: Vec<_> = state.devices.values().cloned().collect();
    app.emit("update", &devices);
}
```

## Testing Pattern

Unit tests go in the same file as the code they test, in a `#[cfg(test)]` module:

```rust
#[cfg(test)]
mod tests {
    use super::*;

    #[tokio::test]
    async fn test_scan_returns_devices() {
        let devices = scan().await;
        assert!(!devices.is_empty());
    }
}
```

Integration tests that need the full Tauri app context go in `frontend/src-tauri/tests/`.

## Reference

For detailed type definitions, see `docs/data-models.md`.
For the full module map and command signatures, see `docs/backend.md`.
For architecture decisions and directory structure, see `docs/architecture.md`.
