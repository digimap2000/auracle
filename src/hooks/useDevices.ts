import { useCallback, useState } from "react";
import {
  type BleDevice,
  type ConnectedDevice,
  type SerialPort,
  connectDevice,
  disconnectDevice,
  getConnectedDevices,
  scanBleDevices,
  scanSerialPorts,
} from "@/lib/tauri";

interface DevicesState {
  bleDevices: BleDevice[];
  serialPorts: SerialPort[];
  connectedDevices: ConnectedDevice[];
  scanning: boolean;
  error: string | null;
}

export function useDevices() {
  const [state, setState] = useState<DevicesState>({
    bleDevices: [],
    serialPorts: [],
    connectedDevices: [],
    scanning: false,
    error: null,
  });

  const scan = useCallback(async () => {
    setState((s) => ({ ...s, scanning: true, error: null }));
    try {
      const [ble, serial] = await Promise.all([
        scanBleDevices(),
        scanSerialPorts(),
      ]);
      setState((s) => ({ ...s, bleDevices: ble, serialPorts: serial, scanning: false }));
    } catch (e) {
      setState((s) => ({
        ...s,
        scanning: false,
        error: e instanceof Error ? e.message : String(e),
      }));
    }
  }, []);

  const connect = useCallback(async (deviceId: string) => {
    try {
      await connectDevice(deviceId);
      const devices = await getConnectedDevices();
      setState((s) => ({ ...s, connectedDevices: devices, error: null }));
    } catch (e) {
      setState((s) => ({
        ...s,
        error: e instanceof Error ? e.message : String(e),
      }));
    }
  }, []);

  const disconnect = useCallback(async (deviceId: string) => {
    try {
      await disconnectDevice(deviceId);
      const devices = await getConnectedDevices();
      setState((s) => ({ ...s, connectedDevices: devices, error: null }));
    } catch (e) {
      setState((s) => ({
        ...s,
        error: e instanceof Error ? e.message : String(e),
      }));
    }
  }, []);

  const refreshConnected = useCallback(async () => {
    try {
      const devices = await getConnectedDevices();
      setState((s) => ({ ...s, connectedDevices: devices }));
    } catch (e) {
      setState((s) => ({
        ...s,
        error: e instanceof Error ? e.message : String(e),
      }));
    }
  }, []);

  return {
    ...state,
    scan,
    connect,
    disconnect,
    refreshConnected,
  };
}
