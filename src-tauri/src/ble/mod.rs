pub mod scanner;

use btleplug::api::{Central, Manager as _};
use btleplug::platform::Manager;
use serde::{Deserialize, Serialize};

use crate::error::AuracleError;

pub use scanner::BleScanner;

/// The host machine's Bluetooth adapter.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BluetoothAdapter {
    pub id: String,
    pub name: String,
    pub is_available: bool,
}

/// A BLE peripheral discovered during scanning.
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

/// Manufacturer-specific advertisement data.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ManufacturerData {
    pub company_id: u16,
    pub data: Vec<u8>,
}

/// Detect the system Bluetooth adapter via btleplug.
pub async fn get_adapter() -> Result<BluetoothAdapter, AuracleError> {
    let manager = Manager::new().await.map_err(|e| {
        let msg = e.to_string();
        if msg.contains("permission") || msg.contains("denied") || msg.contains("unauthorized") {
            AuracleError::Ble(
                "Bluetooth permission denied — enable Auracle in System Settings > Privacy & Security > Bluetooth".to_string(),
            )
        } else {
            AuracleError::from(e)
        }
    })?;

    let adapters = manager.adapters().await.map_err(|e| {
        let msg = e.to_string();
        if msg.contains("permission") || msg.contains("denied") || msg.contains("unauthorized") {
            AuracleError::Ble(
                "Bluetooth permission denied — enable Auracle in System Settings > Privacy & Security > Bluetooth".to_string(),
            )
        } else {
            AuracleError::from(e)
        }
    })?;

    let adapter = adapters
        .into_iter()
        .next()
        .ok_or_else(|| {
            AuracleError::Ble(
                "No Bluetooth adapter found — check that Bluetooth is enabled in System Settings"
                    .to_string(),
            )
        })?;

    let info = adapter.adapter_info().await?;

    Ok(BluetoothAdapter {
        id: info.clone(),
        name: info,
        is_available: true,
    })
}
