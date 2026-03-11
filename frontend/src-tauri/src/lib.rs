mod ble;
mod daemon;
mod devices;
mod error;
mod serial;

use ble::BluetoothAdapter;
use daemon::scan_bridge::DaemonScanBridge;
use daemon::{DaemonCandidate, DaemonClient, DaemonUnit};
use devices::ConnectedDevice;
use error::AuracleError;
use serial::SerialPort;

#[tauri::command]
async fn get_bluetooth_adapter() -> Result<BluetoothAdapter, AuracleError> {
    // Query daemon for host bluetooth adapter unit
    let mut client = DaemonClient::connect()
        .await
        .map_err(AuracleError::ConnectionFailed)?;
    let units = client
        .list_units(false)
        .await
        .map_err(AuracleError::CommandFailed)?;

    let bt_unit = units.into_iter().find(|u| u.kind == "host-bluetooth");
    match bt_unit {
        Some(unit) => Ok(BluetoothAdapter {
            id: unit.id,
            name: unit.product,
            is_available: unit.present,
        }),
        None => Err(AuracleError::ConnectionFailed(
            "No Bluetooth adapter found — ensure auracle-daemon is running".to_string(),
        )),
    }
}

#[tauri::command]
async fn start_ble_scan(
    app: tauri::AppHandle,
    state: tauri::State<'_, DaemonScanBridge>,
) -> Result<(), AuracleError> {
    // Find the host bluetooth unit to scan on
    let mut client = DaemonClient::connect()
        .await
        .map_err(AuracleError::ConnectionFailed)?;
    let units = client
        .list_units(false)
        .await
        .map_err(AuracleError::CommandFailed)?;

    let bt_unit = units.into_iter().find(|u| u.kind == "host-bluetooth");
    match bt_unit {
        Some(unit) => state.start_scan(app, unit.id).await,
        None => Err(AuracleError::ConnectionFailed(
            "No Bluetooth adapter unit found — ensure auracle-daemon is running".to_string(),
        )),
    }
}

#[tauri::command]
async fn stop_ble_scan(
    app: tauri::AppHandle,
    state: tauri::State<'_, DaemonScanBridge>,
) -> Result<(), AuracleError> {
    // Find the host bluetooth unit to stop scanning on
    let mut client = DaemonClient::connect()
        .await
        .map_err(AuracleError::ConnectionFailed)?;
    let units = client
        .list_units(false)
        .await
        .map_err(AuracleError::CommandFailed)?;

    let bt_unit = units.into_iter().find(|u| u.kind == "host-bluetooth");
    match bt_unit {
        Some(unit) => state.stop_scan(app, unit.id).await,
        None => Ok(()),
    }
}

#[tauri::command]
async fn scan_serial_ports() -> Vec<SerialPort> {
    serial::scan().await
}

#[tauri::command]
async fn connect_device(device_id: String) -> Result<(), AuracleError> {
    if device_id.is_empty() {
        return Err(AuracleError::DeviceNotFound(
            "No device ID provided".to_string(),
        ));
    }
    Ok(())
}

#[tauri::command]
async fn disconnect_device(device_id: String) -> Result<(), AuracleError> {
    if device_id.is_empty() {
        return Err(AuracleError::DeviceNotFound(
            "No device ID provided".to_string(),
        ));
    }
    Ok(())
}

#[tauri::command]
async fn get_connected_devices() -> Vec<ConnectedDevice> {
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
async fn start_daemon_scan(
    app: tauri::AppHandle,
    state: tauri::State<'_, DaemonScanBridge>,
    unit_id: String,
) -> Result<(), AuracleError> {
    state.start_scan(app, unit_id).await
}

#[tauri::command]
async fn stop_daemon_scan(
    app: tauri::AppHandle,
    state: tauri::State<'_, DaemonScanBridge>,
    unit_id: String,
) -> Result<(), AuracleError> {
    state.stop_scan(app, unit_id).await
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
        .manage(DaemonScanBridge::new())
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
            start_daemon_scan,
            stop_ble_scan,
            stop_daemon_scan,
        ])
        .run(tauri::generate_context!())
        .expect("error while running Auracle");
}
