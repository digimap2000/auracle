# Data Models

All types are defined in both TypeScript (frontend) and Rust (backend) with matching field names. The Tauri IPC bridge serializes Rust structs to JSON which maps to the TypeScript interfaces.

## BluetoothAdapter

The host machine's Bluetooth adapter, detected via btleplug.

**TypeScript** (`src/lib/tauri.ts`):
```typescript
interface BluetoothAdapter {
  id: string;           // System identifier (e.g. "CoreBluetooth" on macOS)
  name: string;         // Human-readable name
  is_available: boolean; // Whether the adapter is powered on and accessible
}
```

**Rust** (`src-tauri/src/ble/mod.rs`):
```rust
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BluetoothAdapter {
    pub id: String,
    pub name: String,
    pub is_available: bool,
}
```

## BleDevice

A BLE peripheral discovered during scanning. Updated live via the `ble-devices-updated` event.

**TypeScript** (`src/lib/tauri.ts`):
```typescript
interface BleDevice {
  id: string;                        // MAC address or UUID
  name: string;                      // Advertised name, or "Unknown" if none
  rssi: number;                      // Signal strength in dBm (negative integer)
  is_connected: boolean;             // Whether currently connected
  tx_power: number | null;           // Transmit power level if advertised
  services: string[];                // Advertised service UUIDs as strings
  manufacturer_data: ManufacturerData[]; // Manufacturer-specific data
  last_seen: string;                 // ISO 8601 timestamp of last advertisement
}
```

**Rust** (`src-tauri/src/ble/mod.rs`):
```rust
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BleDevice {
    pub id: String,
    pub name: String,
    pub rssi: i16,
    pub is_connected: bool,
    pub tx_power: Option<i16>,
    pub services: Vec<String>,
    pub manufacturer_data: Vec<ManufacturerData>,
    pub last_seen: String,
}
```

## ManufacturerData

Manufacturer-specific advertisement data from a BLE peripheral.

**TypeScript** (`src/lib/tauri.ts`):
```typescript
interface ManufacturerData {
  company_id: number;   // Bluetooth SIG company identifier
  data: number[];       // Raw data bytes
}
```

**Rust** (`src-tauri/src/ble/mod.rs`):
```rust
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ManufacturerData {
    pub company_id: u16,
    pub data: Vec<u8>,
}
```

## SerialPort

A serial port enumerated on the system.

**TypeScript** (`src/lib/tauri.ts`):
```typescript
interface SerialPort {
  path: string;              // System path (e.g. "/dev/tty.usbmodem0001")
  name: string;              // Human-readable name
  vendor_id: number | null;  // USB vendor ID (e.g. 0x1915 for Nordic)
  product_id: number | null; // USB product ID (e.g. 0x5340)
}
```

**Rust** (`src-tauri/src/serial/mod.rs`):
```rust
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SerialPort {
    pub path: String,
    pub name: String,
    pub vendor_id: Option<u16>,
    pub product_id: Option<u16>,
}
```

## ConnectedDevice

A device that has an active connection.

**TypeScript** (`src/lib/tauri.ts`):
```typescript
interface ConnectedDevice {
  id: string;               // Unique device identifier
  name: string;             // Display name
  device_type: string;      // Device type identifier
  firmware_version: string; // Firmware version string
}
```

**Rust** (`src-tauri/src/devices/mod.rs`):
```rust
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ConnectedDevice {
    pub id: String,
    pub name: String,
    pub device_type: String,
    pub firmware_version: String,
}
```

## Command

A command to send to a connected device.

**Rust only** (`src-tauri/src/devices/mod.rs`):
```rust
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Command {
    pub opcode: u8,        // Command opcode
    pub payload: Vec<u8>,  // Command payload bytes
}
```

Not exposed to the frontend. Used internally by `AuracleDevice` trait implementations.

## Response

A response received from a connected device.

**Rust only** (`src-tauri/src/devices/mod.rs`):
```rust
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Response {
    pub status: u8,        // Response status code (0 = success)
    pub payload: Vec<u8>,  // Response payload bytes
}
```

Not exposed to the frontend. Used internally by `AuracleDevice` trait implementations.

## AuracleError

Application error type used across all Tauri commands.

**Rust** (`src-tauri/src/error.rs`):
```rust
#[derive(Debug, thiserror::Error)]
pub enum AuracleError {
    #[error("BLE error: {0}")]
    Ble(String),

    #[error("Serial error: {0}")]
    Serial(String),

    #[error("Device not found: {0}")]
    DeviceNotFound(String),

    #[error("Connection failed: {0}")]
    ConnectionFailed(String),

    #[error("Command failed: {0}")]
    CommandFailed(String),
}
```

Serialized to a string via a custom `Serialize` impl that calls `self.to_string()`. On the frontend, errors from Tauri commands arrive as string messages.

## AuracleDevice Trait

Async trait for device communication. All device implementations must implement this.

**Rust** (`src-tauri/src/devices/mod.rs`):
```rust
pub trait AuracleDevice {
    async fn connect(&mut self) -> Result<(), AuracleError>;
    async fn send_command(&mut self, cmd: Command) -> Result<Response, AuracleError>;
    async fn disconnect(&mut self) -> Result<(), AuracleError>;
}
```

### Implementations

| Struct | File | Status |
|--------|------|--------|
| `Nrf5340AudioDk` | `src-tauri/src/devices/nrf5340.rs` | Stub — sets `connected` flag but no real I/O |

## IPC Contract

Mapping between frontend invoke wrappers and backend command handlers:

| TypeScript function | Tauri command | Parameters | Return type |
|--------------------|---------------|------------|-------------|
| `getBluetoothAdapter()` | `get_bluetooth_adapter` | none | `BluetoothAdapter` |
| `startBleScan()` | `start_ble_scan` | none | `void` |
| `stopBleScan()` | `stop_ble_scan` | none | `void` |
| `scanSerialPorts()` | `scan_serial_ports` | none | `SerialPort[]` |
| `connectDevice(deviceId)` | `connect_device` | `{ deviceId: string }` | `void` |
| `disconnectDevice(deviceId)` | `disconnect_device` | `{ deviceId: string }` | `void` |
| `getConnectedDevices()` | `get_connected_devices` | none | `ConnectedDevice[]` |

### Events

| Event name | Payload | Direction |
|-----------|---------|-----------|
| `ble-devices-updated` | `BleDevice[]` | Backend → Frontend |

Frontend wrappers are in `src/lib/tauri.ts`. Backend handlers are in `src-tauri/src/lib.rs`.

## Frontend-only Types

### UpdateStatus (`src/hooks/useUpdater.ts`)

Discriminated union for update state machine:

```typescript
type UpdateStatus =
  | { state: "idle" }
  | { state: "checking" }
  | { state: "available"; update: Update }
  | { state: "downloading"; progress: number }
  | { state: "ready" }
  | { state: "error"; message: string };
```

`Update` is imported from `@tauri-apps/plugin-updater`.

### BluetoothAdapterState (`src/hooks/useBluetoothAdapter.ts`)

```typescript
interface BluetoothAdapterState {
  adapter: BluetoothAdapter | null;
  loading: boolean;
  error: string | null;
}
```

### DevicesState (`src/hooks/useDevices.ts`)

```typescript
interface DevicesState {
  bleDevices: BleDevice[];
  serialPorts: SerialPort[];
  connectedDevices: ConnectedDevice[];
  scanning: boolean;
  error: string | null;
}
```

### LogEntry (`src/pages/Logs.tsx`)

```typescript
interface LogEntry {
  timestamp: string; // ISO 8601 string
  level: "info" | "warn" | "error";
  message: string;
}
```
