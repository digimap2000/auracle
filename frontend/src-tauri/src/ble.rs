use serde::{Deserialize, Serialize};

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
