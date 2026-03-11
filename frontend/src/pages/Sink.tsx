import { useEffect, useMemo, useRef, useState } from "react";
import { Headphones } from "lucide-react";
import { ScrollArea } from "@/components/ui/scroll-area";
import { DeviceCard } from "@/components/devices/DeviceCard";
import { isLeAudioDevice } from "@/lib/ble-utils";
import type { BleDevice, DaemonUnit } from "@/lib/tauri";

const RSSI_WINDOW_SIZE = 6;

interface SinkProps {
  bleDevices: BleDevice[];
  scanning: boolean;
  activeUnit: DaemonUnit | null;
}

export function Sink({ bleDevices, scanning, activeUnit }: SinkProps) {
  // Tick timer for relative time updates
  const [, setTick] = useState(0);
  useEffect(() => {
    const id = setInterval(() => setTick((t) => t + 1), 1000);
    return () => clearInterval(id);
  }, []);

  const [expandedIds, setExpandedIds] = useState<Set<string>>(new Set());

  // Rolling RSSI window per device (keyed by device ID)
  const rssiWindows = useRef<Map<string, number[]>>(new Map());

  // Filter to LE Audio devices, smooth RSSI, then sort
  const audioDevices = useMemo(() => {
    const filtered = bleDevices.filter((d) => isLeAudioDevice(d.services));
    const windows = rssiWindows.current;

    const smoothed = filtered.map((device) => {
      let samples = windows.get(device.id);
      if (!samples) {
        samples = [];
        windows.set(device.id, samples);
      }
      samples.push(device.rssi);
      if (samples.length > RSSI_WINDOW_SIZE) {
        samples.shift();
      }
      const avg = Math.round(
        samples.reduce((sum, v) => sum + v, 0) / samples.length
      );
      return { ...device, rssi: avg };
    });

    // Prune devices no longer present
    const activeIds = new Set(filtered.map((d) => d.id));
    for (const id of windows.keys()) {
      if (!activeIds.has(id)) windows.delete(id);
    }

    return smoothed.sort((a, b) => {
      const aUnknown = a.name === "Unknown";
      const bUnknown = b.name === "Unknown";
      if (aUnknown !== bUnknown) return aUnknown ? 1 : -1;
      if (!aUnknown && !bUnknown) return a.name.localeCompare(b.name);
      return b.rssi - a.rssi;
    });
  }, [bleDevices]);

  return (
    <div className="flex h-full flex-col">
      <div className="flex-1 overflow-hidden">
        {!activeUnit ? (
          <div className="flex h-full items-center justify-center">
            <div className="text-center text-muted-foreground">
              <Headphones className="mx-auto mb-3 size-6 opacity-30" />
              <p className="text-sm">Select an active unit on the Home page</p>
            </div>
          </div>
        ) : audioDevices.length === 0 ? (
          <div className="flex h-full items-center justify-center">
            <div className="text-center text-muted-foreground">
              <Headphones className="mx-auto mb-3 size-6 animate-pulse opacity-50" />
              <p className="text-sm">
                {scanning
                  ? `Scanning for audio streams from ${activeUnit.product || activeUnit.kind}...`
                  : `Waiting for audio streams from ${activeUnit.product || activeUnit.kind}...`}
              </p>
            </div>
          </div>
        ) : (
          <ScrollArea className="h-full">
            <div className="mx-auto w-full max-w-2xl space-y-3 px-6 py-6">
              {audioDevices.map((device) => (
                <DeviceCard
                  key={device.id}
                  device={device}
                  expanded={expandedIds.has(device.id)}
                  onToggle={() =>
                    setExpandedIds((prev) => {
                      const next = new Set(prev);
                      if (next.has(device.id)) next.delete(device.id);
                      else next.add(device.id);
                      return next;
                    })
                  }
                />
              ))}
            </div>
          </ScrollArea>
        )}
      </div>
    </div>
  );
}
