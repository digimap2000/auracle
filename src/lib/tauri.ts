import { invoke } from "@tauri-apps/api/core";

export interface BleDevice {
  id: string;
  name: string;
  rssi: number;
  is_connected: boolean;
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

export async function scanBleDevices(): Promise<BleDevice[]> {
  return invoke<BleDevice[]>("scan_ble_devices");
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
