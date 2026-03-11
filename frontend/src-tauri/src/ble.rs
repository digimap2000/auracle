use std::collections::HashMap;

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
    pub service_labels: HashMap<String, String>,
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
    pub service_labels: HashMap<String, String>,
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

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DecodedServiceData {
    pub service_uuid: String,
    pub service_label: String,
    pub raw_value: String,
    pub status_code: u32,
    pub status_message: String,
    pub fields: Vec<DecodedField>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ServiceDataFormatMetadata {
    pub service_uuid: String,
    pub service_label: String,
    pub service_description: String,
    pub status_code: u32,
    pub status_message: String,
    pub fields: Vec<ServiceDataFieldMetadata>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ServiceDataFieldMetadata {
    pub field: String,
    pub r#type: String,
    pub enum_match: String,
    pub enum_entries: Vec<ServiceDataEnumEntryMetadata>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ServiceDataEnumEntryMetadata {
    pub value: u32,
    pub short_name: String,
    pub description: String,
    pub implications: Vec<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DecodedField {
    pub field: String,
    pub r#type: String,
    pub value: String,
}

/// Manufacturer-specific advertisement data.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ManufacturerData {
    pub company_id: u16,
    pub data: Vec<u8>,
}
