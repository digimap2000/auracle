#![allow(dead_code)]

pub mod nrf5340;

use crate::error::AuracleError;
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Command {
    pub opcode: u8,
    pub payload: Vec<u8>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Response {
    pub status: u8,
    pub payload: Vec<u8>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ConnectedDevice {
    pub id: String,
    pub name: String,
    pub device_type: String,
    pub firmware_version: String,
}

#[allow(async_fn_in_trait)]
pub trait AuracleDevice {
    async fn connect(&mut self) -> Result<(), AuracleError>;
    async fn send_command(&mut self, cmd: Command) -> Result<Response, AuracleError>;
    async fn disconnect(&mut self) -> Result<(), AuracleError>;
}
