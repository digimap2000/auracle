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
    pub unit_id: String,
    pub stable_id: String,
    pub id: String,
    pub name: String,
    pub rssi: i16,
    pub is_connected: bool,
    pub tx_power: Option<i16>,
    pub services: Vec<String>,
    pub manufacturer_data: Vec<ManufacturerData>,
    pub last_seen: String,
}

/// A single BLE advertisement packet observation.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BlePacket {
    pub unit_id: String,
    pub stable_id: String,
    pub id: String,
    pub device_id: String,
    pub name: String,
    pub rssi: i16,
    pub tx_power: Option<i16>,
    pub service_uuids: Vec<String>,
    pub company_id: Option<u16>,
    pub company_data: Vec<u8>,
    pub address_type: String,
    pub sid: u32,
    pub adv_type: u32,
    pub adv_props: u32,
    pub interval: u32,
    pub primary_phy: u32,
    pub secondary_phy: u32,
    pub raw_data: Vec<u8>,
    pub raw_scan_response: Vec<u8>,
    pub timestamp_ms: i64,
}

/// Manufacturer-specific advertisement data.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ManufacturerData {
    pub company_id: u16,
    pub data: Vec<u8>,
}
