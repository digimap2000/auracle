# Feature 001: Bluetooth Adapter Detection

## Summary

Detect the host machine's Bluetooth adapter using btleplug and surface its availability throughout the UI. This is the first real hardware integration — it replaces mock data in the BLE module with actual system calls and establishes the pattern for all future hardware features.

## Motivation

Before Auracle can scan for BLE peripherals or monitor Auracast streams, it needs to know whether a Bluetooth adapter exists and is accessible. The adapter's presence or absence affects every downstream feature, so it must be detected early and communicated clearly.

## Requirements

### Backend (Rust)

#### New type: `BluetoothAdapter`

File: `src-tauri/src/ble/mod.rs`

```rust
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BluetoothAdapter {
    pub id: String,              // System identifier (e.g. "CoreBluetooth" on macOS)
    pub name: String,            // Human-readable name (e.g. "Built-in Bluetooth")
    pub is_available: bool,      // Whether the adapter is powered on and accessible
}
```

#### New function: `ble::get_adapter()`

File: `src-tauri/src/ble/mod.rs`

```rust
pub async fn get_adapter() -> Result<BluetoothAdapter, AuracleError>
```

Uses btleplug's `Manager::new()` and `manager.adapters()` to detect the system Bluetooth adapter.

Behaviour:
- If an adapter is found and accessible → return `BluetoothAdapter` with `is_available: true`
- If no adapter is found → return `AuracleError::Ble("No Bluetooth adapter found — check that Bluetooth is enabled in System Settings")`
- If btleplug returns a permission error → return `AuracleError::Ble("Bluetooth permission denied — enable Auracle in System Settings > Privacy & Security > Bluetooth")`
- Map btleplug errors to `AuracleError::Ble` via a `From` impl

#### New error conversion

File: `src-tauri/src/error.rs`

Add `impl From<btleplug::Error> for AuracleError` that maps to the `Ble` variant.

#### New Tauri command: `get_bluetooth_adapter`

File: `src-tauri/src/lib.rs`

```rust
#[tauri::command]
async fn get_bluetooth_adapter() -> Result<BluetoothAdapter, AuracleError> {
    ble::get_adapter().await
}
```

Register in `generate_handler!`.

### Frontend (TypeScript)

#### New type: `BluetoothAdapter`

File: `src/lib/tauri.ts`

```typescript
export interface BluetoothAdapter {
  id: string;
  name: string;
  is_available: boolean;
}
```

#### New invoke wrapper

File: `src/lib/tauri.ts`

```typescript
export async function getBluetoothAdapter(): Promise<BluetoothAdapter> {
  return invoke<BluetoothAdapter>("get_bluetooth_adapter");
}
```

#### New hook: `useBluetoothAdapter`

File: `src/hooks/useBluetoothAdapter.ts`

State:
```typescript
interface BluetoothAdapterState {
  adapter: BluetoothAdapter | null;
  loading: boolean;
  error: string | null;
}
```

Behaviour:
- On mount, call `getBluetoothAdapter()` once
- Expose a `refresh()` function for manual retry
- Return `{ adapter, loading, error, refresh }`

#### Dashboard changes

File: `src/pages/Dashboard.tsx`

Add a third stat card: **Bluetooth Adapter**
- When loading: show a Skeleton
- When adapter is available: show adapter name, green "Available" badge
- When adapter is unavailable or errored: show the error message, red "Unavailable" badge, and a Retry button
- Use `Bluetooth` icon from Lucide at 14px

#### StatusBar changes

File: `src/components/layout/StatusBar.tsx`

Add a Bluetooth indicator to the left section (before the device count):
- Small Bluetooth icon (12px) that is `text-primary` when adapter is available, `text-muted-foreground/30` when not
- Tooltip on hover showing the adapter name or error message

### IPC Contract Addition

| TypeScript function | Tauri command | Parameters | Return type |
|--------------------|---------------|------------|-------------|
| `getBluetoothAdapter()` | `get_bluetooth_adapter` | none | `BluetoothAdapter` |

### Spec Updates Required

After implementation, update:
- `docs/data-models.md` — add `BluetoothAdapter` type
- `docs/backend.md` — add `get_bluetooth_adapter` command, update BLE module status
- `docs/frontend.md` — add `useBluetoothAdapter` hook, update Dashboard and StatusBar sections

## Acceptance Criteria

1. `cargo build` completes with zero warnings
2. `npm run build` completes with zero TypeScript errors
3. The `get_bluetooth_adapter` command returns real adapter info on a machine with Bluetooth
4. The `get_bluetooth_adapter` command returns a meaningful error on a machine without Bluetooth or with Bluetooth disabled
5. The Dashboard shows the adapter status card
6. The StatusBar shows the Bluetooth indicator
7. All error states display actionable messages (not generic "something went wrong")
8. The `docs/` specs are updated to reflect the new types, commands, and UI
