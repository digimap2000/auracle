import { useCallback, useEffect, useState } from "react";
import { listen } from "@tauri-apps/api/event";
import {
  type BleDevice,
  type ConnectedDevice,
  type SerialPort,
  connectDevice,
  disconnectDevice,
  getConnectedDevices,
  scanSerialPorts,
  startBleScan,
  stopBleScan,
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

  // Listen to live BLE device updates from backend
  useEffect(() => {
    const unlisten = listen<BleDevice[]>("ble-devices-updated", (event) => {
      setState((prev) => ({ ...prev, bleDevices: event.payload }));
    });
    return () => {
      unlisten.then((fn) => fn());
    };
  }, []);

  // Stop scanning on unmount
  useEffect(() => {
    return () => {
      stopBleScan().catch(() => {});
    };
  }, []);

  const startScan = useCallback(async () => {
    setState((s) => ({ ...s, scanning: true, error: null }));
    try {
      await startBleScan();
    } catch (e) {
      setState((s) => ({
        ...s,
        scanning: false,
        error: e instanceof Error ? e.message : String(e),
      }));
    }
  }, []);

  const stopScan = useCallback(async () => {
    try {
      await stopBleScan();
    } catch (e) {
      setState((s) => ({
        ...s,
        error: e instanceof Error ? e.message : String(e),
      }));
    }
    setState((s) => ({ ...s, scanning: false }));
  }, []);

  const refreshSerialPorts = useCallback(async () => {
    try {
      const ports = await scanSerialPorts();
      setState((s) => ({ ...s, serialPorts: ports }));
    } catch (e) {
      setState((s) => ({
        ...s,
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
    startScan,
    stopScan,
    refreshSerialPorts,
    connect,
    disconnect,
    refreshConnected,
  };
}
