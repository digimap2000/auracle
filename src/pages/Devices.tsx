import { useState, useMemo, useEffect, useCallback } from "react";
import { Play, Square, Search, Bluetooth } from "lucide-react";
import { Button } from "@/components/ui/button";
import { Card, CardContent } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import { ScrollArea } from "@/components/ui/scroll-area";
import { Separator } from "@/components/ui/separator";
import { Header } from "@/components/layout/Header";
import { cn } from "@/lib/utils";
import type { BleDevice } from "@/lib/tauri";
import bleCompanyIds from "@/data/ble-company-ids.json";

// --- Lookup maps ---

const KNOWN_SERVICES: Record<string, string> = {
  "00001800-0000-1000-8000-00805f9b34fb": "Generic Access",
  "00001801-0000-1000-8000-00805f9b34fb": "Generic Attribute",
  "0000180a-0000-1000-8000-00805f9b34fb": "Device Information",
  "0000180d-0000-1000-8000-00805f9b34fb": "Heart Rate",
  "0000180f-0000-1000-8000-00805f9b34fb": "Battery Service",
  "0000184e-0000-1000-8000-00805f9b34fb": "Audio Stream Control",
  "0000184f-0000-1000-8000-00805f9b34fb": "Broadcast Audio Scan",
  "00001850-0000-1000-8000-00805f9b34fb": "Published Audio Capabilities",
  "00001851-0000-1000-8000-00805f9b34fb": "Basic Audio Profile",
  "3e1d50cd-7e3e-427d-8e1c-b78aa87fe624": "Focal Naim Auracast",
};

const companyLookup = bleCompanyIds as Record<string, string>;

function resolveServiceName(uuid: string): string {
  return KNOWN_SERVICES[uuid.toLowerCase()] ?? uuid;
}

function resolveCompanyName(id: number): string | null {
  return companyLookup[String(id)] ?? null;
}

function formatHexBytes(bytes: number[]): string {
  return bytes.map((b) => b.toString(16).padStart(2, "0").toUpperCase()).join(" ");
}

function rssiColor(rssi: number): string {
  if (rssi > -50) return "text-green-500";
  if (rssi >= -70) return "text-yellow-500";
  return "text-red-500";
}

function relativeTime(isoString: string): string {
  const diff = Math.floor((Date.now() - new Date(isoString).getTime()) / 1000);
  if (diff < 1) return "now";
  if (diff < 60) return `${diff}s ago`;
  if (diff < 3600) return `${Math.floor(diff / 60)}m ago`;
  return `${Math.floor(diff / 3600)}h ago`;
}

function isStale(isoString: string): boolean {
  return Date.now() - new Date(isoString).getTime() > 30_000;
}

// --- Components ---

interface DevicesProps {
  bleDevices: BleDevice[];
  scanning: boolean;
  onStartScan: () => void;
  onStopScan: () => void;
}

export function Devices({
  bleDevices,
  scanning,
  onStartScan,
  onStopScan,
}: DevicesProps) {
  const [filter, setFilter] = useState("");
  const [selectedId, setSelectedId] = useState<string | null>(null);
  const [, setTick] = useState(0);

  // Update relative times every second
  useEffect(() => {
    const id = setInterval(() => setTick((t) => t + 1), 1000);
    return () => clearInterval(id);
  }, []);

  const filteredDevices = useMemo(() => {
    const q = filter.toLowerCase();
    const filtered = q
      ? bleDevices.filter(
          (d) =>
            d.name.toLowerCase().includes(q) ||
            d.id.toLowerCase().includes(q)
        )
      : bleDevices;
    return [...filtered].sort((a, b) => {
      const nameA = a.name === "Unknown" ? "\uffff" : a.name;
      const nameB = b.name === "Unknown" ? "\uffff" : b.name;
      const nameCmp = nameA.localeCompare(nameB);
      return nameCmp !== 0 ? nameCmp : b.rssi - a.rssi;
    });
  }, [bleDevices, filter]);

  const selectedDevice = useMemo(
    () => bleDevices.find((d) => d.id === selectedId) ?? null,
    [bleDevices, selectedId]
  );

  const handleToggleScan = useCallback(() => {
    if (scanning) {
      onStopScan();
    } else {
      onStartScan();
    }
  }, [scanning, onStartScan, onStopScan]);

  return (
    <div className="flex h-full flex-col">
      <Header title="Devices" description="Discover and inspect BLE peripherals" />

      {/* Toolbar */}
      <div className="flex items-center gap-2 border-b px-4 py-2">
        <Button
          size="sm"
          variant={scanning ? "destructive" : "default"}
          onClick={handleToggleScan}
          className="gap-1.5"
        >
          {scanning ? <Square size={14} /> : <Play size={14} />}
          {scanning ? "Stop" : "Start Scan"}
        </Button>
        <div className="relative flex-1">
          <Search
            size={14}
            className="absolute left-2.5 top-1/2 -translate-y-1/2 text-muted-foreground"
          />
          <input
            type="text"
            placeholder="Filter devices..."
            value={filter}
            onChange={(e) => setFilter(e.target.value)}
            className="h-7 w-full rounded-md border bg-background pl-8 pr-3 text-xs text-foreground placeholder:text-muted-foreground focus:outline-none focus:ring-1 focus:ring-ring"
          />
        </div>
        <div className="flex items-center gap-1.5">
          {scanning && (
            <span className="flex items-center gap-1">
              <span className="h-1.5 w-1.5 animate-pulse rounded-full bg-primary" />
              <span className="text-[11px] text-primary">Scanning</span>
            </span>
          )}
          <span className="text-[11px] text-muted-foreground">
            {filteredDevices.length} device{filteredDevices.length !== 1 ? "s" : ""}
          </span>
        </div>
      </div>

      {/* Two-panel layout */}
      <div
        className={cn(
          "flex-1 overflow-hidden",
          scanning && "ring-1 ring-primary/20"
        )}
      >
        {bleDevices.length === 0 ? (
          <div className="flex h-full items-center justify-center p-4">
            <Card>
              <CardContent className="flex flex-col items-center gap-2 p-8">
                <Bluetooth size={16} className="text-muted-foreground" />
                <p className="text-center text-xs text-muted-foreground">
                  {scanning
                    ? "Scanning for nearby devices..."
                    : "Click Start Scan to discover nearby Bluetooth devices"}
                </p>
              </CardContent>
            </Card>
          </div>
        ) : (
          <div className="grid h-full grid-cols-[1fr_minmax(320px,0.4fr)]">
            {/* Device list */}
            <ScrollArea className="border-r">
              <div className="space-y-0 p-2">
                {filteredDevices.map((device) => {
                  const stale = isStale(device.last_seen);
                  const selected = device.id === selectedId;
                  return (
                    <button
                      key={device.id}
                      onClick={() => setSelectedId(device.id)}
                      className={cn(
                        "flex w-full items-center justify-between rounded-md px-3 py-2 text-left transition-colors",
                        selected
                          ? "border-l-2 border-l-primary bg-accent/50"
                          : "hover:bg-accent/30",
                        stale && "opacity-50"
                      )}
                    >
                      <div className="min-w-0 flex-1">
                        <p className={cn(
                          "truncate text-sm font-medium",
                          device.name === "Unknown" && "italic text-muted-foreground"
                        )}>
                          {device.name}
                        </p>
                      </div>
                      <div className="ml-3 flex shrink-0 flex-col items-end gap-0.5">
                        <span className={cn("font-mono text-[10px] font-medium", rssiColor(device.rssi))}>
                          {device.rssi} dBm
                        </span>
                        <span className="text-[10px] text-muted-foreground">
                          {relativeTime(device.last_seen)}
                        </span>
                      </div>
                    </button>
                  );
                })}
              </div>
            </ScrollArea>

            {/* Detail pane */}
            {selectedDevice ? (
              <DeviceDetail device={selectedDevice} />
            ) : (
              <div className="flex flex-col items-center justify-center gap-2 p-4">
                <Bluetooth size={16} className="text-muted-foreground" />
                <p className="text-xs text-muted-foreground">
                  Select a device to view details
                </p>
              </div>
            )}
          </div>
        )}
      </div>
    </div>
  );
}

function SectionHeading({ children }: { children: React.ReactNode }) {
  return (
    <h3 className="text-[11px] font-medium uppercase tracking-wider text-muted-foreground">
      {children}
    </h3>
  );
}

function DeviceDetail({ device }: { device: BleDevice }) {
  return (
    <ScrollArea className="h-full">
      <div className="space-y-4 p-4">
        {/* Header */}
        <div>
          <h2 className={cn(
            "text-lg font-medium",
            device.name === "Unknown" && "italic text-muted-foreground"
          )}>
            {device.name}
          </h2>
          <div className="flex items-center gap-1.5">
            <span className="text-[10px] text-muted-foreground">
              {navigator.platform.startsWith("Mac") ? "Peripheral ID (macOS)" : "MAC Address"}
            </span>
          </div>
          <p className="font-mono text-xs text-muted-foreground">{device.id}</p>
        </div>

        <Separator />

        {/* Signal */}
        <div className="space-y-1.5">
          <SectionHeading>Signal</SectionHeading>
          <div className="flex items-center gap-3 text-sm">
            <span className={cn("font-mono font-medium", rssiColor(device.rssi))}>
              {device.rssi} dBm
            </span>
            <span className="text-muted-foreground">RSSI</span>
          </div>
          <div className="flex items-center gap-3 text-sm">
            {device.tx_power !== null ? (
              <>
                <span className="font-mono font-medium">{device.tx_power} dBm</span>
                <span className="text-muted-foreground">TX Power</span>
              </>
            ) : (
              <span className="text-[11px] text-muted-foreground">TX power not advertised</span>
            )}
          </div>
        </div>

        <Separator />

        {/* Services */}
        <div className="space-y-1.5">
          <SectionHeading>Services</SectionHeading>
          {device.services.length > 0 ? (
            <div className="space-y-1">
              {device.services.map((uuid, i) => {
                const name = resolveServiceName(uuid);
                const isKnown = name !== uuid;
                return (
                  <div key={i} className="flex items-center gap-2">
                    {isKnown && (
                      <Badge variant="outline" className="text-[10px]">{name}</Badge>
                    )}
                    <span className="font-mono text-[10px] text-muted-foreground">{uuid}</span>
                  </div>
                );
              })}
            </div>
          ) : (
            <p className="text-[11px] text-muted-foreground">No services advertised</p>
          )}
        </div>

        <Separator />

        {/* Manufacturer Data */}
        <div className="space-y-1.5">
          <SectionHeading>Manufacturer Data</SectionHeading>
          {device.manufacturer_data.length > 0 ? (
            <div className="space-y-2">
              {device.manufacturer_data.map((md, i) => {
                const company = resolveCompanyName(md.company_id);
                return (
                  <div key={i} className="space-y-0.5">
                    <div className="flex items-center gap-2">
                      {company ? (
                        <Badge variant="outline" className="text-[10px]">{company}</Badge>
                      ) : null}
                      <span className="font-mono text-[10px] text-muted-foreground">
                        0x{md.company_id.toString(16).padStart(4, "0").toUpperCase()}
                      </span>
                    </div>
                    <p className="font-mono text-[10px] text-muted-foreground leading-relaxed">
                      {formatHexBytes(md.data)}
                    </p>
                  </div>
                );
              })}
            </div>
          ) : (
            <p className="text-[11px] text-muted-foreground">No manufacturer data</p>
          )}
        </div>

        <Separator />

        {/* Timing */}
        <div className="space-y-1.5">
          <SectionHeading>Timing</SectionHeading>
          <div className="flex items-center gap-3 text-sm">
            <span className="font-mono text-[11px]">
              {new Date(device.last_seen).toLocaleTimeString()}
            </span>
            <span className="text-[11px] text-muted-foreground">Last seen</span>
          </div>
        </div>
      </div>
    </ScrollArea>
  );
}
