use super::{AuracleDevice, Command, Response};
use crate::error::AuracleError;

pub struct Nrf5340AudioDk {
    pub id: String,
    pub name: String,
    connected: bool,
}

impl Nrf5340AudioDk {
    pub fn new(id: String) -> Self {
        Self {
            id: id.clone(),
            name: format!("nRF5340 Audio DK ({})", id),
            connected: false,
        }
    }
}

impl AuracleDevice for Nrf5340AudioDk {
    async fn connect(&mut self) -> Result<(), AuracleError> {
        // Stub: would establish BLE/serial connection to nRF5340 Audio DK
        self.connected = true;
        Ok(())
    }

    async fn send_command(&mut self, cmd: Command) -> Result<Response, AuracleError> {
        if !self.connected {
            return Err(AuracleError::ConnectionFailed(
                "Device not connected".to_string(),
            ));
        }

        // Stub: would send command over BLE/serial
        Ok(Response {
            status: 0,
            payload: vec![cmd.opcode],
        })
    }

    async fn disconnect(&mut self) -> Result<(), AuracleError> {
        self.connected = false;
        Ok(())
    }
}
