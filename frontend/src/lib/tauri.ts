import { invoke } from "@tauri-apps/api/core";

export interface BluetoothAdapter {
  id: string;
  name: string;
  is_available: boolean;
}

export async function getBluetoothAdapter(): Promise<BluetoothAdapter> {
  return invoke<BluetoothAdapter>("get_bluetooth_adapter");
}

export interface ManufacturerData {
  company_id: number;
  data: number[];
}

export interface BleDevice {
  id: string;
  name: string;
  rssi: number;
  is_connected: boolean;
  tx_power: number | null;
  services: string[];
  manufacturer_data: ManufacturerData[];
  last_seen: string;
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
