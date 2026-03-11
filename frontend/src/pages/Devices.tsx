import { useEffect, useMemo, useRef, useState } from "react";
import { Bluetooth } from "lucide-react";
import { Badge } from "@/components/ui/badge";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { ScrollArea } from "@/components/ui/scroll-area";
import { Separator } from "@/components/ui/separator";
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
  bleDevices: BleDevice[];
  blePackets: BlePacket[];
  scanning: boolean;
  activeUnit: DaemonUnit | null;
}

function EmptyState({
  scanning,
  activeUnit,
}: {
  scanning: boolean;
  activeUnit: DaemonUnit | null;
}) {
  if (!activeUnit) {
    return (
      <div className="flex h-full items-center justify-center">
        <div className="text-center text-muted-foreground">
          <Bluetooth className="mx-auto mb-3 size-6 opacity-30" />
          <p className="text-sm">Select an active unit on the Home page</p>
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
            ? `Scanning from ${activeUnit.product || activeUnit.kind}...`
            : `Waiting for scan results from ${activeUnit.product || activeUnit.kind}...`}
        </p>
      </div>
    </div>
  );
}

export function Devices({ bleDevices, blePackets, scanning, activeUnit }: DevicesProps) {
  const [, setTick] = useState(0);
  useEffect(() => {
    const id = setInterval(() => setTick((t) => t + 1), 1000);
    return () => clearInterval(id);
  }, []);

  const [expandedIds, setExpandedIds] = useState<Set<string>>(new Set());
  const [selectedPacketId, setSelectedPacketId] = useState<string | null>(null);
  const rssiWindows = useRef<Map<string, number[]>>(new Map());

  const recentPackets = useMemo(
    () => blePackets.slice(-MAX_PACKETS_IN_VIEW).reverse(),
    [blePackets]
  );

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
    for (const packet of blePackets) {
      counts.set(packet.device_id, (counts.get(packet.device_id) ?? 0) + 1);
    }
    return counts;
  }, [blePackets]);

  const strongestRssi = useMemo(() => {
    const strongest = new Map<string, number>();
    for (const packet of blePackets) {
      const existing = strongest.get(packet.device_id);
      if (existing == null || packet.rssi > existing) {
        strongest.set(packet.device_id, packet.rssi);
      }
    }
    return strongest;
  }, [blePackets]);

  const sortedDevices = useMemo(() => {
    const windows = rssiWindows.current;

    const smoothed = bleDevices.map((device) => {
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
        samples.reduce((sum, value) => sum + value, 0) / samples.length
      );

      return {
        ...device,
        rssi: avg,
        packetCount: packetCounts.get(device.id) ?? 0,
        strongestRssi: strongestRssi.get(device.id) ?? avg,
      };
    });

    const activeIds = new Set(bleDevices.map((device) => device.id));
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
      return b.rssi - a.rssi;
    });
  }, [bleDevices, packetCounts, strongestRssi]);

  const serviceSummary = useMemo(() => {
    const counts = new Map<string, number>();

    for (const device of bleDevices) {
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
  }, [bleDevices]);

  const companySummary = useMemo(() => {
    const counts = new Map<number, number>();

    for (const packet of blePackets) {
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
  }, [blePackets]);

  const uniqueServicesCount = serviceSummary.length;
  const namedDevicesCount = bleDevices.filter((device) => device.name !== "Unknown").length;
  const packetRate = useMemo(() => {
    const now = Date.now();
    const recent = blePackets.filter((packet) => now - packet.timestamp_ms <= 10_000);
    return recent.length / 10;
  }, [blePackets]);

  const timelineBuckets = useMemo(() => {
    const now = Date.now();
    const bucketWidth = TIMELINE_WINDOW_MS / TIMELINE_BUCKETS;
    const buckets = Array.from({ length: TIMELINE_BUCKETS }, (_, index) => ({
      label: `${Math.round((TIMELINE_BUCKETS - index) * (bucketWidth / 1000))}s`,
      count: 0,
      namedCount: 0,
    }));

    for (const packet of blePackets) {
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
  }, [blePackets]);

  const maxBucketCount = Math.max(...timelineBuckets.map((bucket) => bucket.count), 1);

  if (!activeUnit || (bleDevices.length === 0 && blePackets.length === 0)) {
    return <EmptyState scanning={scanning} activeUnit={activeUnit} />;
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
              {activeUnit.product || activeUnit.kind} on {activeUnit.id}
            </CardDescription>
          </div>
          <div className="flex flex-wrap items-center gap-2">
            <Badge variant="outline">{bleDevices.length} devices</Badge>
            <Badge variant="outline">{blePackets.length} packets</Badge>
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
                  Current deduplicated view for the active scanning unit.
                </CardDescription>
              </CardHeader>
              <CardContent className="min-h-0">
                <ScrollArea className="h-[calc(100vh-22rem)] xl:h-[calc(100vh-19rem)]">
                  <div className="space-y-3 pr-4">
                    {sortedDevices.map((device) => (
                      <div key={device.id} className="space-y-2">
                        <DeviceCard
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
                  <MetricCard label="Unnamed Devices" value={bleDevices.length - namedDevicesCount} />
                  <MetricCard label="Unique Services" value={uniqueServicesCount} />
                  <MetricCard label="Recent Packets" value={blePackets.filter((packet) => Date.now() - packet.timestamp_ms <= 30_000).length} />
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
                  Live packet log for the active unit. Select a row to inspect the payload.
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
