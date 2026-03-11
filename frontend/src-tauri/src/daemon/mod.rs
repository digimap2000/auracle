pub mod scan_bridge;

pub mod proto {
    pub mod compliance {
        tonic::include_proto!("auracle.compliance.v1");
    }
    pub mod inventory {
        tonic::include_proto!("auracle.inventory.v1");
    }
    pub mod observation {
        tonic::include_proto!("auracle.observation.v1");
    }
}

use proto::compliance::compliance_service_client::ComplianceServiceClient;
use proto::inventory::inventory_service_client::InventoryServiceClient;
use proto::observation::observation_service_client::ObservationServiceClient;

use serde::Serialize;
use tonic::transport::Channel;

const DEFAULT_DAEMON_ADDR: &str = "http://127.0.0.1:50051";

#[derive(Debug, Serialize, Clone)]
pub struct DaemonUnit {
    pub id: String,
    pub kind: String,
    pub present: bool,
    pub vendor: String,
    pub product: String,
    pub serial: String,
    pub firmware_version: String,
    pub capabilities: Vec<String>,
}

#[derive(Debug, Serialize, Clone)]
pub struct DaemonCandidate {
    pub id: String,
    pub transport: String,
    pub present: bool,
    pub detail: String,
}

#[derive(Debug, Serialize, Clone)]
pub struct ComplianceRuleInfo {
    pub id: String,
    pub title: String,
    pub verdict: String,
    pub message: String,
    pub reference: String,
}

#[derive(Debug, Serialize, Clone)]
pub struct ComplianceSuiteInfo {
    pub id: String,
    pub title: String,
    pub rule_ids: Vec<String>,
}

#[derive(Debug, Serialize, Clone)]
pub struct ComplianceFinding {
    pub rule_id: String,
    pub verdict: String,
    pub message: String,
    pub reference: String,
    pub observed_device_id: String,
    pub observed_device_name: String,
}

#[derive(Debug, Serialize, Clone)]
pub struct ComplianceRunResult {
    pub unit_id: String,
    pub target_id: String,
    pub rule_count: u32,
    pub evaluated_device_count: u32,
    pub findings: Vec<ComplianceFinding>,
}

fn kind_name(kind: i32) -> &'static str {
    match kind {
        1 => "nrf5340-audio-dk",
        2 => "host-bluetooth",
        _ => "unknown",
    }
}

fn transport_name(transport: i32) -> &'static str {
    match transport {
        1 => "serial",
        2 => "mdns",
        3 => "host-bluetooth",
        _ => "unknown",
    }
}

fn capability_name(cap: i32) -> &'static str {
    match cap {
        1 => "ble-scan",
        2 => "le-audio-sink",
        3 => "le-audio-source",
        4 => "unicast-client",
        _ => "unknown",
    }
}

fn candidate_detail(candidate: &proto::inventory::HardwareCandidate) -> String {
    if let Some(details) = &candidate.details {
        match details {
            proto::inventory::hardware_candidate::Details::Serial(s) => {
                s.port.clone()
            }
            proto::inventory::hardware_candidate::Details::Mdns(m) => {
                format!("{}:{}", m.host, m.port)
            }
            proto::inventory::hardware_candidate::Details::HostBluetooth(bt) => {
                format!("{} [{}]", bt.name, bt.address)
            }
        }
    } else {
        String::new()
    }
}

pub struct DaemonClient {
    inventory: InventoryServiceClient<Channel>,
}

pub fn observation_client() -> ObservationServiceClient<Channel> {
    static OBSERVATION_CHANNEL: std::sync::OnceLock<Channel> = std::sync::OnceLock::new();
    let channel = OBSERVATION_CHANNEL
        .get_or_init(|| Channel::from_static(DEFAULT_DAEMON_ADDR).connect_lazy())
        .clone();
    ObservationServiceClient::new(channel)
}

pub fn compliance_client() -> ComplianceServiceClient<Channel> {
    static COMPLIANCE_CHANNEL: std::sync::OnceLock<Channel> = std::sync::OnceLock::new();
    let channel = COMPLIANCE_CHANNEL
        .get_or_init(|| Channel::from_static(DEFAULT_DAEMON_ADDR).connect_lazy())
        .clone();
    ComplianceServiceClient::new(channel)
}

impl DaemonClient {
    pub async fn connect() -> Result<Self, String> {
        let channel = Channel::from_static(DEFAULT_DAEMON_ADDR)
            .connect()
            .await
            .map_err(|e| format!("Failed to connect to daemon: {e}"))?;

        Ok(Self {
            inventory: InventoryServiceClient::new(channel),
        })
    }

    pub async fn list_units(&mut self, include_offline: bool) -> Result<Vec<DaemonUnit>, String> {
        let request = proto::inventory::ListUnitsRequest { include_offline };
        let response = self.inventory
            .list_units(request)
            .await
            .map_err(|e| format!("ListUnits failed: {e}"))?;

        let units = response.into_inner().units.into_iter().map(|u| {
            let identity = u.identity.as_ref();
            DaemonUnit {
                id: u.id,
                kind: kind_name(u.kind).to_string(),
                present: u.present,
                vendor: identity.map(|i| i.vendor.clone()).unwrap_or_default(),
                product: identity.map(|i| i.product.clone()).unwrap_or_default(),
                serial: identity.map(|i| i.serial.clone()).unwrap_or_default(),
                firmware_version: identity.map(|i| i.firmware_version.clone()).unwrap_or_default(),
                capabilities: u.capabilities.iter().map(|c| capability_name(*c).to_string()).collect(),
            }
        }).collect();

        Ok(units)
    }

    pub async fn list_candidates(&mut self, include_gone: bool) -> Result<Vec<DaemonCandidate>, String> {
        let request = proto::inventory::ListCandidatesRequest { include_gone };
        let response = self.inventory
            .list_candidates(request)
            .await
            .map_err(|e| format!("ListCandidates failed: {e}"))?;

        let candidates = response.into_inner().candidates.into_iter().map(|c| {
            DaemonCandidate {
                id: c.id.clone(),
                transport: transport_name(c.transport).to_string(),
                present: c.present,
                detail: candidate_detail(&c),
            }
        }).collect();

        Ok(candidates)
    }

}
