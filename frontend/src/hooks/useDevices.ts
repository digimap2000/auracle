import { useCallback, useEffect, useRef, useState } from "react";
import { listen } from "@tauri-apps/api/event";
import {
  type BleDevice,
  type BlePacket,
  type ConnectedDevice,
  type DaemonUnit,
  type SerialPort,
  connectDevice,
  disconnectDevice,
  getConnectedDevices,
  scanSerialPorts,
  startDaemonScan,
  stopDaemonScan,
} from "@/lib/tauri";

interface DevicesState {
  allBleDevices: BleDevice[];
  allBlePackets: BlePacket[];
  serialPorts: SerialPort[];
  connectedDevices: ConnectedDevice[];
  scanning: boolean;
  error: string | null;
}

const MAX_PACKET_HISTORY = 1500;

function canScan(unit: DaemonUnit): boolean {
  return unit.present && unit.capabilities.includes("ble-scan");
}

export function useDevices(units: DaemonUnit[]) {
  const [state, setState] = useState<DevicesState>({
    allBleDevices: [],
    allBlePackets: [],
    serialPorts: [],
    connectedDevices: [],
    scanning: false,
    error: null,
  });
  const trackedUnitIdsRef = useRef<Set<string>>(new Set());

  // Listen to live BLE device updates from backend
  useEffect(() => {
    const unlisten = listen<BleDevice[]>("ble-devices-updated", (event) => {
      setState((prev) => ({ ...prev, allBleDevices: event.payload }));
    });
    return () => {
      unlisten.then((fn) => fn());
    };
  }, []);

  useEffect(() => {
    const unlisten = listen<BlePacket>("ble-packet-received", (event) => {
      setState((prev) => {
        const nextPackets = [...prev.allBlePackets, event.payload];
        if (nextPackets.length > MAX_PACKET_HISTORY) {
          nextPackets.splice(0, nextPackets.length - MAX_PACKET_HISTORY);
        }
        return { ...prev, allBlePackets: nextPackets };
      });
    });
    return () => {
      unlisten.then((fn) => fn());
    };
  }, []);

  // Auto-manage scans for every present unit that advertises BLE scan capability.
  useEffect(() => {
    const nextTrackedUnitIds = new Set(
      units.filter(canScan).map((unit) => unit.id)
    );
    const previousTrackedUnitIds = trackedUnitIdsRef.current;
    trackedUnitIdsRef.current = nextTrackedUnitIds;

    const unitsToStart = Array.from(nextTrackedUnitIds).filter(
      (id) => !previousTrackedUnitIds.has(id)
    );
    const unitsToStop = Array.from(previousTrackedUnitIds).filter(
      (id) => !nextTrackedUnitIds.has(id)
    );

    setState((prev) => ({
      ...prev,
      allBleDevices: prev.allBleDevices.filter((device) =>
        nextTrackedUnitIds.has(device.unit_id)
      ),
      allBlePackets: prev.allBlePackets.filter((packet) =>
        nextTrackedUnitIds.has(packet.unit_id)
      ),
      scanning: nextTrackedUnitIds.size > 0,
      error: null,
    }));

    for (const unitId of unitsToStop) {
      stopDaemonScan(unitId).catch(() => {});
    }

    if (unitsToStart.length === 0) {
      return;
    }

    Promise.allSettled(unitsToStart.map((unitId) => startDaemonScan(unitId))).then(
      (results) => {
        const failure = results.find(
          (result): result is PromiseRejectedResult => result.status === "rejected"
        );

        if (failure) {
          setState((prev) => ({
            ...prev,
            error:
              failure.reason instanceof Error
                ? failure.reason.message
                : String(failure.reason),
          }));
        }
      }
    );
  }, [units]);

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
    refreshSerialPorts,
    connect,
    disconnect,
    refreshConnected,
  };
}
