import { useEffect, useMemo, useRef, useState } from "react";
import { Bluetooth } from "lucide-react";
import { Badge } from "@/components/ui/badge";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { ScrollArea } from "@/components/ui/scroll-area";
import { Separator } from "@/components/ui/separator";
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/select";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { DeviceCard } from "@/components/devices/DeviceCard";
import {
  formatHexBlock,
  formatPacketTimestamp,
  isStandardUuid,
  parseAdvertisementStructures,
  relativeTime,
  resolveCompanyName,
  resolveServiceName,
} from "@/lib/ble-utils";
import type { BleDevice, BlePacket, DaemonUnit } from "@/lib/tauri";

const RSSI_WINDOW_SIZE = 6;
const MAX_PACKETS_IN_VIEW = 300;
const TIMELINE_WINDOW_MS = 60_000;
const TIMELINE_BUCKETS = 24;

interface DevicesProps {
  units: DaemonUnit[];
  bleDevices: BleDevice[];
  blePackets: BlePacket[];
  scanning: boolean;
}

function canScan(unit: DaemonUnit) {
  return unit.present && unit.capabilities.includes("ble-scan");
}

function EmptyState({
  scanning,
  selectedUnit,
}: {
  scanning: boolean;
  selectedUnit: DaemonUnit | null;
}) {
  if (!selectedUnit) {
    return (
      <div className="flex h-full items-center justify-center">
        <div className="text-center text-muted-foreground">
          <Bluetooth className="mx-auto mb-3 size-6 opacity-30" />
          <p className="text-sm">No scannable units are currently available</p>
        </div>
      </div>
    );
  }

  return (
    <div className="flex h-full items-center justify-center">
      <div className="text-center text-muted-foreground">
        <Bluetooth className="mx-auto mb-3 size-6 animate-pulse opacity-50" />
        <p className="text-sm">
          {scanning
            ? `Scanning from ${selectedUnit.product || selectedUnit.kind}...`
            : `Waiting for scan results from ${selectedUnit.product || selectedUnit.kind}...`}
        </p>
      </div>
    </div>
  );
}

export function Devices({ units, bleDevices, blePackets, scanning }: DevicesProps) {
  const [, setTick] = useState(0);
  useEffect(() => {
    const id = setInterval(() => setTick((t) => t + 1), 1000);
    return () => clearInterval(id);
  }, []);

  const [expandedIds, setExpandedIds] = useState<Set<string>>(new Set());
  const [selectedPacketId, setSelectedPacketId] = useState<string | null>(null);
  const [selectedUnitId, setSelectedUnitId] = useState<string | null>(null);
  const rssiWindows = useRef<Map<string, number[]>>(new Map());

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

  const unitBleDevices = useMemo(() => {
    if (!selectedUnitId) {
      return [];
    }
    return bleDevices.filter((device) => device.unit_id === selectedUnitId);
  }, [bleDevices, selectedUnitId]);

  const unitBlePackets = useMemo(() => {
    if (!selectedUnitId) {
      return [];
    }
    return blePackets.filter((packet) => packet.unit_id === selectedUnitId);
  }, [blePackets, selectedUnitId]);

  const recentPackets = useMemo(
    () => unitBlePackets.slice(-MAX_PACKETS_IN_VIEW).reverse(),
    [unitBlePackets]
  );

  const observedFieldsByStableId = useMemo(() => {
    const grouped = new Map<
      string,
      Map<
        string,
        {
          key: string;
          source: "adv" | "scan-response";
          typeName: string;
          typeHex: string;
          summary: string;
          hex: string;
          count: number;
          lastSeenMs: number;
        }
      >
    >();

    const ingestFields = (
      stableId: string,
      source: "adv" | "scan-response",
      payload: number[],
      timestampMs: number
    ) => {
      if (payload.length === 0) {
        return;
      }

      let fields = grouped.get(stableId);
      if (!fields) {
        fields = new Map();
        grouped.set(stableId, fields);
      }

      for (const field of parseAdvertisementStructures(payload)) {
        const typeHex = field.type >= 0
          ? `0x${field.type.toString(16).padStart(2, "0").toUpperCase()}`
          : "0x??";
        const key = `${source}:${typeHex}:${field.hex}`;
        const existing = fields.get(key);
        if (existing) {
          existing.count += 1;
          existing.lastSeenMs = Math.max(existing.lastSeenMs, timestampMs);
          continue;
        }

        fields.set(key, {
          key,
          source,
          typeName: field.typeName,
          typeHex,
          summary: field.summary,
          hex: field.hex,
          count: 1,
          lastSeenMs: timestampMs,
        });
      }
    };

    for (const packet of unitBlePackets) {
      ingestFields(packet.stable_id, "adv", packet.raw_data, packet.timestamp_ms);
      ingestFields(packet.stable_id, "scan-response", packet.raw_scan_response, packet.timestamp_ms);
    }

    return new Map(
      [...grouped.entries()].map(([stableId, fields]) => [
        stableId,
        [...fields.values()]
          .sort((a, b) =>
            b.lastSeenMs - a.lastSeenMs
            || b.count - a.count
            || a.typeName.localeCompare(b.typeName)
            || a.hex.localeCompare(b.hex)
          )
          .map((field) => ({
            ...field,
            lastSeenIso: new Date(field.lastSeenMs).toISOString(),
          })),
      ])
    );
  }, [unitBlePackets]);

  useEffect(() => {
    if (recentPackets.length === 0) {
      setSelectedPacketId(null);
      return;
    }

    if (!selectedPacketId || !recentPackets.some((packet) => packet.id === selectedPacketId)) {
      setSelectedPacketId(recentPackets[0]?.id ?? null);
    }
  }, [recentPackets, selectedPacketId]);

  const selectedPacket = useMemo(
    () => recentPackets.find((packet) => packet.id === selectedPacketId) ?? null,
    [recentPackets, selectedPacketId]
  );

  const packetCounts = useMemo(() => {
    const counts = new Map<string, number>();
    for (const packet of unitBlePackets) {
      counts.set(packet.stable_id, (counts.get(packet.stable_id) ?? 0) + 1);
    }
    return counts;
  }, [unitBlePackets]);

  const strongestRssi = useMemo(() => {
    const strongest = new Map<string, number>();
    for (const packet of unitBlePackets) {
      const existing = strongest.get(packet.stable_id);
      if (existing == null || packet.rssi > existing) {
        strongest.set(packet.stable_id, packet.rssi);
      }
    }
    return strongest;
  }, [unitBlePackets]);

  const firstSeenAt = useMemo(() => {
    const firstSeen = new Map<string, number>();
    for (const packet of unitBlePackets) {
      const existing = firstSeen.get(packet.stable_id);
      if (existing == null || packet.timestamp_ms < existing) {
        firstSeen.set(packet.stable_id, packet.timestamp_ms);
      }
    }
    return firstSeen;
  }, [unitBlePackets]);

  const sortedDevices = useMemo(() => {
    const windows = rssiWindows.current;

    const smoothed = unitBleDevices.map((device) => {
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

      return {
        ...device,
        rssi: avg,
        packetCount: packetCounts.get(device.stable_id) ?? 0,
        strongestRssi: strongestRssi.get(device.stable_id) ?? avg,
        firstSeenAt: firstSeenAt.get(device.stable_id) ?? Number.MAX_SAFE_INTEGER,
      };
    });

    const activeIds = new Set(unitBleDevices.map((device) => device.stable_id));
    for (const id of windows.keys()) {
      if (!activeIds.has(id)) {
        windows.delete(id);
      }
    }

    return smoothed.sort((a, b) => {
      const aUnknown = a.name === "Unknown";
      const bUnknown = b.name === "Unknown";
      if (aUnknown !== bUnknown) {
        return aUnknown ? 1 : -1;
      }
      if (!aUnknown && !bUnknown && a.name !== b.name) {
        return a.name.localeCompare(b.name);
      }

      if (a.firstSeenAt !== b.firstSeenAt) {
        return a.firstSeenAt - b.firstSeenAt;
      }

      return a.id.localeCompare(b.id);
    });
  }, [firstSeenAt, packetCounts, strongestRssi, unitBleDevices]);

  const serviceSummary = useMemo(() => {
    const counts = new Map<string, number>();

    for (const device of unitBleDevices) {
      const uniqueServices = new Set(device.services.map((service) => service.toLowerCase()));
      for (const service of uniqueServices) {
        counts.set(service, (counts.get(service) ?? 0) + 1);
      }
    }

    return [...counts.entries()]
      .map(([uuid, count]) => ({
        uuid,
        label: resolveServiceName(uuid),
        count,
        isStandard: isStandardUuid(uuid),
      }))
      .sort((a, b) => b.count - a.count || a.label.localeCompare(b.label));
  }, [unitBleDevices]);

  const companySummary = useMemo(() => {
    const counts = new Map<number, number>();

    for (const packet of unitBlePackets) {
      if (packet.company_id != null) {
        counts.set(packet.company_id, (counts.get(packet.company_id) ?? 0) + 1);
      }
    }

    return [...counts.entries()]
      .map(([companyId, count]) => ({
        companyId,
        label: resolveCompanyName(companyId) ?? "Unknown Company",
        count,
      }))
      .sort((a, b) => b.count - a.count || a.label.localeCompare(b.label));
  }, [unitBlePackets]);

  const uniqueServicesCount = serviceSummary.length;
  const namedDevicesCount = unitBleDevices.filter((device) => device.name !== "Unknown").length;
  const packetRate = useMemo(() => {
    const now = Date.now();
    const recent = unitBlePackets.filter((packet) => now - packet.timestamp_ms <= 10_000);
    return recent.length / 10;
  }, [unitBlePackets]);

  const timelineBuckets = useMemo(() => {
    const now = Date.now();
    const bucketWidth = TIMELINE_WINDOW_MS / TIMELINE_BUCKETS;
    const buckets = Array.from({ length: TIMELINE_BUCKETS }, (_, index) => ({
      label: `${Math.round((TIMELINE_BUCKETS - index) * (bucketWidth / 1000))}s`,
      count: 0,
      namedCount: 0,
    }));

    for (const packet of unitBlePackets) {
      const age = now - packet.timestamp_ms;
      if (age < 0 || age > TIMELINE_WINDOW_MS) {
        continue;
      }

      const index = TIMELINE_BUCKETS - 1 - Math.floor(age / bucketWidth);
      const bucket = buckets[index];
      if (bucket && index >= 0 && index < TIMELINE_BUCKETS) {
        bucket.count += 1;
        if (packet.name !== "Unknown") {
          bucket.namedCount += 1;
        }
      }
    }

    return buckets;
  }, [unitBlePackets]);

  const maxBucketCount = Math.max(...timelineBuckets.map((bucket) => bucket.count), 1);

  if (!selectedUnit) {
    return <EmptyState scanning={scanning} selectedUnit={selectedUnit} />;
  }

  return (
    <div className="flex h-full min-h-0 flex-col gap-4 px-4 py-4 md:px-6">
      <Card className="border-border/60">
        <CardHeader className="gap-3 md:flex-row md:items-start md:justify-between">
          <div>
            <CardTitle className="text-base">
              Scan Diagnostics
            </CardTitle>
            <CardDescription>
              {selectedUnit.product || selectedUnit.kind} on {selectedUnit.id}
            </CardDescription>
          </div>
          <div className="flex flex-wrap items-center gap-2">
            <Select value={selectedUnitId ?? undefined} onValueChange={setSelectedUnitId}>
              <SelectTrigger className="h-8 min-w-64 bg-background" size="sm">
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
            <Badge variant="outline">{unitBleDevices.length} devices</Badge>
            <Badge variant="outline">{unitBlePackets.length} packets</Badge>
            <Badge variant="outline">{uniqueServicesCount} services</Badge>
            <Badge variant={scanning ? "default" : "secondary"}>
              {scanning ? `${packetRate.toFixed(1)} pkt/s` : "Idle"}
            </Badge>
          </div>
        </CardHeader>
      </Card>

      <Tabs defaultValue="summary" className="min-h-0 flex-1">
        <TabsList variant="line" className="w-full justify-start rounded-none border-b bg-transparent p-0">
          <TabsTrigger value="summary" className="rounded-none px-4">Summary</TabsTrigger>
          <TabsTrigger value="timeline" className="rounded-none px-4">Timeline</TabsTrigger>
          <TabsTrigger value="packets" className="rounded-none px-4">Packets</TabsTrigger>
        </TabsList>

        <TabsContent value="summary" className="min-h-0 flex-1 pt-4">
          <div className="grid h-full min-h-0 gap-4 xl:grid-cols-[minmax(0,1.55fr)_minmax(320px,0.95fr)]">
            <Card className="min-h-0">
              <CardHeader>
                <CardTitle>Detected Devices</CardTitle>
                <CardDescription>
                  Current deduplicated view for the selected scanning unit.
                </CardDescription>
              </CardHeader>
              <CardContent className="min-h-0">
                <ScrollArea className="h-[calc(100vh-22rem)] xl:h-[calc(100vh-19rem)]">
                  <div className="space-y-3 pr-4">
                    {sortedDevices.map((device) => (
                      <div key={device.stable_id} className="space-y-2">
                        <DeviceCard
                          device={device}
                          expanded={expandedIds.has(device.stable_id)}
                          observedFields={observedFieldsByStableId.get(device.stable_id)}
                          onToggle={() =>
                            setExpandedIds((prev) => {
                              const next = new Set(prev);
                              if (next.has(device.stable_id)) next.delete(device.stable_id);
                              else next.add(device.stable_id);
                              return next;
                            })
                          }
                        />
                        <div className="flex flex-wrap gap-2 pl-3 text-xs text-muted-foreground">
                          <span>{device.packetCount} packets</span>
                          <span>strongest {device.strongestRssi} dBm</span>
                          <span>last seen {relativeTime(device.last_seen)}</span>
                        </div>
                      </div>
                    ))}
                  </div>
                </ScrollArea>
              </CardContent>
            </Card>

            <div className="grid min-h-0 gap-4">
              <Card>
                <CardHeader>
                  <CardTitle>Coverage</CardTitle>
                  <CardDescription>Quick health view of the current scan window.</CardDescription>
                </CardHeader>
                <CardContent className="grid grid-cols-2 gap-3 text-sm">
                  <MetricCard label="Named Devices" value={namedDevicesCount} />
                  <MetricCard label="Unnamed Devices" value={unitBleDevices.length - namedDevicesCount} />
                  <MetricCard label="Unique Services" value={uniqueServicesCount} />
                  <MetricCard label="Recent Packets" value={unitBlePackets.filter((packet) => Date.now() - packet.timestamp_ms <= 30_000).length} />
                </CardContent>
              </Card>

              <Card className="min-h-0">
                <CardHeader>
                  <CardTitle>Service Inventory</CardTitle>
                  <CardDescription>Services currently visible across deduplicated devices.</CardDescription>
                </CardHeader>
                <CardContent className="min-h-0">
                  <ScrollArea className="h-48">
                    <div className="space-y-2 pr-4">
                      {serviceSummary.length === 0 ? (
                        <p className="text-sm text-muted-foreground">No services decoded yet.</p>
                      ) : (
                        serviceSummary.map((service) => (
                          <div key={service.uuid} className="flex items-start justify-between gap-3 rounded-md border border-border/60 px-3 py-2">
                            <div className="min-w-0">
                              <p className="truncate text-sm font-medium">{service.label}</p>
                              <p className="truncate font-mono text-xs text-muted-foreground">{service.uuid}</p>
                            </div>
                            <div className="flex shrink-0 items-center gap-2">
                              <Badge variant={service.isStandard ? "secondary" : "outline"}>
                                {service.isStandard ? "SIG" : "Custom"}
                              </Badge>
                              <span className="text-sm text-muted-foreground">{service.count}</span>
                            </div>
                          </div>
                        ))
                      )}
                    </div>
                  </ScrollArea>
                </CardContent>
              </Card>

              <Card className="min-h-0">
                <CardHeader>
                  <CardTitle>Vendors on Air</CardTitle>
                  <CardDescription>Manufacturer identifiers seen in the live packet stream.</CardDescription>
                </CardHeader>
                <CardContent className="min-h-0">
                  <ScrollArea className="h-48">
                    <div className="space-y-2 pr-4">
                      {companySummary.length === 0 ? (
                        <p className="text-sm text-muted-foreground">No manufacturer data seen yet.</p>
                      ) : (
                        companySummary.map((company) => (
                          <div key={company.companyId} className="flex items-center justify-between rounded-md border border-border/60 px-3 py-2">
                            <div>
                              <p className="text-sm font-medium">{company.label}</p>
                              <p className="font-mono text-xs text-muted-foreground">
                                0x{company.companyId.toString(16).padStart(4, "0").toUpperCase()}
                              </p>
                            </div>
                            <span className="text-sm text-muted-foreground">{company.count} packets</span>
                          </div>
                        ))
                      )}
                    </div>
                  </ScrollArea>
                </CardContent>
              </Card>
            </div>
          </div>
        </TabsContent>

        <TabsContent value="timeline" className="min-h-0 flex-1 pt-4">
          <div className="grid h-full min-h-0 gap-4 xl:grid-cols-[minmax(0,1.2fr)_minmax(360px,0.8fr)]">
            <Card className="min-h-0">
              <CardHeader>
                <CardTitle>Packet Arrival Timeline</CardTitle>
                <CardDescription>
                  Packet density over the last {TIMELINE_WINDOW_MS / 1000} seconds.
                </CardDescription>
              </CardHeader>
              <CardContent className="flex h-full min-h-0 flex-col gap-6">
                <div className="grid grid-cols-2 gap-3 md:grid-cols-4">
                  <MetricCard label="Peak Bucket" value={maxBucketCount} />
                  <MetricCard label="Average Rate" value={`${packetRate.toFixed(1)}/s`} />
                  <MetricCard label="Last Packet" value={recentPackets[0] ? relativeTime(new Date(recentPackets[0].timestamp_ms).toISOString()) : "n/a"} />
                  <MetricCard label="Timeline Samples" value={timelineBuckets.reduce((sum, bucket) => sum + bucket.count, 0)} />
                </div>

                <div className="flex min-h-[14rem] items-end gap-2 rounded-lg border border-border/60 bg-muted/20 p-4">
                  {timelineBuckets.map((bucket, index) => {
                    const height = Math.max(8, (bucket.count / maxBucketCount) * 100);
                    return (
                      <div key={`${bucket.label}-${index}`} className="flex flex-1 flex-col items-center gap-2">
                        <div className="text-[10px] text-muted-foreground">{bucket.count}</div>
                        <div className="relative flex w-full flex-1 items-end">
                          <div
                            className="w-full rounded-t-md bg-foreground/85 transition-[height]"
                            style={{ height: `${height}%` }}
                          />
                          {bucket.namedCount > 0 && (
                            <div
                              className="absolute inset-x-[18%] bottom-0 rounded-t-md bg-emerald-400/70"
                              style={{ height: `${Math.max(6, (bucket.namedCount / maxBucketCount) * 100)}%` }}
                            />
                          )}
                        </div>
                        <div className="text-[10px] text-muted-foreground">
                          -{Math.round(((TIMELINE_BUCKETS - index - 1) * TIMELINE_WINDOW_MS) / TIMELINE_BUCKETS / 1000)}s
                        </div>
                      </div>
                    );
                  })}
                </div>

                <p className="text-xs text-muted-foreground">
                  Dark bars show total packets per bucket. Green overlays show packets where a name was present in the received advertisement.
                </p>
              </CardContent>
            </Card>

            <Card className="min-h-0">
              <CardHeader>
                <CardTitle>Recent Packet Feed</CardTitle>
                <CardDescription>Arrival order for the newest packets on this unit.</CardDescription>
              </CardHeader>
              <CardContent className="min-h-0">
                <ScrollArea className="h-[calc(100vh-19rem)]">
                  <div className="space-y-2 pr-4">
                    {recentPackets.map((packet) => (
                      <div key={packet.id} className="rounded-md border border-border/60 px-3 py-2">
                        <div className="flex items-center justify-between gap-3">
                          <div className="min-w-0">
                            <p className="truncate text-sm font-medium">
                              {packet.name !== "Unknown" ? packet.name : packet.device_id}
                            </p>
                            <p className="truncate font-mono text-xs text-muted-foreground">
                              {packet.device_id}
                            </p>
                          </div>
                          <div className="text-right">
                            <p className="text-sm font-medium">{packet.rssi} dBm</p>
                            <p className="text-xs text-muted-foreground">{formatPacketTimestamp(packet.timestamp_ms)}</p>
                          </div>
                        </div>
                        <div className="mt-2 flex flex-wrap gap-2 text-xs text-muted-foreground">
                          {packet.company_id != null && (
                            <Badge variant="outline">
                              {resolveCompanyName(packet.company_id) ?? "Unknown"} · 0x{packet.company_id.toString(16).padStart(4, "0").toUpperCase()}
                            </Badge>
                          )}
                          {packet.service_uuids.slice(0, 2).map((uuid) => (
                            <Badge key={uuid} variant="secondary">
                              {resolveServiceName(uuid)}
                            </Badge>
                          ))}
                          <span>{packet.raw_data.length} B payload</span>
                        </div>
                      </div>
                    ))}
                  </div>
                </ScrollArea>
              </CardContent>
            </Card>
          </div>
        </TabsContent>

        <TabsContent value="packets" className="min-h-0 flex-1 pt-4">
          <div className="grid h-full min-h-0 gap-4 xl:grid-cols-[minmax(0,1.05fr)_minmax(360px,0.95fr)]">
            <Card className="min-h-0">
              <CardHeader>
                <CardTitle>Packet Stream</CardTitle>
                <CardDescription>
                  Live packet log for the selected unit. Select a row to inspect the payload.
                </CardDescription>
              </CardHeader>
              <CardContent className="min-h-0">
                <ScrollArea className="h-[calc(100vh-19rem)]">
                  <div className="space-y-2 pr-4">
                    {recentPackets.map((packet) => (
                      <button
                        key={packet.id}
                        type="button"
                        onClick={() => setSelectedPacketId(packet.id)}
                        className={`w-full rounded-lg border px-3 py-3 text-left transition-colors ${
                          selectedPacketId === packet.id
                            ? "border-foreground/40 bg-accent"
                            : "border-border/60 hover:border-foreground/20 hover:bg-accent/40"
                        }`}
                      >
                        <div className="flex items-start justify-between gap-3">
                          <div className="min-w-0">
                            <p className="truncate text-sm font-medium">
                              {packet.name !== "Unknown" ? packet.name : "(unknown)"}{" "}
                              <span className="font-mono text-muted-foreground">{packet.device_id}</span>
                            </p>
                            <div className="mt-1 flex flex-wrap gap-2 text-xs text-muted-foreground">
                              <span>{formatPacketTimestamp(packet.timestamp_ms)}</span>
                              <span>{packet.address_type || "addr type unknown"}</span>
                              <span>adv type {packet.adv_type}</span>
                              <span>{packet.raw_data.length} B</span>
                            </div>
                          </div>
                          <div className="shrink-0 text-right">
                            <p className="text-sm font-medium">{packet.rssi} dBm</p>
                            <p className="text-xs text-muted-foreground">SID {packet.sid}</p>
                          </div>
                        </div>
                      </button>
                    ))}
                  </div>
                </ScrollArea>
              </CardContent>
            </Card>

            <Card className="min-h-0">
              <CardHeader>
                <CardTitle>Packet Inspector</CardTitle>
                <CardDescription>
                  Raw payloads, decoded AD structures, and radio metadata for the selected packet.
                </CardDescription>
              </CardHeader>
              <CardContent className="min-h-0">
                {selectedPacket ? (
                  <ScrollArea className="h-[calc(100vh-19rem)]">
                    <div className="space-y-4 pr-4">
                      <div className="rounded-lg border border-border/60 bg-muted/20 p-3">
                        <div className="flex flex-wrap items-center justify-between gap-3">
                          <div>
                            <p className="text-sm font-medium">
                              {selectedPacket.name !== "Unknown" ? selectedPacket.name : selectedPacket.device_id}
                            </p>
                            <p className="font-mono text-xs text-muted-foreground">{selectedPacket.device_id}</p>
                          </div>
                          <div className="text-right">
                            <p className="text-sm font-medium">{selectedPacket.rssi} dBm</p>
                            <p className="text-xs text-muted-foreground">{formatPacketTimestamp(selectedPacket.timestamp_ms)}</p>
                          </div>
                        </div>
                        <div className="mt-3 grid grid-cols-2 gap-2 text-xs text-muted-foreground">
                          <MetaPill label="Addr Type" value={selectedPacket.address_type || "unknown"} />
                          <MetaPill label="SID" value={selectedPacket.sid} />
                          <MetaPill label="Adv Type" value={selectedPacket.adv_type} />
                          <MetaPill label="Adv Props" value={`0x${selectedPacket.adv_props.toString(16).toUpperCase()}`} />
                          <MetaPill label="Interval" value={selectedPacket.interval} />
                          <MetaPill label="Primary PHY" value={selectedPacket.primary_phy} />
                          <MetaPill label="Secondary PHY" value={selectedPacket.secondary_phy} />
                          <MetaPill label="Tx Power" value={selectedPacket.tx_power ?? "unknown"} />
                        </div>
                      </div>

                      <PacketPayloadSection
                        title="Advertising Payload"
                        description={`${selectedPacket.raw_data.length} bytes`}
                        bytes={selectedPacket.raw_data}
                      />

                      {selectedPacket.raw_scan_response.length > 0 && (
                        <PacketPayloadSection
                          title="Scan Response"
                          description={`${selectedPacket.raw_scan_response.length} bytes`}
                          bytes={selectedPacket.raw_scan_response}
                        />
                      )}

                      <Card className="border-border/60">
                        <CardHeader>
                          <CardTitle>Decoded Fields</CardTitle>
                          <CardDescription>
                            Best-effort interpretation of AD structures in the selected packet.
                          </CardDescription>
                        </CardHeader>
                        <CardContent className="space-y-2">
                          {parseAdvertisementStructures(selectedPacket.raw_data).map((field) => (
                            <div key={`${field.offset}-${field.type}`} className="rounded-md border border-border/60 px-3 py-2">
                              <div className="flex items-center justify-between gap-3">
                                <p className="text-sm font-medium">{field.typeName}</p>
                                <span className="font-mono text-xs text-muted-foreground">
                                  offset {field.offset} · len {field.length}
                                </span>
                              </div>
                              <p className="mt-1 text-sm text-muted-foreground">{field.summary}</p>
                              <p className="mt-1 font-mono text-xs text-muted-foreground">{field.hex || "No field data"}</p>
                            </div>
                          ))}
                          {selectedPacket.raw_data.length === 0 && (
                            <p className="text-sm text-muted-foreground">No raw payload available for this packet.</p>
                          )}
                        </CardContent>
                      </Card>
                    </div>
                  </ScrollArea>
                ) : (
                  <p className="text-sm text-muted-foreground">Select a packet to inspect it.</p>
                )}
              </CardContent>
            </Card>
          </div>
        </TabsContent>
      </Tabs>
    </div>
  );
}

function MetricCard({ label, value }: { label: string; value: number | string }) {
  return (
    <div className="rounded-lg border border-border/60 bg-muted/20 px-3 py-2">
      <p className="text-xs uppercase tracking-[0.12em] text-muted-foreground">{label}</p>
      <p className="mt-1 text-lg font-semibold">{value}</p>
    </div>
  );
}

function MetaPill({ label, value }: { label: string; value: number | string }) {
  return (
    <div className="rounded-md border border-border/60 px-2 py-1">
      <span className="text-[10px] uppercase tracking-[0.12em] text-muted-foreground">{label}</span>
      <p className="truncate text-sm text-foreground">{value}</p>
    </div>
  );
}

function PacketPayloadSection({
  title,
  description,
  bytes,
}: {
  title: string;
  description: string;
  bytes: number[];
}) {
  return (
    <Card className="border-border/60">
      <CardHeader>
        <CardTitle>{title}</CardTitle>
        <CardDescription>{description}</CardDescription>
      </CardHeader>
      <CardContent className="space-y-3">
        <div className="rounded-md border border-border/60 bg-muted/20 p-3">
          <p className="font-mono text-xs leading-6 text-muted-foreground">
            {formatHexBlock(bytes, 8)}
          </p>
        </div>
        <Separator />
        <div className="grid gap-2 text-xs text-muted-foreground md:grid-cols-2">
          <span>{bytes.length} bytes total</span>
          <span>{Math.ceil(bytes.length / 8)} groups of 8 bytes</span>
        </div>
      </CardContent>
    </Card>
  );
}
