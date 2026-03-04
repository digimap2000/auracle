---
name: auracle-devices
description: "Device integration for Auracle — BLE adapters, serial ports, nRF5340 Audio DK, and the AuracleDevice trait. Use this skill whenever working on hardware communication: Bluetooth adapter detection, BLE scanning with btleplug, serial port enumeration, device connection/disconnection, the device registry, nRF5340 UART protocol, or adding support for new hardware. Also trigger when working on the Devices page UI or anything that touches the devices/ module."
---

# Auracle Device Integration

This skill covers all hardware communication in Auracle — Bluetooth adapters, BLE scanning, serial ports, device connections, and the abstraction layer that makes adding new hardware straightforward.

Read `docs/data-models.md` for exact type definitions and `docs/backend.md` for the current module map before starting work.

## Hardware Targets

### Local Bluetooth Adapter (first-class device)

The host machine's built-in Bluetooth is always present in the device list if the OS grants permission. It's not an external device — it's the machine itself.

- **macOS**: Bluetooth 5.3 via CoreBluetooth, accessed through btleplug. Requires `NSBluetoothAlwaysUsageDescription` in `Info.plist` (in `src-tauri/Info.plist`).
- **Windows**: WinRT Bluetooth APIs via btleplug, capabilities discovered at runtime.
- **Primary use**: Verify that streams broadcast by external devices (like the nRF5340) are actually on-air and correctly configured.
- **Permission handling**: If the OS denies Bluetooth access, surface this clearly in the UI — never crash or silently fail. The error message should tell the user exactly where to go to fix it (e.g., "Bluetooth permission denied — enable in System Settings > Privacy & Security > Bluetooth").

### Naim Products (Muso 3, Bathys V2)

Naim's products use an IDC777 Bluetooth module (IOT747 chipset) controlled over UART from a SAM4E main processor. Each physical device advertises **two separate BLE entities**:

1. **Auracast entity** (e.g., "Mu-so 0012") — advertises under Vervent Audio Group company ID (`0x0BA4`) with custom service UUID `3e1d50cd-7e3e-427d-8e1c-b78aa87fe624` ("Focal Naim Auracast"). Carries Auracast broadcast configuration.
2. **Classic Bluetooth entity** (e.g., "Muso3-880012") — advertises under Microsoft company ID (`0x0006`) from the Stream Unlimited audio module's BLE stack. This is the module vendor's ID, not a bug.

The number suffix in both names appears to be a shared device identifier (serial number). This could be used to correlate the two BLE entities as belonging to the same physical device.

The IDC777 firmware source is in the `sam4e` repository at `BSP/src/devices/bluetooth/IDC777/`.

### nRF5340 Audio DK

Nordic's reference board for LE Audio / Auracast development. Dual-core (app + network).

- **USB connection**: CDC-ACM serial/UART as primary control channel. Appears as `/dev/tty.usbmodem*` on macOS. Nordic VID: `0x1915`, PID: `0x5340`.
- **BLE connection**: Wireless control and stream monitoring.
- **Serial output**: Log output can be interleaved from both cores — the parser must handle this.
- **Firmware**: Runs Nordic nRF Connect SDK with a specific UART command protocol.
- **Capability discovery**: At connect time, query the device for its capabilities (e.g., max concurrent streams). Don't hardcode limits.

## The AuracleDevice Trait

All device implementations implement this async trait, defined in `src-tauri/src/devices/mod.rs`:

```rust
pub trait AuracleDevice {
    async fn connect(&mut self) -> Result<(), AuracleError>;
    async fn send_command(&mut self, cmd: Command) -> Result<Response, AuracleError>;
    async fn disconnect(&mut self) -> Result<(), AuracleError>;
}
```

The trait uses `#[allow(async_fn_in_trait)]` because we only use it with concrete types (no `dyn AuracleDevice`). If dynamic dispatch becomes necessary later, switch to the `async-trait` crate.

Supporting types:

```rust
pub struct Command {
    pub opcode: u8,
    pub payload: Vec<u8>,
}

pub struct Response {
    pub status: u8,       // 0 = success
    pub payload: Vec<u8>,
}

pub struct ConnectedDevice {
    pub id: String,
    pub name: String,
    pub device_type: String,
    pub firmware_version: String,
}
```

`Command` and `Response` are internal to the Rust backend — they don't cross the IPC boundary.

## Adding a New Device

1. Create `src-tauri/src/devices/my_device.rs`
2. Define a struct with at minimum: an `id`, a `name`, and a `connected` state
3. Implement `AuracleDevice` for it
4. Register the module in `src-tauri/src/devices/mod.rs`: `pub mod my_device;`
5. Add it to the device registry (when the registry exists)

The struct should handle its own connection details internally. The rest of the app interacts with it only through the trait.

```rust
pub struct MyDevice {
    pub id: String,
    pub name: String,
    connected: bool,
    // device-specific fields: port handle, BLE peripheral, etc.
}

impl MyDevice {
    pub fn new(id: String) -> Self {
        Self {
            name: format!("My Device ({})", id),
            id,
            connected: false,
        }
    }
}

impl AuracleDevice for MyDevice {
    async fn connect(&mut self) -> Result<(), AuracleError> {
        // Real connection logic here
        self.connected = true;
        Ok(())
    }

    async fn send_command(&mut self, cmd: Command) -> Result<Response, AuracleError> {
        if !self.connected {
            return Err(AuracleError::ConnectionFailed(
                "not connected".to_string()
            ));
        }
        // Send command, get response
        todo!()
    }

    async fn disconnect(&mut self) -> Result<(), AuracleError> {
        self.connected = false;
        Ok(())
    }
}
```

## BLE with btleplug

btleplug is the cross-platform BLE library. It's already in Cargo.toml with `features = ["serde"]`.

Key patterns for btleplug usage:

```rust
use btleplug::api::{Central, Manager as _, Peripheral as _, ScanFilter};
use btleplug::platform::Manager;

// Get the adapter
let manager = Manager::new().await?;
let adapters = manager.adapters().await?;
let adapter = adapters.into_iter().next()
    .ok_or(AuracleError::Ble("no Bluetooth adapter found".to_string()))?;

// Get adapter info
let info = adapter.adapter_info().await?;

// Scan for peripherals
adapter.start_scan(ScanFilter::default()).await?;
tokio::time::sleep(Duration::from_secs(5)).await;
let peripherals = adapter.peripherals().await?;
```

Error mapping — btleplug errors should be converted to `AuracleError::Ble`:

```rust
impl From<btleplug::Error> for AuracleError {
    fn from(e: btleplug::Error) -> Self {
        AuracleError::Ble(e.to_string())
    }
}
```

### macOS Bluetooth Permissions

On macOS, the first btleplug call triggers a system permission dialog. If the user denies it, subsequent calls will fail. The app must:

1. Detect the permission denial (btleplug will return an error)
2. Surface it with a helpful message
3. Not retry in a loop — wait for the user to take action

The `Info.plist` in `src-tauri/` must contain:
```xml
<key>NSBluetoothAlwaysUsageDescription</key>
<string>Auracle needs Bluetooth to discover and communicate with audio devices</string>
```

## Serial with serialport

The `serialport` crate handles USB serial enumeration and communication. Also already in Cargo.toml.

```rust
use serialport;

// Enumerate ports
let ports = serialport::available_ports()
    .map_err(|e| AuracleError::Serial(e.to_string()))?;

// Filter for Nordic devices
let nordic_ports: Vec<_> = ports.into_iter()
    .filter(|p| {
        if let serialport::SerialPortType::UsbPort(info) = &p.port_type {
            info.vid == 0x1915  // Nordic Semiconductor
        } else {
            false
        }
    })
    .collect();
```

## Device Registry (not yet built)

The device registry will be a Tauri-managed state that tracks all known and connected devices. When implementing it:

- Use `Arc<tokio::sync::Mutex<HashMap<String, Box<dyn AuracleDevice + Send>>>>` or a dedicated struct wrapping this
- Register it via `.manage()` in `lib.rs`
- Access it in commands via `State<'_, DeviceRegistry>`
- Emit events when devices connect/disconnect so the frontend can react in real-time

## Frontend Integration

The Devices page (`src/pages/Devices.tsx`) and the `useDevices` hook (`src/hooks/useDevices.ts`) are the frontend counterparts. The hook calls:

| Function | Command | Returns |
|----------|---------|---------|
| `startBleScan()` | `start_ble_scan` | `void` |
| `stopBleScan()` | `stop_ble_scan` | `void` |
| `scanSerialPorts()` | `scan_serial_ports` | `SerialPort[]` |
| `connectDevice(id)` | `connect_device` | `void` |
| `disconnectDevice(id)` | `disconnect_device` | `void` |
| `getConnectedDevices()` | `get_connected_devices` | `ConnectedDevice[]` |

The BLE scanner emits `ble-devices-updated` events with the full device list. The hook listens to this event and updates state reactively — no polling. The hook also stops scanning on unmount.

When adding new device-related commands, follow the same pattern: Rust command → TS wrapper → hook integration.

## Test Tool Principle

Auracle is a test harness for firmware developers. **Never hide, deduplicate, or sanitise data reported by devices.** If a device advertises the same service UUID five times, show it five times. If manufacturer data contains unexpected values, display them. Bugs in device firmware are what this tool exists to find.

## Reference

Read `references/btleplug-api.md` for btleplug API details (create this when starting BLE work).
Read `references/nrf5340-protocol.md` for the UART command protocol (create this when starting nRF5340 work).
