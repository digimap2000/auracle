use std::collections::HashMap;
use std::sync::Arc;
use std::time::Instant;

use btleplug::api::{Central, CentralEvent, Manager as _, Peripheral as _, ScanFilter};
use btleplug::platform::Manager;
use chrono::{DateTime, Utc};
use futures::StreamExt;
use tauri::{AppHandle, Emitter};
use tokio::sync::Mutex;

use super::{BleDevice, ManufacturerData};
use crate::error::AuracleError;

/// Extract a BleDevice from a btleplug peripheral's properties.
async fn extract_device(
    id: String,
    peripheral: &btleplug::platform::Peripheral,
) -> Option<BleDevice> {
    let props = peripheral.properties().await.ok()??;

    // Diagnostic: see exactly what btleplug returns
    eprintln!(
        "[ble] {} local_name={:?} rssi={:?} services={}",
        &id[..8.min(id.len())],
        &props.local_name,
        props.rssi,
        props.services.len(),
    );

    let name = props.local_name.unwrap_or_else(|| "Unknown".to_string());
    let rssi = props.rssi.unwrap_or(-100);
    let tx_power = props.tx_power_level;
    let services: Vec<String> = props.services.iter().map(|u| u.to_string()).collect();
    let manufacturer_data: Vec<ManufacturerData> = props
        .manufacturer_data
        .iter()
        .map(|(k, v)| ManufacturerData {
            company_id: *k,
            data: v.clone(),
        })
        .collect();
    let now = Utc::now().to_rfc3339();

    Some(BleDevice {
        id,
        name,
        rssi,
        is_connected: false,
        tx_power,
        services,
        manufacturer_data,
        last_seen: now,
    })
}

struct ScannerState {
    scanning: bool,
    devices: HashMap<String, BleDevice>,
    cancel_tx: Option<tokio::sync::oneshot::Sender<()>>,
}

/// Managed state for BLE scanning. Registered via `.manage()` in lib.rs.
pub struct BleScanner {
    inner: Arc<Mutex<ScannerState>>,
}

impl BleScanner {
    pub fn new() -> Self {
        Self {
            inner: Arc::new(Mutex::new(ScannerState {
                scanning: false,
                devices: HashMap::new(),
                cancel_tx: None,
            })),
        }
    }

    /// Start continuous BLE scanning. Idempotent — returns Ok if already scanning.
    pub async fn start_scan(&self, app: AppHandle) -> Result<(), AuracleError> {
        let mut state = self.inner.lock().await;
        if state.scanning {
            return Ok(());
        }

        let manager = Manager::new().await.map_err(|e| {
            let msg = e.to_string();
            if msg.contains("permission") || msg.contains("denied") || msg.contains("unauthorized")
            {
                AuracleError::Ble(
                    "Bluetooth permission denied — enable Auracle in System Settings > Privacy & Security > Bluetooth".to_string(),
                )
            } else {
                AuracleError::from(e)
            }
        })?;

        let adapters = manager.adapters().await?;
        let adapter = adapters.into_iter().next().ok_or_else(|| {
            AuracleError::Ble(
                "No Bluetooth adapter found — check that Bluetooth is enabled in System Settings"
                    .to_string(),
            )
        })?;

        adapter.start_scan(ScanFilter::default()).await?;

        state.scanning = true;
        state.devices.clear();

        let (cancel_tx, cancel_rx) = tokio::sync::oneshot::channel::<()>();
        state.cancel_tx = Some(cancel_tx);

        let inner = Arc::clone(&self.inner);

        tokio::spawn(async move {
            Self::scan_loop(adapter, inner, app, cancel_rx).await;
        });

        Ok(())
    }

    /// Stop BLE scanning. Idempotent — returns Ok if not scanning.
    pub async fn stop_scan(&self) -> Result<(), AuracleError> {
        let mut state = self.inner.lock().await;
        if !state.scanning {
            return Ok(());
        }
        state.scanning = false;
        if let Some(tx) = state.cancel_tx.take() {
            let _ = tx.send(());
        }
        Ok(())
    }

    /// Remove devices not seen in the last 30 seconds.
    fn prune_stale(devices: &mut HashMap<String, BleDevice>) {
        let now = Utc::now();
        devices.retain(|_, d| {
            if let Ok(ts) = d.last_seen.parse::<DateTime<Utc>>() {
                (now - ts).num_seconds() < 30
            } else {
                false
            }
        });
    }

    async fn scan_loop(
        adapter: btleplug::platform::Adapter,
        inner: Arc<Mutex<ScannerState>>,
        app: AppHandle,
        mut cancel_rx: tokio::sync::oneshot::Receiver<()>,
    ) {
        // Try event stream first, fall back to polling
        let event_stream = adapter.events().await;
        match event_stream {
            Ok(mut stream) => {
                let mut last_emit = Instant::now();
                let mut dirty = false;

                loop {
                    tokio::select! {
                        _ = &mut cancel_rx => {
                            let _ = adapter.stop_scan().await;
                            break;
                        }
                        event = stream.next() => {
                            match event {
                                Some(CentralEvent::DeviceDiscovered(pid))
                                | Some(CentralEvent::DeviceUpdated(pid)) => {
                                    if let Ok(peripheral) = adapter.peripheral(&pid).await {
                                        let id = pid.to_string();
                                        if let Some(mut device) = extract_device(id.clone(), &peripheral).await {
                                            let mut state = inner.lock().await;
                                            // Preserve previously known name if new advertisement lacks one
                                            if device.name == "Unknown" {
                                                if let Some(existing) = state.devices.get(&id) {
                                                    if existing.name != "Unknown" {
                                                        device.name = existing.name.clone();
                                                    }
                                                }
                                            }
                                            // Preserve previously known services if new advertisement has none
                                            if device.services.is_empty() {
                                                if let Some(existing) = state.devices.get(&id) {
                                                    if !existing.services.is_empty() {
                                                        device.services = existing.services.clone();
                                                    }
                                                }
                                            }
                                            state.devices.insert(id, device);
                                            dirty = true;

                                            // Debounce: emit at most ~2 times per second
                                            if last_emit.elapsed().as_millis() >= 500 {
                                                let devices: Vec<BleDevice> = state.devices.values().cloned().collect();
                                                let _ = app.emit("ble-devices-updated", &devices);
                                                dirty = false;
                                                last_emit = Instant::now();
                                            }
                                        }
                                    }
                                }
                                None => break,
                                _ => {}
                            }
                        }
                        // Periodic flush for debounced events + stale device pruning
                        _ = tokio::time::sleep(tokio::time::Duration::from_millis(500)) => {
                            let mut state = inner.lock().await;
                            let before = state.devices.len();
                            Self::prune_stale(&mut state.devices);
                            if state.devices.len() != before {
                                dirty = true;
                            }
                            if dirty {
                                let devices: Vec<BleDevice> = state.devices.values().cloned().collect();
                                let _ = app.emit("ble-devices-updated", &devices);
                                dirty = false;
                                last_emit = Instant::now();
                            }
                        }
                    }
                }
            }
            Err(_) => {
                // Fallback: periodic polling
                loop {
                    tokio::select! {
                        _ = &mut cancel_rx => {
                            let _ = adapter.stop_scan().await;
                            break;
                        }
                        _ = tokio::time::sleep(tokio::time::Duration::from_secs(1)) => {
                            if let Ok(peripherals) = adapter.peripherals().await {
                                let mut state = inner.lock().await;
                                for p in peripherals {
                                    let id = p.id().to_string();
                                    if let Some(mut device) = extract_device(id.clone(), &p).await {
                                        if device.name == "Unknown" {
                                            if let Some(existing) = state.devices.get(&id) {
                                                if existing.name != "Unknown" {
                                                    device.name = existing.name.clone();
                                                }
                                            }
                                        }
                                        if device.services.is_empty() {
                                            if let Some(existing) = state.devices.get(&id) {
                                                if !existing.services.is_empty() {
                                                    device.services = existing.services.clone();
                                                }
                                            }
                                        }
                                        state.devices.insert(id, device);
                                    }
                                }
                                Self::prune_stale(&mut state.devices);
                                let devices: Vec<BleDevice> = state.devices.values().cloned().collect();
                                let _ = app.emit("ble-devices-updated", &devices);
                            }
                        }
                    }
                }
            }
        }
    }
}
