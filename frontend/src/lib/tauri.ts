import { invoke } from "@tauri-apps/api/core";

export interface ManufacturerData {
  company_id: number;
  data: number[];
}

export interface BleDevice {
  unit_id: string;
  stable_id: string;
  id: string;
  name: string;
  rssi: number;
  is_connected: boolean;
  tx_power: number | null;
  services: string[];
  service_labels: Record<string, string>;
  manufacturer_data: ManufacturerData[];
  last_seen: string;
}

export interface BlePacket {
  unit_id: string;
  stable_id: string;
  id: string;
  device_id: string;
  name: string;
  rssi: number;
  tx_power: number | null;
  service_uuids: string[];
  service_labels: Record<string, string>;
  company_id: number | null;
  company_data: number[];
  address_type: string;
  sid: number;
  adv_type: number;
  adv_props: number;
  interval: number;
  primary_phy: number;
  secondary_phy: number;
  raw_data: number[];
  raw_scan_response: number[];
  timestamp_ms: number;
}

export interface DecodedServiceData {
  service_uuid: string;
  service_label: string;
  raw_value: string;
  fields: DecodedField[];
}

export interface DecodedField {
  field: string;
  type: string;
  value: string;
}

export async function startBleScan(): Promise<void> {
  return invoke<void>("start_ble_scan");
}

export async function stopBleScan(): Promise<void> {
  return invoke<void>("stop_ble_scan");
}

export interface SerialPort {
  path: string;
  name: string;
  vendor_id: number | null;
  product_id: number | null;
}

export interface ConnectedDevice {
  id: string;
  name: string;
  device_type: string;
  firmware_version: string;
}

export async function scanSerialPorts(): Promise<SerialPort[]> {
  return invoke<SerialPort[]>("scan_serial_ports");
}

export async function connectDevice(deviceId: string): Promise<void> {
  return invoke("connect_device", { deviceId });
}

export async function disconnectDevice(deviceId: string): Promise<void> {
  return invoke("disconnect_device", { deviceId });
}

export async function getConnectedDevices(): Promise<ConnectedDevice[]> {
  return invoke<ConnectedDevice[]>("get_connected_devices");
}

export interface DaemonUnit {
  id: string;
  kind: string;
  present: boolean;
  vendor: string;
  product: string;
  serial: string;
  firmware_version: string;
  capabilities: string[];
}

export async function getDaemonUnits(): Promise<DaemonUnit[]> {
  return invoke<DaemonUnit[]>("get_daemon_units");
}

export async function startDaemonScan(unitId: string): Promise<void> {
  return invoke<void>("start_daemon_scan", { unitId });
}

export async function stopDaemonScan(unitId: string): Promise<void> {
  return invoke<void>("stop_daemon_scan", { unitId });
}

export async function decodeDaemonAdvertisement(
  rawData: number[],
  rawScanResponse: number[] = []
): Promise<DecodedServiceData[]> {
  return invoke<DecodedServiceData[]>("decode_daemon_advertisement", {
    rawData,
    rawScanResponse,
  });
}
