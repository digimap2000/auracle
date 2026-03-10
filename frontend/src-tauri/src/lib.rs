mod ble;
mod daemon;
mod devices;
mod error;
mod serial;

use ble::{BleScanner, BluetoothAdapter};
use daemon::{DaemonCandidate, DaemonClient, DaemonUnit};
use devices::ConnectedDevice;
use error::AuracleError;
use serial::SerialPort;

#[tauri::command]
async fn get_bluetooth_adapter() -> Result<BluetoothAdapter, AuracleError> {
    ble::get_adapter().await
}

#[tauri::command]
async fn start_ble_scan(
    app: tauri::AppHandle,
    state: tauri::State<'_, BleScanner>,
) -> Result<(), AuracleError> {
    state.start_scan(app).await
}

#[tauri::command]
async fn stop_ble_scan(state: tauri::State<'_, BleScanner>) -> Result<(), AuracleError> {
    state.stop_scan().await
}

#[tauri::command]
async fn scan_serial_ports() -> Vec<SerialPort> {
    serial::scan().await
}

#[tauri::command]
async fn connect_device(device_id: String) -> Result<(), AuracleError> {
    // Stub: would look up device in registry and connect
    if device_id.is_empty() {
        return Err(AuracleError::DeviceNotFound(
            "No device ID provided".to_string(),
        ));
    }
    Ok(())
}

#[tauri::command]
async fn disconnect_device(device_id: String) -> Result<(), AuracleError> {
    // Stub: would look up device in registry and disconnect
    if device_id.is_empty() {
        return Err(AuracleError::DeviceNotFound(
            "No device ID provided".to_string(),
        ));
    }
    Ok(())
}

#[tauri::command]
async fn get_connected_devices() -> Vec<ConnectedDevice> {
    // Stub: would return actual connected devices from registry
    vec![]
}

#[tauri::command]
async fn get_daemon_units() -> Result<Vec<DaemonUnit>, AuracleError> {
    let mut client = DaemonClient::connect()
        .await
        .map_err(AuracleError::CommandFailed)?;
    client
        .list_units(false)
        .await
        .map_err(AuracleError::CommandFailed)
}

#[tauri::command]
async fn get_daemon_candidates() -> Result<Vec<DaemonCandidate>, AuracleError> {
    let mut client = DaemonClient::connect()
        .await
        .map_err(AuracleError::CommandFailed)?;
    client
        .list_candidates(false)
        .await
        .map_err(AuracleError::CommandFailed)
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
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
            get_daemon_candidates,
            get_daemon_units,
            scan_serial_ports,
            start_ble_scan,
            stop_ble_scan,
        ])
        .run(tauri::generate_context!())
        .expect("error while running Auracle");
}
