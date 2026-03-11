import { useEffect, useMemo, useRef, useState } from "react";
import { Headphones } from "lucide-react";
import { ScrollArea } from "@/components/ui/scroll-area";
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/select";
import { DeviceCard } from "@/components/devices/DeviceCard";
import { isLeAudioDevice } from "@/lib/ble-utils";
import type { BleDevice, DaemonUnit } from "@/lib/tauri";

const RSSI_WINDOW_SIZE = 6;

interface SinkProps {
  units: DaemonUnit[];
  bleDevices: BleDevice[];
  scanning: boolean;
}

function canScan(unit: DaemonUnit) {
  return unit.present && unit.capabilities.includes("ble-scan");
}

export function Sink({ units, bleDevices, scanning }: SinkProps) {
  const [, setTick] = useState(0);
  useEffect(() => {
    const id = setInterval(() => setTick((t) => t + 1), 1000);
    return () => clearInterval(id);
  }, []);

  const [expandedIds, setExpandedIds] = useState<Set<string>>(new Set());
  const [selectedUnitId, setSelectedUnitId] = useState<string | null>(null);

  const scannableUnits = useMemo(
    () => units.filter(canScan),
    [units]
  );
  const selectedUnit = useMemo(
    () => scannableUnits.find((unit) => unit.id === selectedUnitId) ?? null,
    [scannableUnits, selectedUnitId]
  );

  useEffect(() => {
    if (scannableUnits.length === 0) {
      if (selectedUnitId !== null) {
        setSelectedUnitId(null);
      }
      return;
    }

    if (!selectedUnitId || !scannableUnits.some((unit) => unit.id === selectedUnitId)) {
      setSelectedUnitId(scannableUnits[0]?.id ?? null);
    }
  }, [scannableUnits, selectedUnitId]);

  const rssiWindows = useRef<Map<string, number[]>>(new Map());

  const audioDevices = useMemo(() => {
    const filtered = bleDevices.filter(
      (device) => device.unit_id === selectedUnitId && isLeAudioDevice(device.services)
    );
    const windows = rssiWindows.current;

    const smoothed = filtered.map((device) => {
      let samples = windows.get(device.stable_id);
      if (!samples) {
        samples = [];
        windows.set(device.stable_id, samples);
      }
      samples.push(device.rssi);
      if (samples.length > RSSI_WINDOW_SIZE) {
        samples.shift();
      }
      const avg = Math.round(
        samples.reduce((sum, value) => sum + value, 0) / samples.length
      );
      return { ...device, rssi: avg };
    });

    const activeIds = new Set(filtered.map((device) => device.stable_id));
    for (const id of windows.keys()) {
      if (!activeIds.has(id)) {
        windows.delete(id);
      }
    }

    return smoothed.sort((a, b) => {
      const aUnknown = a.name === "Unknown";
      const bUnknown = b.name === "Unknown";
      if (aUnknown !== bUnknown) return aUnknown ? 1 : -1;
      if (!aUnknown && !bUnknown) return a.name.localeCompare(b.name);
      return b.rssi - a.rssi;
    });
  }, [bleDevices, selectedUnitId]);

  if (!selectedUnit) {
    return (
      <div className="flex h-full items-center justify-center">
        <div className="text-center text-muted-foreground">
          <Headphones className="mx-auto mb-3 size-6 opacity-30" />
          <p className="text-sm">No scannable units are currently available</p>
        </div>
      </div>
    );
  }

  return (
    <div className="flex h-full flex-col">
      <div className="flex-1 overflow-hidden">
        {audioDevices.length === 0 ? (
          <div className="flex h-full items-center justify-center">
            <div className="text-center text-muted-foreground">
              <Headphones className="mx-auto mb-3 size-6 animate-pulse opacity-50" />
              <p className="text-sm">
                {scanning
                  ? `Scanning for audio streams from ${selectedUnit.product || selectedUnit.kind}...`
                  : `Waiting for audio streams from ${selectedUnit.product || selectedUnit.kind}...`}
              </p>
              <div className="mt-4 flex justify-center">
                <Select value={selectedUnitId ?? undefined} onValueChange={setSelectedUnitId}>
                  <SelectTrigger className="min-w-64 bg-background">
                    <SelectValue placeholder="Select unit" />
                  </SelectTrigger>
                  <SelectContent>
                    {scannableUnits.map((unit) => (
                      <SelectItem key={unit.id} value={unit.id}>
                        {unit.product || unit.kind} · {unit.id}
                      </SelectItem>
                    ))}
                  </SelectContent>
                </Select>
              </div>
            </div>
          </div>
        ) : (
          <ScrollArea className="h-full">
            <div className="mx-auto w-full max-w-2xl space-y-3 px-6 py-6">
              <div className="flex items-center justify-between gap-3 rounded-lg border bg-card p-3">
                <div>
                  <p className="text-sm font-medium">Sink View</p>
                  <p className="text-xs text-muted-foreground">
                    Showing LE Audio-capable devices seen by the selected unit.
                  </p>
                </div>
                <Select value={selectedUnitId ?? undefined} onValueChange={setSelectedUnitId}>
                  <SelectTrigger className="min-w-64 bg-background">
                    <SelectValue placeholder="Select unit" />
                  </SelectTrigger>
                  <SelectContent>
                    {scannableUnits.map((unit) => (
                      <SelectItem key={unit.id} value={unit.id}>
                        {unit.product || unit.kind} · {unit.id}
                      </SelectItem>
                    ))}
                  </SelectContent>
                </Select>
              </div>
              {audioDevices.map((device) => (
                <DeviceCard
                  key={device.stable_id}
                  device={device}
                  expanded={expandedIds.has(device.stable_id)}
                  onToggle={() =>
                    setExpandedIds((prev) => {
                      const next = new Set(prev);
                      if (next.has(device.stable_id)) next.delete(device.stable_id);
                      else next.add(device.stable_id);
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
