use std::collections::HashMap;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;
use std::time::Instant;

use chrono::{DateTime, Utc};
use tauri::{AppHandle, Emitter};
use tokio::sync::Mutex;
use tokio_stream::StreamExt;

use super::proto::observation;
use super::proto::observation::observation_service_client::ObservationServiceClient;
use crate::ble::{BleDevice, BlePacket, ManufacturerData};
use crate::error::AuracleError;

const DEFAULT_DAEMON_ADDR: &str = "http://127.0.0.1:50051";
static NEXT_PACKET_ID: AtomicU64 = AtomicU64::new(1);

struct UnitScanState {
    devices: HashMap<String, BleDevice>,
    cancel_tx: Option<tokio::sync::oneshot::Sender<()>>,
}

struct BridgeState {
    scans: HashMap<String, UnitScanState>,
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
                scans: HashMap::new(),
            })),
        }
    }

    pub async fn start_scan(&self, app: AppHandle, unit_id: String) -> Result<(), AuracleError> {
        {
            let state = self.inner.lock().await;
            if state
                .scans
                .get(&unit_id)
                .and_then(|scan| scan.cancel_tx.as_ref())
                .is_some()
            {
                return Ok(());
            }
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

        let (cancel_tx, cancel_rx) = tokio::sync::oneshot::channel::<()>();

        {
            let mut state = self.inner.lock().await;
            let scan = state
                .scans
                .entry(unit_id.clone())
                .or_insert_with(|| UnitScanState {
                    devices: HashMap::new(),
                    cancel_tx: None,
                });
            scan.cancel_tx = Some(cancel_tx);
        }

        let inner = Arc::clone(&self.inner);
        let stream_app = app.clone();

        tokio::spawn(async move {
            Self::stream_loop(client, unit_id, inner, stream_app, cancel_rx).await;
        });

        Ok(())
    }

    pub async fn stop_scan(&self, app: AppHandle, unit_id: String) -> Result<(), AuracleError> {
        let cancel_tx = {
            let mut state = self.inner.lock().await;
            state.scans.remove(&unit_id).and_then(|mut scan| scan.cancel_tx.take())
        };

        if let Some(tx) = cancel_tx {
            let _ = tx.send(());
        }

        // Best-effort stop scan on daemon
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

        Self::emit_devices(&self.inner, &app).await;
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

    fn collect_devices(state: &BridgeState) -> Vec<BleDevice> {
        state
            .scans
            .values()
            .flat_map(|scan| scan.devices.values().cloned())
            .collect()
    }

    async fn emit_devices(inner: &Arc<Mutex<BridgeState>>, app: &AppHandle) {
        let devices = {
            let state = inner.lock().await;
            Self::collect_devices(&state)
        };
        let _ = app.emit("ble-devices-updated", &devices);
    }

    fn timestamp_to_rfc3339(timestamp_ms: i64) -> String {
        DateTime::<Utc>::from_timestamp_millis(timestamp_ms)
            .map(|ts| ts.to_rfc3339())
            .unwrap_or_else(|| Utc::now().to_rfc3339())
    }

    fn proto_to_ble_device(
        stable_id: &str,
        unit_id: &str,
        timestamp_ms: i64,
        adv: &observation::BleAdvertisement,
    ) -> BleDevice {
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
            unit_id: unit_id.to_string(),
            stable_id: stable_id.to_string(),
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
            service_labels: adv.service_labels.clone(),
            manufacturer_data,
            last_seen: Self::timestamp_to_rfc3339(timestamp_ms),
        }
    }

    fn proto_to_ble_packet(
        stable_id: &str,
        unit_id: &str,
        timestamp_ms: i64,
        adv: &observation::BleAdvertisement,
    ) -> BlePacket {
        let packet_id = NEXT_PACKET_ID.fetch_add(1, Ordering::Relaxed);

        BlePacket {
            unit_id: unit_id.to_string(),
            stable_id: stable_id.to_string(),
            id: format!("{unit_id}:{packet_id}"),
            device_id: adv.device_id.clone(),
            name: if adv.name.is_empty() {
                "Unknown".to_string()
            } else {
                adv.name.clone()
            },
            rssi: adv.rssi as i16,
            tx_power: if adv.tx_power == 127 {
                None
            } else {
                Some(adv.tx_power as i16)
            },
            service_uuids: adv.service_uuids.clone(),
            service_labels: adv.service_labels.clone(),
            company_id: if adv.company_id == 0 {
                None
            } else {
                Some(adv.company_id as u16)
            },
            company_data: adv.manufacturer_data.clone(),
            address_type: adv.address_type.clone(),
            sid: adv.sid,
            adv_type: adv.adv_type,
            adv_props: adv.adv_props,
            interval: adv.interval,
            primary_phy: adv.primary_phy,
            secondary_phy: adv.secondary_phy,
            raw_data: adv.raw_data.clone(),
            raw_scan_response: adv.raw_scan_response.clone(),
            timestamp_ms,
        }
    }

    fn is_host_unit(unit_id: &str) -> bool {
        unit_id.starts_with("hostbt:")
    }

    fn is_address_like(device_id: &str) -> bool {
        device_id.contains(':')
    }

    fn normalize_name(name: &str) -> String {
        name.trim().to_lowercase()
    }

    fn hex_prefix(bytes: &[u8], max_len: usize) -> String {
        bytes.iter()
            .take(max_len)
            .map(|byte| format!("{byte:02x}"))
            .collect::<String>()
    }

    fn fingerprint_hash(parts: &[String]) -> String {
        let mut hash: u64 = 0xcbf29ce484222325;

        for part in parts {
            for byte in part.as_bytes() {
                hash ^= u64::from(*byte);
                hash = hash.wrapping_mul(0x100000001b3);
            }
            hash ^= u64::from(b'|');
            hash = hash.wrapping_mul(0x100000001b3);
        }

        format!("{hash:016x}")
    }

    fn stable_id_for(unit_id: &str, adv: &observation::BleAdvertisement) -> String {
        if Self::is_host_unit(unit_id) || !Self::is_address_like(&adv.device_id) {
            return adv.device_id.clone();
        }

        let mut parts = Vec::new();

        if !adv.name.is_empty() {
            parts.push(format!("name:{}", Self::normalize_name(&adv.name)));
        }
        if adv.company_id != 0 {
            parts.push(format!("company:{:04x}", adv.company_id));
        }
        if !adv.service_uuids.is_empty() {
            let mut uuids = adv
                .service_uuids
                .iter()
                .map(|uuid| uuid.to_lowercase())
                .collect::<Vec<_>>();
            uuids.sort();
            uuids.dedup();
            parts.push(format!("svc:{}", uuids.join(",")));
        }
        if !adv.manufacturer_data.is_empty() {
            parts.push(format!(
                "mfg:{}",
                Self::hex_prefix(&adv.manufacturer_data, 8)
            ));
        }
        if !adv.raw_data.is_empty() {
            parts.push(format!("raw:{}", Self::hex_prefix(&adv.raw_data, 12)));
        }

        if parts.is_empty() {
            return adv.device_id.clone();
        }

        format!("fp:{}", Self::fingerprint_hash(&parts))
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
                unit_id: unit_id.clone(),
                include_initial_snapshot: true,
            })
            .await;

        let mut stream = match stream_result {
            Ok(response) => response.into_inner(),
            Err(e) => {
                eprintln!("[daemon-scan] WatchObservations failed: {e}");
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
                            if obs_event.unit_id != unit_id {
                                continue;
                            }
                            if let Some(observation::observation_event::Payload::BleAdvertisement(adv)) = obs_event.payload {
                                let stable_id = Self::stable_id_for(&unit_id, &adv);
                                let mut device = Self::proto_to_ble_device(&stable_id, &unit_id, obs_event.timestamp_ms, &adv);
                                let packet = Self::proto_to_ble_packet(&stable_id, &unit_id, obs_event.timestamp_ms, &adv);
                                let _ = app.emit("ble-packet-received", &packet);
                                let mut state = inner.lock().await;
                                let Some(scan) = state.scans.get_mut(&unit_id) else {
                                    break;
                                };

                                // Preserve name and services from previous advertisements
                                if let Some(existing) = scan.devices.get(&stable_id) {
                                    device.id = existing.id.clone();
                                    if device.name == "Unknown" && existing.name != "Unknown" {
                                        device.name = existing.name.clone();
                                    }
                                    for service in &existing.services {
                                        if !device.services.iter().any(|current| current == service) {
                                            device.services.push(service.clone());
                                        }
                                    }
                                    for (uuid, label) in &existing.service_labels {
                                        device
                                            .service_labels
                                            .entry(uuid.clone())
                                            .or_insert_with(|| label.clone());
                                    }
                                    if device.manufacturer_data.is_empty() && !existing.manufacturer_data.is_empty() {
                                        device.manufacturer_data = existing.manufacturer_data.clone();
                                    }
                                }
                                device.services.sort();
                                device.services.dedup();

                                scan.devices.insert(stable_id, device);
                                dirty = true;

                                // Debounce: emit at most ~2 times per second
                                if last_emit.elapsed().as_millis() >= 500 {
                                    let devices = Self::collect_devices(&state);
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
                    let Some(scan) = state.scans.get_mut(&unit_id) else {
                        break;
                    };
                    let before = scan.devices.len();
                    Self::prune_stale(&mut scan.devices);
                    if scan.devices.len() != before {
                        dirty = true;
                    }
                    if dirty {
                        let devices = Self::collect_devices(&state);
                        let _ = app.emit("ble-devices-updated", &devices);
                        dirty = false;
                        last_emit = Instant::now();
                    }
                }
            }
        }
    }
}
