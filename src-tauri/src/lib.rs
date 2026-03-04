mod ble;
mod devices;
mod error;
mod serial;

use ble::BleDevice;
use devices::ConnectedDevice;
use error::AuracleError;
use serial::SerialPort;

#[tauri::command]
async fn scan_ble_devices() -> Vec<BleDevice> {
    ble::scan().await
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

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .plugin(tauri_plugin_process::init())
        .setup(|app| {
            #[cfg(desktop)]
            app.handle()
                .plugin(tauri_plugin_updater::Builder::new().build())?;
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            scan_ble_devices,
            scan_serial_ports,
            connect_device,
            disconnect_device,
            get_connected_devices,
        ])
        .run(tauri::generate_context!())
        .expect("error while running Auracle");
}
