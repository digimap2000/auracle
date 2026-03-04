use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BleDevice {
    pub id: String,
    pub name: String,
    pub rssi: i16,
    pub is_connected: bool,
}

/// Scan for BLE devices. Returns mock data as a stub.
pub async fn scan() -> Vec<BleDevice> {
    // Stub: would use btleplug to scan for BLE peripherals
    vec![
        BleDevice {
            id: "AA:BB:CC:DD:EE:01".to_string(),
            name: "nRF5340 Audio DK".to_string(),
            rssi: -42,
            is_connected: false,
        },
        BleDevice {
            id: "AA:BB:CC:DD:EE:02".to_string(),
            name: "nRF5340 Audio DK (2)".to_string(),
            rssi: -58,
            is_connected: false,
        },
    ]
}
