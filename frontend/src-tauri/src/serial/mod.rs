use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SerialPort {
    pub path: String,
    pub name: String,
    pub vendor_id: Option<u16>,
    pub product_id: Option<u16>,
}

/// Scan for serial ports. Returns mock data as a stub.
pub async fn scan() -> Vec<SerialPort> {
    // Stub: would use serialport crate to enumerate ports
    vec![SerialPort {
        path: "/dev/tty.usbmodem0001".to_string(),
        name: "nRF5340 Audio DK".to_string(),
        vendor_id: Some(0x1915),
        product_id: Some(0x5340),
    }]
}
