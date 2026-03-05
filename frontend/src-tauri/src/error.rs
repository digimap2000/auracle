use serde::Serialize;

#[derive(Debug, thiserror::Error)]
#[allow(dead_code)]
pub enum AuracleError {
    #[error("BLE error: {0}")]
    Ble(String),

    #[error("Serial error: {0}")]
    Serial(String),

    #[error("Device not found: {0}")]
    DeviceNotFound(String),

    #[error("Connection failed: {0}")]
    ConnectionFailed(String),

    #[error("Command failed: {0}")]
    CommandFailed(String),
}

impl From<btleplug::Error> for AuracleError {
    fn from(e: btleplug::Error) -> Self {
        AuracleError::Ble(e.to_string())
    }
}

impl Serialize for AuracleError {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        serializer.serialize_str(&self.to_string())
    }
}
