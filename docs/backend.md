# Backend

## Module Map

```
src-tauri/src/
├── main.rs              # Entry point: calls auracle_lib::run()
├── lib.rs               # Tauri app setup, command handlers, plugin registration
├── error.rs             # AuracleError enum
├── ble/
│   ├── mod.rs           # BluetoothAdapter, BleDevice, ManufacturerData structs, get_adapter()
│   └── scanner.rs       # BleScanner managed state, event-driven scan loop
├── serial/
│   └── mod.rs           # SerialPort struct, scan() function
└── devices/
    ├── mod.rs           # AuracleDevice trait, Command, Response, ConnectedDevice
    └── nrf5340.rs       # Nrf5340AudioDk implementation
```

## App Setup

**File**: `src-tauri/src/lib.rs`

The `run()` function builds the Tauri app:

```rust
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .plugin(tauri_plugin_process::init())
        .manage(BleScanner::new())
        .setup(|app| {
            #[cfg(desktop)]
            app.handle()
                .plugin(tauri_plugin_updater::Builder::new().build())?;
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            connect_device,
            disconnect_device,
            get_bluetooth_adapter,
            get_connected_devices,
            scan_serial_ports,
            start_ble_scan,
            stop_ble_scan,
        ])
        .run(tauri::generate_context!())
        .expect("error while running Auracle");
}
```

Note: The updater plugin is registered inside `.setup()` with a `#[cfg(desktop)]` guard because it uses the Builder pattern rather than `init()`.

## Tauri Commands

All commands are `async` and defined in `src-tauri/src/lib.rs`.

### get_bluetooth_adapter

```rust
#[tauri::command]
async fn get_bluetooth_adapter() -> Result<BluetoothAdapter, AuracleError>
```

Calls `ble::get_adapter()`. **Status**: Live — uses btleplug to detect the system Bluetooth adapter. Returns adapter info with `is_available: true` on success, or an actionable `AuracleError::Ble` message on failure (no adapter, permission denied).

### start_ble_scan

```rust
#[tauri::command]
async fn start_ble_scan(app: AppHandle, state: State<'_, BleScanner>) -> Result<(), AuracleError>
```

Starts continuous BLE scanning. Idempotent — returns `Ok(())` if already scanning. Spawns a background tokio task that uses btleplug's event stream to discover peripherals and emits `ble-devices-updated` events (debounced to ~2/sec). **Status**: Live.

### stop_ble_scan

```rust
#[tauri::command]
async fn stop_ble_scan(state: State<'_, BleScanner>) -> Result<(), AuracleError>
```

Stops BLE scanning. Idempotent — returns `Ok(())` if not scanning. Cancels the background scan task and calls `adapter.stop_scan()`. **Status**: Live.

### scan_serial_ports

```rust
#[tauri::command]
async fn scan_serial_ports() -> Vec<SerialPort>
```

Calls `serial::scan()`. **Status**: Returns mock data (1 nRF5340 on `/dev/tty.usbmodem0001`, vendor 0x1915, product 0x5340).

### connect_device

```rust
#[tauri::command]
async fn connect_device(device_id: String) -> Result<(), AuracleError>
```

**Status**: Stub. Validates that `device_id` is not empty (returns `DeviceNotFound` error if empty), otherwise returns `Ok(())`. No actual connection logic.

### disconnect_device

```rust
#[tauri::command]
async fn disconnect_device(device_id: String) -> Result<(), AuracleError>
```

**Status**: Stub. Same validation as `connect_device`. No actual disconnection logic.

### get_connected_devices

```rust
#[tauri::command]
async fn get_connected_devices() -> Vec<ConnectedDevice>
```

**Status**: Stub. Always returns an empty `Vec`.

## Error Handling

**File**: `src-tauri/src/error.rs`

`AuracleError` derives `thiserror::Error` and implements a custom `Serialize` that converts to the error's `Display` string. This allows Tauri to return errors as serialized strings to the frontend.

Variants:

| Variant | Message format | When used |
|---------|---------------|-----------|
| `Ble(String)` | "BLE error: {0}" | BLE-related failures |
| `Serial(String)` | "Serial error: {0}" | Serial port failures |
| `DeviceNotFound(String)` | "Device not found: {0}" | Device lookup failures |
| `ConnectionFailed(String)` | "Connection failed: {0}" | Connection establishment failures |
| `CommandFailed(String)` | "Command failed: {0}" | Device command failures |

## BLE Module

**Files**: `src-tauri/src/ble/mod.rs`, `src-tauri/src/ble/scanner.rs`

### Types (mod.rs)

- `BluetoothAdapter` — system adapter info (see data-models.md)
- `BleDevice` — discovered peripheral with full advertisement data (see data-models.md)
- `ManufacturerData` — company ID + raw bytes (see data-models.md)

### Functions (mod.rs)

- `get_adapter() -> Result<BluetoothAdapter, AuracleError>` — detects the system Bluetooth adapter via btleplug

### BleScanner (scanner.rs)

Tauri managed state for continuous BLE scanning. Registered via `.manage(BleScanner::new())`.

```rust
pub struct BleScanner {
    inner: Arc<tokio::sync::Mutex<ScannerState>>,
}
```

Methods:
- `start_scan(app: AppHandle)` — acquires adapter, calls `adapter.start_scan()`, spawns background task
- `stop_scan()` — cancels background task, calls `adapter.stop_scan()`

The scan loop prefers btleplug's event stream (`adapter.events()`) for efficiency. Falls back to periodic polling (~1s) if the event stream isn't available. Emits `ble-devices-updated` Tauri events debounced to at most ~2 per second.

**Status**: Live. All functions use real btleplug calls.

Error conversion: `impl From<btleplug::Error> for AuracleError` maps btleplug errors to the `Ble` variant (defined in `error.rs`).

## Serial Module

**File**: `src-tauri/src/serial/mod.rs`

Exports:
- `SerialPort` struct (see data-models.md)
- `scan() -> Vec<SerialPort>` — async function

**Status**: `scan()` returns hardcoded mock data. Ready for `serialport` crate integration.

## Device Abstraction

**File**: `src-tauri/src/devices/mod.rs`

Defines the `AuracleDevice` trait and supporting types (see data-models.md for type definitions).

The trait uses `#[allow(async_fn_in_trait)]` to permit async methods without `dyn` compatibility concerns (only used with concrete types).

### Nrf5340AudioDk

**File**: `src-tauri/src/devices/nrf5340.rs`

```rust
pub struct Nrf5340AudioDk {
    pub id: String,
    pub name: String,      // Auto-formatted as "nRF5340 Audio DK ({id})"
    connected: bool,       // Private — tracks connection state
}
```

Constructor: `Nrf5340AudioDk::new(id: String)`

**Status**: Stub implementation. `connect()` sets `connected = true`. `send_command()` checks connection state, returns echo response. `disconnect()` sets `connected = false`. No real BLE/serial I/O.

## Managed State

| State | Registration | Notes |
|-------|-------------|-------|
| `BleScanner` | `.manage(BleScanner::new())` | BLE scan lifecycle and device map |

## Plugin Registration

Plugins are registered in `lib.rs`:

| Plugin | Registration | Notes |
|--------|-------------|-------|
| `tauri_plugin_opener` | `.plugin(init())` | Always active |
| `tauri_plugin_process` | `.plugin(init())` | Always active |
| `tauri_plugin_updater` | `.setup()` with `#[cfg(desktop)]` | Desktop only, uses Builder pattern |

## Patterns

### Adding a new Tauri command

1. Define the command function in `src-tauri/src/lib.rs`:
   ```rust
   #[tauri::command]
   async fn my_command(param: String) -> Result<ReturnType, AuracleError> {
       // implementation
   }
   ```
2. Register it in the `generate_handler!` macro invocation
3. Add the corresponding TypeScript wrapper in `src/lib/tauri.ts`:
   ```typescript
   export async function myCommand(param: string): Promise<ReturnType> {
     return invoke<ReturnType>("my_command", { param });
   }
   ```
4. If a new type is needed, define the Rust struct with `#[derive(Serialize, Deserialize)]` and a matching TypeScript interface

### Adding a new device implementation

1. Create `src-tauri/src/devices/my_device.rs`
2. Define a struct and implement `AuracleDevice` for it
3. Add `pub mod my_device;` to `src-tauri/src/devices/mod.rs`

### Adding a new error variant

1. Add the variant to `AuracleError` in `src-tauri/src/error.rs` with a `#[error("...")]` attribute
2. The custom `Serialize` impl handles it automatically
