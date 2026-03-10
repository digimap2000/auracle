use std::collections::HashMap;
use std::sync::Arc;
use std::time::Instant;

use chrono::{DateTime, Utc};
use tauri::{AppHandle, Emitter};
use tokio::sync::Mutex;
use tokio_stream::StreamExt;

use super::proto::observation;
use super::proto::observation::observation_service_client::ObservationServiceClient;
use crate::ble::{BleDevice, ManufacturerData};
use crate::error::AuracleError;

const DEFAULT_DAEMON_ADDR: &str = "http://127.0.0.1:50051";

struct BridgeState {
    scanning: bool,
    devices: HashMap<String, BleDevice>,
    cancel_tx: Option<tokio::sync::oneshot::Sender<()>>,
    unit_id: String,
}

/// Bridges daemon BLE scan observations to the existing `ble-devices-updated` Tauri event.
/// Drop-in replacement for BleScanner that routes through the daemon gRPC API.
pub struct DaemonScanBridge {
    inner: Arc<Mutex<BridgeState>>,
}

impl DaemonScanBridge {
    pub fn new() -> Self {
        Self {
            inner: Arc::new(Mutex::new(BridgeState {
                scanning: false,
                devices: HashMap::new(),
                cancel_tx: None,
                unit_id: String::new(),
            })),
        }
    }

    pub async fn start_scan(&self, app: AppHandle, unit_id: String) -> Result<(), AuracleError> {
        let mut state = self.inner.lock().await;
        if state.scanning {
            return Ok(());
        }

        let channel = tonic::transport::Channel::from_static(DEFAULT_DAEMON_ADDR)
            .connect()
            .await
            .map_err(|e| AuracleError::ConnectionFailed(format!("Daemon connection failed: {e}")))?;

        let mut client = ObservationServiceClient::new(channel);

        // Start scan on the daemon
        client
            .start_scan(observation::StartScanRequest {
                unit_id: unit_id.clone(),
                allow_duplicates: true,
            })
            .await
            .map_err(|e| AuracleError::CommandFailed(format!("StartScan failed: {e}")))?;

        state.scanning = true;
        state.devices.clear();
        state.unit_id = unit_id.clone();

        let (cancel_tx, cancel_rx) = tokio::sync::oneshot::channel::<()>();
        state.cancel_tx = Some(cancel_tx);

        let inner = Arc::clone(&self.inner);

        tokio::spawn(async move {
            Self::stream_loop(client, unit_id, inner, app, cancel_rx).await;
        });

        Ok(())
    }

    pub async fn stop_scan(&self) -> Result<(), AuracleError> {
        let mut state = self.inner.lock().await;
        if !state.scanning {
            return Ok(());
        }
        state.scanning = false;
        let unit_id = state.unit_id.clone();
        if let Some(tx) = state.cancel_tx.take() {
            let _ = tx.send(());
        }

        // Best-effort stop scan on daemon
        if !unit_id.is_empty() {
            if let Ok(channel) = tonic::transport::Channel::from_static(DEFAULT_DAEMON_ADDR)
                .connect()
                .await
            {
                let mut client = ObservationServiceClient::new(channel);
                let _ = client
                    .stop_scan(observation::StopScanRequest {
                        unit_id,
                    })
                    .await;
            }
        }

        Ok(())
    }

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

    fn proto_to_ble_device(adv: &observation::BleAdvertisement) -> BleDevice {
        let name = if adv.name.is_empty() {
            "Unknown".to_string()
        } else {
            adv.name.clone()
        };

        let manufacturer_data = if adv.company_id != 0 || !adv.manufacturer_data.is_empty() {
            vec![ManufacturerData {
                company_id: adv.company_id as u16,
                data: adv.manufacturer_data.clone(),
            }]
        } else {
            vec![]
        };

        BleDevice {
            id: adv.device_id.clone(),
            name,
            rssi: adv.rssi as i16,
            is_connected: false,
            tx_power: if adv.tx_power == 127 {
                None
            } else {
                Some(adv.tx_power as i16)
            },
            services: adv.service_uuids.clone(),
            manufacturer_data,
            last_seen: Utc::now().to_rfc3339(),
        }
    }

    async fn stream_loop(
        mut client: ObservationServiceClient<tonic::transport::Channel>,
        unit_id: String,
        inner: Arc<Mutex<BridgeState>>,
        app: AppHandle,
        mut cancel_rx: tokio::sync::oneshot::Receiver<()>,
    ) {
        let stream_result = client
            .watch_observations(observation::WatchObservationsRequest {
                unit_id,
            })
            .await;

        let mut stream = match stream_result {
            Ok(response) => response.into_inner(),
            Err(e) => {
                eprintln!("[daemon-scan] WatchObservations failed: {e}");
                let mut state = inner.lock().await;
                state.scanning = false;
                return;
            }
        };

        let mut last_emit = Instant::now();
        let mut dirty = false;

        loop {
            tokio::select! {
                _ = &mut cancel_rx => {
                    break;
                }
                event = stream.next() => {
                    match event {
                        Some(Ok(obs_event)) => {
                            if let Some(observation::observation_event::Payload::BleAdvertisement(adv)) = obs_event.payload {
                                let mut device = Self::proto_to_ble_device(&adv);
                                let mut state = inner.lock().await;

                                // Preserve name and services from previous advertisements
                                if device.name == "Unknown" {
                                    if let Some(existing) = state.devices.get(&device.id) {
                                        if existing.name != "Unknown" {
                                            device.name = existing.name.clone();
                                        }
                                    }
                                }
                                if device.services.is_empty() {
                                    if let Some(existing) = state.devices.get(&device.id) {
                                        if !existing.services.is_empty() {
                                            device.services = existing.services.clone();
                                        }
                                    }
                                }

                                state.devices.insert(device.id.clone(), device);
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
                        Some(Err(e)) => {
                            eprintln!("[daemon-scan] stream error: {e}");
                            break;
                        }
                        None => break,
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

        let mut state = inner.lock().await;
        state.scanning = false;
    }
}
