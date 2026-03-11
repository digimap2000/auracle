import { useEffect, useMemo, useRef, useState } from "react";
import { Bluetooth, Fingerprint } from "lucide-react";
import { Badge } from "@/components/ui/badge";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { ScrollArea } from "@/components/ui/scroll-area";
import {
  ResizableHandle,
  ResizablePanel,
  ResizablePanelGroup,
} from "@/components/ui/resizable";
import { Separator } from "@/components/ui/separator";
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/select";
import {
  Sidebar,
  SidebarContent,
  SidebarGroup,
  SidebarGroupContent,
  SidebarGroupLabel,
  SidebarHeader,
  SidebarMenu,
  SidebarMenuBadge,
  SidebarMenuButton,
  SidebarMenuItem,
  SidebarProvider,
  SidebarSeparator,
} from "@/components/ui/sidebar";
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
import {
  decodeDaemonAdvertisement,
  describeDaemonServiceDataFormats,
} from "@/lib/tauri";
import type {
  BleDevice,
  BlePacket,
  DaemonUnit,
  DecodedField,
  DecodedServiceData,
  ServiceDataEnumEntryMetadata,
  ServiceDataFieldMetadata,
  ServiceDataFormatMetadata,
} from "@/lib/tauri";

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

interface ResolvedServicePayloadObservation {
  key: string;
  serviceUuid: string;
  serviceLabel: string;
  rawValue: string;
  statusCode: number;
  statusMessage: string;
  fields: DecodedServiceData["fields"];
  count: number;
  lastSeenMs: number;
  variantCount: number;
}

const serviceDataFormatMetadataCache = new Map<string, ServiceDataFormatMetadata>();

function normalizeMetadataUuid(uuid: string) {
  return uuid.trim().toLowerCase();
}

function parseNumericFieldValue(value: string) {
  if (value.startsWith("0x") || value.startsWith("0X")) {
    const parsed = Number.parseInt(value.slice(2), 16);
    return Number.isNaN(parsed) ? null : parsed;
  }

  if (/^\d+$/.test(value)) {
    const parsed = Number.parseInt(value, 10);
    return Number.isNaN(parsed) ? null : parsed;
  }

  return null;
}

function resolveEnumEntries(
  field: DecodedField,
  metadata: ServiceDataFieldMetadata | undefined
) {
  if (!metadata || metadata.enum_entries.length === 0) {
    return [];
  }

  const numericValue = parseNumericFieldValue(field.value);
  if (numericValue == null) {
    return [];
  }

  return metadata.enum_entries.filter((entry) => {
    if (metadata.enum_match === "bitfield_all") {
      return entry.value !== 0 && (numericValue & entry.value) === entry.value;
    }
    return numericValue === entry.value;
  });
}

function useServiceDataFormatMetadata(serviceUuids: string[]) {
  const [metadataByUuid, setMetadataByUuid] = useState<Record<string, ServiceDataFormatMetadata>>({});
  const [metadataState, setMetadataState] = useState<"idle" | "loading" | "ready" | "error">("idle");
  const [metadataError, setMetadataError] = useState<string | null>(null);

  const normalizedUuids = useMemo(() => {
    const seen = new Set<string>();
    const unique: string[] = [];
    for (const uuid of serviceUuids) {
      const normalized = normalizeMetadataUuid(uuid);
      if (!normalized || seen.has(normalized)) {
        continue;
      }
      seen.add(normalized);
      unique.push(normalized);
    }
    return unique;
  }, [serviceUuids]);
  const normalizedUuidsKey = normalizedUuids.join("\u001F");

  useEffect(() => {
    let cancelled = false;

    if (normalizedUuids.length === 0) {
      setMetadataByUuid({});
      setMetadataState("ready");
      setMetadataError(null);
      return;
    }

    setMetadataByUuid(
      Object.fromEntries(
        normalizedUuids.flatMap((uuid) => {
          const metadata = serviceDataFormatMetadataCache.get(uuid);
          return metadata ? [[uuid, metadata] as const] : [];
        })
      )
    );

    const missing = normalizedUuids.filter((uuid) => !serviceDataFormatMetadataCache.has(uuid));
    if (missing.length === 0) {
      setMetadataState("ready");
      setMetadataError(null);
      return;
    }

    setMetadataState("loading");
    setMetadataError(null);

    describeDaemonServiceDataFormats(missing)
      .then((formats) => {
        if (cancelled) {
          return;
        }

        for (const format of formats) {
          serviceDataFormatMetadataCache.set(normalizeMetadataUuid(format.service_uuid), format);
        }

        setMetadataByUuid(
          Object.fromEntries(
            normalizedUuids.flatMap((uuid) => {
              const metadata = serviceDataFormatMetadataCache.get(uuid);
              return metadata ? [[uuid, metadata] as const] : [];
            })
          )
        );
        setMetadataState("ready");
      })
      .catch((error: unknown) => {
        if (cancelled) {
          return;
        }

        setMetadataState("error");
        setMetadataError(error instanceof Error ? error.message : String(error));
      });

    return () => {
      cancelled = true;
    };
  }, [normalizedUuidsKey]);

  return { metadataByUuid, metadataState, metadataError };
}

function EnumEntryList({
  entries,
}: {
  entries: ServiceDataEnumEntryMetadata[];
}) {
  if (entries.length === 0) {
    return null;
  }

  return (
    <div className="mt-2 space-y-2">
      {entries.map((entry) => (
        <div
          key={`${entry.short_name}:${entry.value}`}
          className="rounded-md border border-border/50 bg-muted/30 px-2 py-2"
        >
          <div className="flex flex-wrap items-center gap-2">
            <Badge variant="outline">{entry.short_name}</Badge>
            <span className="font-mono text-xs text-muted-foreground">
              0x{entry.value.toString(16).toUpperCase()}
            </span>
          </div>
          <p className="mt-1 text-xs text-muted-foreground">{entry.description}</p>
        </div>
      ))}
    </div>
  );
}

function fingerprintLabel(stableId: string) {
  if (stableId.startsWith("fp:")) {
    return stableId.slice(0, 15);
  }
  return stableId.length > 18 ? `${stableId.slice(0, 18)}...` : stableId;
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
  const [selectedStableId, setSelectedStableId] = useState<string | null>(null);
  const rssiWindows = useRef<Map<string, number[]>>(new Map());

  const scannableUnits = useMemo(() => units.filter(canScan), [units]);

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

  useEffect(() => {
    if (!selectedStableId) {
      return;
    }

    if (!unitBleDevices.some((device) => device.stable_id === selectedStableId)) {
      setSelectedStableId(null);
    }
  }, [selectedStableId, unitBleDevices]);

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

      return a.stable_id.localeCompare(b.stable_id);
    });
  }, [firstSeenAt, packetCounts, strongestRssi, unitBleDevices]);

  const focusedDevices = useMemo(() => {
    if (!selectedStableId) {
      return sortedDevices;
    }
    return sortedDevices.filter((device) => device.stable_id === selectedStableId);
  }, [selectedStableId, sortedDevices]);

  const focusedPackets = useMemo(() => {
    if (!selectedStableId) {
      return unitBlePackets;
    }
    return unitBlePackets.filter((packet) => packet.stable_id === selectedStableId);
  }, [selectedStableId, unitBlePackets]);

  const recentPackets = useMemo(
    () => focusedPackets.slice(-MAX_PACKETS_IN_VIEW).reverse(),
    [focusedPackets]
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
      timestampMs: number,
      serviceLabels: Record<string, string>
    ) => {
      if (payload.length === 0) {
        return;
      }

      let fields = grouped.get(stableId);
      if (!fields) {
        fields = new Map();
        grouped.set(stableId, fields);
      }

      for (const field of parseAdvertisementStructures(payload, serviceLabels)) {
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

    for (const packet of focusedPackets) {
      ingestFields(packet.stable_id, "adv", packet.raw_data, packet.timestamp_ms, packet.service_labels);
      ingestFields(packet.stable_id, "scan-response", packet.raw_scan_response, packet.timestamp_ms, packet.service_labels);
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
  }, [focusedPackets]);

  const focusedServiceLabels = useMemo(() => {
    const labels: Record<string, string> = {};
    for (const device of focusedDevices) {
      Object.assign(labels, device.service_labels);
    }
    for (const packet of focusedPackets) {
      Object.assign(labels, packet.service_labels);
    }
    return labels;
  }, [focusedDevices, focusedPackets]);

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

  const serviceSummary = useMemo(() => {
    const counts = new Map<string, number>();

    for (const device of focusedDevices) {
      const uniqueServices = new Set(device.services.map((service) => service.toLowerCase()));
      for (const service of uniqueServices) {
        counts.set(service, (counts.get(service) ?? 0) + 1);
      }
    }

    return [...counts.entries()]
      .map(([uuid, count]) => ({
        uuid,
        label: resolveServiceName(uuid, focusedServiceLabels),
        count,
        isStandard: isStandardUuid(uuid),
      }))
      .sort((a, b) => b.count - a.count || a.label.localeCompare(b.label));
  }, [focusedDevices, focusedServiceLabels]);

  const companySummary = useMemo(() => {
    const counts = new Map<number, number>();

    for (const packet of focusedPackets) {
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
  }, [focusedPackets]);

  const uniqueServicesCount = serviceSummary.length;
  const namedDevicesCount = focusedDevices.filter((device) => device.name !== "Unknown").length;
  const packetRate = useMemo(() => {
    const now = Date.now();
    const recent = focusedPackets.filter((packet) => now - packet.timestamp_ms <= 10_000);
    return recent.length / 10;
  }, [focusedPackets]);

  const timelineBuckets = useMemo(() => {
    const now = Date.now();
    const bucketWidth = TIMELINE_WINDOW_MS / TIMELINE_BUCKETS;
    const buckets = Array.from({ length: TIMELINE_BUCKETS }, (_, index) => ({
      label: `${Math.round((TIMELINE_BUCKETS - index) * (bucketWidth / 1000))}s`,
      count: 0,
      namedCount: 0,
    }));

    for (const packet of focusedPackets) {
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
  }, [focusedPackets]);

  const maxBucketCount = Math.max(...timelineBuckets.map((bucket) => bucket.count), 1);
  const focusedDevice = selectedStableId != null ? focusedDevices[0] ?? null : null;

  if (!selectedUnit) {
    return <EmptyState scanning={scanning} selectedUnit={selectedUnit} />;
  }

  return (
    <SidebarProvider
      defaultOpen
      className="h-full min-h-0"
      style={{ "--sidebar-width": "22rem" } as React.CSSProperties}
    >
      <Sidebar collapsible="none" className="border-r border-sidebar-border/70">
        <SidebarHeader className="gap-3 border-b border-sidebar-border/70 p-4">
          <div>
            <p className="text-sm font-semibold text-sidebar-foreground">Scan Devices</p>
            <p className="text-xs text-sidebar-foreground/70">
              Fingerprinted device index for the selected scan unit.
            </p>
          </div>
          <div className="space-y-2">
            <span className="text-[11px] font-medium uppercase tracking-[0.12em] text-sidebar-foreground/60">
              Scan Unit
            </span>
            <Select value={selectedUnitId ?? undefined} onValueChange={setSelectedUnitId}>
              <SelectTrigger className="w-full bg-background">
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
          <div className="flex flex-wrap gap-2 text-xs">
            <Badge variant="secondary">{unitBleDevices.length} devices</Badge>
            <Badge variant="secondary">{unitBlePackets.length} packets</Badge>
            <Badge variant={scanning ? "default" : "outline"}>
              {scanning ? "Scanning" : "Idle"}
            </Badge>
          </div>
        </SidebarHeader>

        <SidebarContent>
          <SidebarGroup className="gap-2">
            <SidebarGroupLabel>Detected Devices</SidebarGroupLabel>
            <SidebarGroupContent>
              <SidebarMenu>
                <SidebarMenuItem>
                  <SidebarMenuButton
                    isActive={selectedStableId == null}
                    onClick={() => setSelectedStableId(null)}
                    tooltip="Show all detected devices"
                  >
                    <Bluetooth />
                    <span>All Devices</span>
                  </SidebarMenuButton>
                  <SidebarMenuBadge>{unitBleDevices.length}</SidebarMenuBadge>
                </SidebarMenuItem>
                {sortedDevices.map((device) => (
                  <SidebarMenuItem key={device.stable_id}>
                    <SidebarMenuButton
                      isActive={selectedStableId === device.stable_id}
                      onClick={() => setSelectedStableId(device.stable_id)}
                      className="h-auto items-start py-2"
                      tooltip={device.stable_id}
                    >
                      <Fingerprint className="mt-0.5" />
                      <span className="flex min-w-0 flex-1 flex-col">
                        <span className="truncate text-sm">
                          {device.name !== "Unknown" ? device.name : fingerprintLabel(device.stable_id)}
                        </span>
                        <span className="truncate font-mono text-[11px] text-sidebar-foreground/60">
                          {device.stable_id}
                        </span>
                        <span className="truncate text-[11px] text-sidebar-foreground/60">
                          strongest {device.strongestRssi} dBm
                        </span>
                      </span>
                    </SidebarMenuButton>
                    <SidebarMenuBadge>{device.packetCount}</SidebarMenuBadge>
                  </SidebarMenuItem>
                ))}
              </SidebarMenu>
              {sortedDevices.length === 0 && (
                <p className="px-2 py-3 text-sm text-sidebar-foreground/60">
                  No devices retained for this unit yet.
                </p>
              )}
            </SidebarGroupContent>
          </SidebarGroup>
        </SidebarContent>

        <SidebarSeparator />
      </Sidebar>

      <div className="flex h-full min-h-0 flex-1 flex-col gap-4 px-4 py-4 md:px-6">
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
              {selectedStableId ? (
                <Badge variant="outline">
                  Focused: {fingerprintLabel(selectedStableId)}
                </Badge>
              ) : (
                <Badge variant="outline">Focused: all devices</Badge>
              )}
              <Badge variant="outline">{focusedDevices.length} devices</Badge>
              <Badge variant="outline">{focusedPackets.length} packets</Badge>
              <Badge variant="outline">{uniqueServicesCount} services</Badge>
              <Badge variant={scanning ? "default" : "secondary"}>
                {scanning ? `${packetRate.toFixed(1)} pkt/s` : "Idle"}
              </Badge>
            </div>
          </CardHeader>
        </Card>

        {focusedDevices.length === 0 && focusedPackets.length === 0 ? (
          <EmptyState scanning={scanning} selectedUnit={selectedUnit} />
        ) : focusedDevice ? (
          <DeviceFocusView
            device={focusedDevice}
            packets={recentPackets}
            selectedPacket={selectedPacket}
            onSelectPacket={setSelectedPacketId}
            observedFields={observedFieldsByStableId.get(focusedDevice.stable_id) ?? []}
          />
        ) : (
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
                      {selectedStableId
                        ? "Focused view for the selected fingerprinted device."
                        : "Current deduplicated view for the selected scanning unit."}
                    </CardDescription>
                  </CardHeader>
                  <CardContent className="min-h-0">
                    <ScrollArea className="h-[calc(100vh-22rem)] xl:h-[calc(100vh-19rem)]">
                      <div className="space-y-3 pr-4">
                        {focusedDevices.map((device) => (
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
                      <MetricCard label="Unnamed Devices" value={focusedDevices.length - namedDevicesCount} />
                      <MetricCard label="Unique Services" value={uniqueServicesCount} />
                      <MetricCard label="Recent Packets" value={focusedPackets.filter((packet) => Date.now() - packet.timestamp_ms <= 30_000).length} />
                    </CardContent>
                  </Card>

                  <Card className="min-h-0">
                    <CardHeader>
                      <CardTitle>Service Inventory</CardTitle>
                      <CardDescription>Services currently visible across the focused device set.</CardDescription>
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
                      <CardDescription>Manufacturer identifiers seen in the focused packet stream.</CardDescription>
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
                    <CardDescription>Arrival order for the newest packets in the current focus.</CardDescription>
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
                                  {resolveServiceName(uuid, packet.service_labels)}
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
                      Live packet log for the current unit and device focus. Select a row to inspect the payload.
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
                      <PacketInspector packet={selectedPacket} scrollClassName="h-[calc(100vh-19rem)]" />
                    ) : (
                      <p className="text-sm text-muted-foreground">Select a packet to inspect it.</p>
                    )}
                  </CardContent>
                </Card>
              </div>
            </TabsContent>
          </Tabs>
        )}
      </div>
    </SidebarProvider>
  );
}

function DeviceFocusView({
  device,
  packets,
  selectedPacket,
  onSelectPacket,
  observedFields,
}: {
  device: BleDevice & {
    packetCount?: number;
    strongestRssi?: number;
  };
  packets: BlePacket[];
  selectedPacket: BlePacket | null;
  onSelectPacket: (packetId: string) => void;
  observedFields: {
    key: string;
    source: "adv" | "scan-response";
    typeName: string;
    typeHex: string;
    summary: string;
    hex: string;
    count: number;
    lastSeenIso: string;
  }[];
}) {
  const companyId = device.manufacturer_data[0]?.company_id ?? null;
  const companyLabel = companyId != null ? resolveCompanyName(companyId) : null;
  const [selectedServiceUuid, setSelectedServiceUuid] = useState<string | null>(device.services[0] ?? null);
  const [selectedIdentityKey, setSelectedIdentityKey] = useState<string>("fingerprint");
  const [selectedFieldKey, setSelectedFieldKey] = useState<string | null>(observedFields[0]?.key ?? null);

  const identityEntries = useMemo(() => ([
    {
      key: "fingerprint",
      label: "Fingerprint",
      value: device.stable_id,
      detail: "Stable fingerprint key used to group rotating BLE identities into one retained device.",
    },
    {
      key: "observed-id",
      label: "Observed ID",
      value: device.id,
      detail: "Most recent retained device identifier shown in the observation stream.",
    },
    {
      key: "manufacturer",
      label: "Manufacturer",
      value: companyId != null
        ? `${companyLabel ?? "Unknown Company"} (0x${companyId.toString(16).padStart(4, "0").toUpperCase()})`
        : "No manufacturer data",
      detail: "Derived from manufacturer-specific AD structures retained for this device.",
    },
    {
      key: "last-seen",
      label: "Last Seen",
      value: relativeTime(device.last_seen),
      detail: "Relative timestamp of the most recent retained advertisement for this device.",
    },
  ]), [companyId, companyLabel, device.id, device.last_seen, device.stable_id]);

  const selectedServiceUuidValue = selectedServiceUuid && device.services.includes(selectedServiceUuid)
    ? selectedServiceUuid
    : device.services[0] ?? null;
  const selectedIdentity = identityEntries.find((entry) => entry.key === selectedIdentityKey) ?? identityEntries[0] ?? null;
  const selectedField = observedFields.find((field) => field.key === selectedFieldKey) ?? observedFields[0] ?? null;

  useEffect(() => {
    if (device.services.length === 0) {
      if (selectedServiceUuid !== null) {
        setSelectedServiceUuid(null);
      }
      return;
    }

    if (!selectedServiceUuid || !device.services.includes(selectedServiceUuid)) {
      setSelectedServiceUuid(device.services[0] ?? null);
    }
  }, [device.services, selectedServiceUuid]);

  useEffect(() => {
    if (observedFields.length === 0) {
      if (selectedFieldKey !== null) {
        setSelectedFieldKey(null);
      }
      return;
    }

    if (!selectedFieldKey || !observedFields.some((field) => field.key === selectedFieldKey)) {
      setSelectedFieldKey(observedFields[0]?.key ?? null);
    }
  }, [observedFields, selectedFieldKey]);

  return (
    <div className="grid min-h-0 flex-1 gap-4 xl:grid-cols-[minmax(0,1.1fr)_minmax(360px,0.9fr)]">
      <div className="grid min-h-0 gap-4">
        <Card>
          <CardHeader>
            <CardTitle>{device.name !== "Unknown" ? device.name : "Unnamed Device"}</CardTitle>
            <CardDescription>
              Detailed diagnostic view for fingerprint <span className="font-mono">{device.stable_id}</span>
            </CardDescription>
          </CardHeader>
          <CardContent className="grid gap-3 md:grid-cols-2 xl:grid-cols-4">
            <MetricCard label="Display ID" value={device.id} />
            <MetricCard label="Packets Seen" value={device.packetCount ?? packets.length} />
            <MetricCard label="Strongest RSSI" value={`${device.strongestRssi ?? device.rssi} dBm`} />
            <MetricCard label="Last Seen" value={relativeTime(device.last_seen)} />
          </CardContent>
        </Card>

        <Card className="min-h-0">
          <CardHeader>
            <CardTitle>Device Detail</CardTitle>
            <CardDescription>
              Select a service, identity record, or AD structure and inspect its retained detail below.
            </CardDescription>
          </CardHeader>
          <CardContent className="min-h-0">
            <Tabs defaultValue="services" className="min-h-0">
              <TabsList variant="line" className="w-full justify-start rounded-none border-b bg-transparent p-0">
                <TabsTrigger value="services" className="rounded-none px-4">Advertised Services</TabsTrigger>
                <TabsTrigger value="identity" className="rounded-none px-4">Identity</TabsTrigger>
                <TabsTrigger value="ad" className="rounded-none px-4">Observed AD Structures</TabsTrigger>
              </TabsList>

              <TabsContent value="services" className="min-h-0 pt-4">
                <ResizablePanelGroup orientation="vertical" className="min-h-[28rem] rounded-lg border border-border/60">
                  <ResizablePanel defaultSize={48} minSize={24}>
                    <ScrollArea className="h-full">
                      <div className="space-y-2 p-3">
                        {device.services.length === 0 ? (
                          <p className="text-sm text-muted-foreground">No services retained for this device.</p>
                        ) : (
                          device.services.map((uuid) => (
                            <button
                              key={uuid}
                              type="button"
                              onClick={() => setSelectedServiceUuid(uuid)}
                              className={`w-full rounded-md border px-3 py-2 text-left transition-colors ${
                                selectedServiceUuidValue === uuid
                                  ? "border-foreground/40 bg-accent"
                                  : "border-border/60 hover:border-foreground/20 hover:bg-accent/40"
                              }`}
                            >
                              <div className="flex items-start justify-between gap-3">
                                <div className="min-w-0">
                                  <p className="truncate text-sm font-medium">{resolveServiceName(uuid, device.service_labels)}</p>
                                  <p className="truncate font-mono text-xs text-muted-foreground">{uuid}</p>
                                </div>
                                <Badge variant={isStandardUuid(uuid) ? "secondary" : "outline"}>
                                  {isStandardUuid(uuid) ? "SIG" : "Custom"}
                                </Badge>
                              </div>
                            </button>
                          ))
                        )}
                      </div>
                    </ScrollArea>
                  </ResizablePanel>
                  <ResizableHandle withHandle />
                  <ResizablePanel defaultSize={52} minSize={24}>
                    <div className="h-full p-4">
                      {selectedServiceUuidValue ? (
                        <SelectedServiceInspector
                          packets={packets}
                          serviceLabels={device.service_labels}
                          serviceUuid={selectedServiceUuidValue}
                        />
                      ) : (
                        <p className="text-sm text-muted-foreground">Select a service to inspect it.</p>
                      )}
                    </div>
                  </ResizablePanel>
                </ResizablePanelGroup>
              </TabsContent>

              <TabsContent value="identity" className="min-h-0 pt-4">
                <ResizablePanelGroup orientation="vertical" className="min-h-[28rem] rounded-lg border border-border/60">
                  <ResizablePanel defaultSize={42} minSize={24}>
                    <ScrollArea className="h-full">
                      <div className="space-y-2 p-3">
                        {identityEntries.map((entry) => (
                          <button
                            key={entry.key}
                            type="button"
                            onClick={() => setSelectedIdentityKey(entry.key)}
                            className={`w-full rounded-md border px-3 py-2 text-left transition-colors ${
                              selectedIdentity?.key === entry.key
                                ? "border-foreground/40 bg-accent"
                                : "border-border/60 hover:border-foreground/20 hover:bg-accent/40"
                            }`}
                          >
                            <p className="text-sm font-medium">{entry.label}</p>
                            <p className="mt-1 truncate text-xs text-muted-foreground">{entry.value}</p>
                          </button>
                        ))}
                      </div>
                    </ScrollArea>
                  </ResizablePanel>
                  <ResizableHandle withHandle />
                  <ResizablePanel defaultSize={58} minSize={24}>
                    <div className="h-full p-4">
                      {selectedIdentity ? (
                        <div className="space-y-3">
                          <div className="rounded-md border border-border/60 px-3 py-3">
                            <p className="text-xs uppercase tracking-[0.12em] text-muted-foreground">{selectedIdentity.label}</p>
                            <p className="mt-1 break-all font-mono text-sm">{selectedIdentity.value}</p>
                          </div>
                          <div className="rounded-md border border-border/60 px-3 py-3">
                            <p className="text-xs uppercase tracking-[0.12em] text-muted-foreground">Meaning</p>
                            <p className="mt-1 text-sm text-muted-foreground">{selectedIdentity.detail}</p>
                          </div>
                        </div>
                      ) : (
                        <p className="text-sm text-muted-foreground">Select an identity field to inspect it.</p>
                      )}
                    </div>
                  </ResizablePanel>
                </ResizablePanelGroup>
              </TabsContent>

              <TabsContent value="ad" className="min-h-0 pt-4">
                <ResizablePanelGroup orientation="vertical" className="min-h-[28rem] rounded-lg border border-border/60">
                  <ResizablePanel defaultSize={48} minSize={24}>
                    <ScrollArea className="h-full">
                      <div className="space-y-2 p-3">
                        {observedFields.length === 0 ? (
                          <p className="text-sm text-muted-foreground">No retained AD structures for this device yet.</p>
                        ) : (
                          observedFields.map((field) => (
                            <button
                              key={field.key}
                              type="button"
                              onClick={() => setSelectedFieldKey(field.key)}
                              className={`w-full rounded-md border px-3 py-2 text-left transition-colors ${
                                selectedField?.key === field.key
                                  ? "border-foreground/40 bg-accent"
                                  : "border-border/60 hover:border-foreground/20 hover:bg-accent/40"
                              }`}
                            >
                              <div className="flex items-start justify-between gap-3">
                                <div className="min-w-0">
                                  <p className="truncate text-sm font-medium">
                                    {field.typeName}
                                    <span className="ml-2 font-mono text-xs text-muted-foreground">{field.typeHex}</span>
                                  </p>
                                  <p className="mt-1 truncate text-xs text-muted-foreground">{field.summary}</p>
                                </div>
                                <span className="shrink-0 text-xs text-muted-foreground">{field.count}x</span>
                              </div>
                            </button>
                          ))
                        )}
                      </div>
                    </ScrollArea>
                  </ResizablePanel>
                  <ResizableHandle withHandle />
                  <ResizablePanel defaultSize={52} minSize={24}>
                    <div className="h-full p-4">
                      {selectedField ? (
                        <div className="space-y-3">
                          <div className="rounded-md border border-border/60 px-3 py-3">
                            <p className="text-xs uppercase tracking-[0.12em] text-muted-foreground">AD Type</p>
                            <p className="mt-1 text-sm font-medium">
                              {selectedField.typeName} <span className="font-mono text-xs text-muted-foreground">{selectedField.typeHex}</span>
                            </p>
                          </div>
                          <div className="rounded-md border border-border/60 px-3 py-3">
                            <p className="text-xs uppercase tracking-[0.12em] text-muted-foreground">Decoded Summary</p>
                            <p className="mt-1 text-sm text-muted-foreground">{selectedField.summary}</p>
                          </div>
                          <div className="rounded-md border border-border/60 px-3 py-3">
                            <p className="text-xs uppercase tracking-[0.12em] text-muted-foreground">Field Payload</p>
                            <p className="mt-1 break-all font-mono text-xs text-muted-foreground">{selectedField.hex || "No field payload"}</p>
                          </div>
                          <div className="grid gap-3 md:grid-cols-3">
                            <div className="rounded-md border border-border/60 px-3 py-3">
                              <p className="text-xs uppercase tracking-[0.12em] text-muted-foreground">Source</p>
                              <p className="mt-1 text-sm">{selectedField.source}</p>
                            </div>
                            <div className="rounded-md border border-border/60 px-3 py-3">
                              <p className="text-xs uppercase tracking-[0.12em] text-muted-foreground">Seen</p>
                              <p className="mt-1 text-sm">{selectedField.count} times</p>
                            </div>
                            <div className="rounded-md border border-border/60 px-3 py-3">
                              <p className="text-xs uppercase tracking-[0.12em] text-muted-foreground">Last Seen</p>
                              <p className="mt-1 text-sm">{relativeTime(selectedField.lastSeenIso)}</p>
                            </div>
                          </div>
                        </div>
                      ) : (
                        <p className="text-sm text-muted-foreground">Select an AD structure to inspect it.</p>
                      )}
                    </div>
                  </ResizablePanel>
                </ResizablePanelGroup>
              </TabsContent>
            </Tabs>
          </CardContent>
        </Card>
      </div>

      <div className="grid min-h-0 gap-4">
        <Card className="min-h-0">
          <CardHeader>
            <CardTitle>Recent Packets</CardTitle>
            <CardDescription>Recent packet stream for this device only.</CardDescription>
          </CardHeader>
          <CardContent className="min-h-0">
            <ScrollArea className="h-[calc(100vh-31rem)] min-h-72">
              <div className="space-y-2 pr-4">
                {packets.map((packet) => (
                  <button
                    key={packet.id}
                    type="button"
                    onClick={() => onSelectPacket(packet.id)}
                    className={`w-full rounded-lg border px-3 py-3 text-left transition-colors ${
                      selectedPacket?.id === packet.id
                        ? "border-foreground/40 bg-accent"
                        : "border-border/60 hover:border-foreground/20 hover:bg-accent/40"
                    }`}
                  >
                    <div className="flex items-start justify-between gap-3">
                      <div className="min-w-0">
                        <p className="truncate text-sm font-medium">
                          {packet.name !== "Unknown" ? packet.name : packet.device_id}
                        </p>
                        <div className="mt-1 flex flex-wrap gap-2 text-xs text-muted-foreground">
                          <span>{formatPacketTimestamp(packet.timestamp_ms)}</span>
                          <span>{packet.address_type || "addr type unknown"}</span>
                          <span>adv type {packet.adv_type}</span>
                        </div>
                      </div>
                      <div className="shrink-0 text-right">
                        <p className="text-sm font-medium">{packet.rssi} dBm</p>
                        <p className="text-xs text-muted-foreground">{packet.raw_data.length} B</p>
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
            <CardTitle>Selected Packet</CardTitle>
            <CardDescription>Raw payload and decoded AD fields for the highlighted packet.</CardDescription>
          </CardHeader>
          <CardContent className="min-h-0">
            {selectedPacket ? (
              <PacketInspector packet={selectedPacket} scrollClassName="h-[calc(100vh-31rem)] min-h-72" />
            ) : (
              <p className="text-sm text-muted-foreground">Select a packet to inspect it.</p>
            )}
          </CardContent>
        </Card>
      </div>
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

function SelectedServiceInspector({
  serviceUuid,
  serviceLabels,
  packets,
}: {
  serviceUuid: string;
  serviceLabels: Record<string, string>;
  packets: BlePacket[];
}) {
  const [snapshotPayload, setSnapshotPayload] = useState<ResolvedServicePayloadObservation | null>(null);
  const [decodeState, setDecodeState] = useState<"idle" | "loading" | "ready" | "error">("idle");
  const [decodeError, setDecodeError] = useState<string | null>(null);

  const normalizedServiceUuid = serviceUuid.toLowerCase();
  const resolvedServiceName = resolveServiceName(serviceUuid, serviceLabels);
  const serviceUuid16 = useMemo(() => parseBluetoothSigUuid16(serviceUuid), [serviceUuid]);
  const { metadataByUuid, metadataState, metadataError } = useServiceDataFormatMetadata([serviceUuid]);
  const serviceMetadata = metadataByUuid[normalizeMetadataUuid(serviceUuid)];
  const serviceSnapshot = useMemo(() => {
    if (serviceUuid16 == null) {
      return null;
    }

    const variants = new Map<string, {
      signature: string;
      packet: BlePacket;
      payloadBytes: number[];
      count: number;
      lastSeenMs: number;
    }>();

    for (const packet of packets) {
      const payloadBytes = findServicePayloadSnapshot(packet, serviceUuid16);
      if (!payloadBytes) {
        continue;
      }

      const signature = servicePayloadSignature(packet, serviceUuid16);
      if (!signature) {
        continue;
      }

      const existing = variants.get(signature);
      if (existing) {
        existing.count += 1;
        if (packet.timestamp_ms > existing.lastSeenMs) {
          existing.lastSeenMs = packet.timestamp_ms;
          existing.packet = packet;
        }
        continue;
      }

      variants.set(signature, {
        signature,
        packet,
        payloadBytes,
        count: 1,
        lastSeenMs: packet.timestamp_ms,
      });
    }

    const ordered = [...variants.values()].sort((a, b) => b.lastSeenMs - a.lastSeenMs);
    if (ordered.length === 0) {
      return null;
    }

    const current = ordered[0];
    if (!current) {
      return null;
    }

    return {
      current,
      variantCount: ordered.length,
      totalCount: ordered.reduce((sum, variant) => sum + variant.count, 0),
    };
  }, [packets, serviceUuid16]);
  const snapshotCurrent = serviceSnapshot?.current ?? null;

  useEffect(() => {
    let cancelled = false;

    if (!serviceSnapshot || !snapshotCurrent) {
      setSnapshotPayload(null);
      setDecodeState("ready");
      setDecodeError(null);
      return;
    }

    setDecodeState("loading");
    setDecodeError(null);

    decodeDaemonAdvertisement(
      snapshotCurrent.packet.raw_data,
      snapshotCurrent.packet.raw_scan_response
    )
      .then((decoded) => {
        if (cancelled) {
          return;
        }

        const service = decoded.find((entry) => entry.service_uuid.toLowerCase() === normalizedServiceUuid);
        if (!service) {
          setSnapshotPayload(null);
          setDecodeState("ready");
          return;
        }

        setSnapshotPayload({
          key: `${service.status_code}:${service.raw_value}`,
          serviceUuid: service.service_uuid,
          serviceLabel: service.service_label,
          rawValue: service.raw_value,
          statusCode: service.status_code,
          statusMessage: service.status_message,
          fields: service.fields,
          count: snapshotCurrent.count,
          lastSeenMs: snapshotCurrent.lastSeenMs,
          variantCount: serviceSnapshot.variantCount,
        });
        setDecodeState("ready");
      })
      .catch((error: unknown) => {
        if (cancelled) {
          return;
        }

        setSnapshotPayload(null);
        setDecodeState("error");
        setDecodeError(error instanceof Error ? error.message : String(error));
      });

    return () => {
      cancelled = true;
    };
  }, [normalizedServiceUuid, serviceSnapshot?.variantCount, snapshotCurrent?.signature]);

  useEffect(() => {
    if (!serviceSnapshot || !snapshotCurrent) {
      return;
    }

    setSnapshotPayload((current) => {
      if (!current || current.rawValue.toLowerCase().replace(/^0x/, "") !== snapshotCurrent.signature) {
        return current;
      }

      return {
        ...current,
        count: snapshotCurrent.count,
        lastSeenMs: snapshotCurrent.lastSeenMs,
        variantCount: serviceSnapshot.variantCount,
      };
    });
  }, [serviceSnapshot?.variantCount, snapshotCurrent?.signature, snapshotCurrent?.count, snapshotCurrent?.lastSeenMs]);

  const snapshotBytes = useMemo(
    () => snapshotPayload ? parseHexStringBytes(snapshotPayload.rawValue) : snapshotCurrent?.payloadBytes ?? [],
    [snapshotPayload, snapshotCurrent]
  );

  return (
    <div className="space-y-3">
      <div className="rounded-md border border-border/60 px-3 py-3">
        <p className="text-xs uppercase tracking-[0.12em] text-muted-foreground">Service Name</p>
        <p className="mt-1 text-sm font-medium">{resolvedServiceName}</p>
      </div>
      <div className="rounded-md border border-border/60 px-3 py-3">
        <p className="text-xs uppercase tracking-[0.12em] text-muted-foreground">UUID</p>
        <p className="mt-1 break-all font-mono text-sm">{serviceUuid}</p>
      </div>
      <div className="rounded-md border border-border/60 px-3 py-3">
        <p className="text-xs uppercase tracking-[0.12em] text-muted-foreground">Classification</p>
        <p className="mt-1 text-sm">{isStandardUuid(serviceUuid) ? "Bluetooth SIG base UUID" : "Custom / vendor UUID"}</p>
      </div>
      {metadataState === "error" ? (
        <div className="rounded-md border border-destructive/40 px-3 py-3">
          <p className="text-xs uppercase tracking-[0.12em] text-destructive">Format Metadata</p>
          <p className="mt-1 text-sm text-destructive">
            Failed to load service format metadata: {metadataError ?? "unknown error"}
          </p>
        </div>
      ) : serviceMetadata?.service_description ? (
        <div className="rounded-md border border-border/60 px-3 py-3">
          <p className="text-xs uppercase tracking-[0.12em] text-muted-foreground">Description</p>
          <p className="mt-1 text-sm text-muted-foreground">{serviceMetadata.service_description}</p>
        </div>
      ) : metadataState === "loading" ? (
        <div className="rounded-md border border-border/60 px-3 py-3">
          <p className="text-xs uppercase tracking-[0.12em] text-muted-foreground">Description</p>
          <p className="mt-1 text-sm text-muted-foreground">Loading service format metadata...</p>
        </div>
      ) : null}
      <div className="rounded-md border border-border/60 px-3 py-3">
        <div className="flex items-center justify-between gap-3">
          <p className="text-xs uppercase tracking-[0.12em] text-muted-foreground">Snapshot Payload</p>
          {snapshotPayload && (
            <Badge variant="secondary">
              {snapshotPayload.variantCount} variant{snapshotPayload.variantCount === 1 ? "" : "s"}
            </Badge>
          )}
        </div>

        {serviceUuid16 == null ? (
          <p className="mt-2 text-sm text-muted-foreground">
            No daemon-backed payload resolver is available for this UUID form yet.
          </p>
        ) : decodeState === "error" ? (
          <p className="mt-2 text-sm text-destructive">
            Failed to resolve service payloads: {decodeError ?? "unknown error"}
          </p>
        ) : !serviceSnapshot ? (
          <div className="mt-2 space-y-2">
            <p className="text-sm text-muted-foreground">
              No decoded service-data payloads were observed for this service in the current packet window.
            </p>
            <p className="text-xs text-muted-foreground">
              The service may be advertised only as a UUID list entry, or the daemon may not have a decoder for its payload format yet.
            </p>
          </div>
        ) : (
          <div className="mt-3 space-y-3">
            <div className="grid gap-2 md:grid-cols-4">
              <MetaPill label="Last Seen" value={formatPacketTimestamp(snapshotCurrent!.lastSeenMs)} />
              <MetaPill label="Packet Copies" value={snapshotCurrent!.count} />
              <MetaPill label="Variants" value={serviceSnapshot.variantCount} />
              <MetaPill
                label="Status"
                value={snapshotPayload ? `${snapshotPayload.statusCode} ${snapshotPayload.statusMessage}` : (decodeState === "loading" ? "decoding..." : "pending")}
              />
            </div>

            <div className="grid gap-3 xl:grid-cols-[minmax(0,1.2fr)_minmax(18rem,0.8fr)]">
              <div className="rounded-md border border-border/60 px-3 py-3">
                <div className="mb-3 flex items-center justify-between gap-3">
                  <div>
                    <p className="text-sm font-medium">{snapshotPayload?.serviceLabel ?? resolvedServiceName}</p>
                    <p className="font-mono text-xs text-muted-foreground">{serviceUuid}</p>
                  </div>
                  {snapshotPayload && (
                    <Badge variant={snapshotPayload.statusCode === 200 ? "secondary" : "outline"}>
                      {snapshotPayload.statusCode} {snapshotPayload.statusMessage}
                    </Badge>
                  )}
                </div>
                <HexAsciiTable bytes={snapshotBytes} />
              </div>

              <div className="rounded-md border border-border/60 px-3 py-3">
                <div className="mb-3 flex items-center justify-between gap-3">
                  <p className="text-sm font-medium">Decoded Fields</p>
                  <Badge variant="secondary">{snapshotPayload?.fields.length ?? 0}</Badge>
                </div>
                {decodeState === "loading" && !snapshotPayload ? (
                  <p className="text-sm text-muted-foreground">Decoding snapshot payload...</p>
                ) : !snapshotPayload ? (
                  <p className="text-sm text-muted-foreground">No decoded snapshot is available yet.</p>
                ) : (
                  <div className="space-y-2">
                    {snapshotPayload.fields.map((field, index) => (
                      <div key={`${snapshotPayload.key}:${field.field}:${index}`} className="rounded-md border border-border/60 px-3 py-2">
                        <div className="flex items-center justify-between gap-3">
                          <p className="text-sm font-medium">{field.field}</p>
                          <Badge variant="secondary">{field.type}</Badge>
                        </div>
                        <p className="mt-1 break-all font-mono text-xs text-muted-foreground">{field.value}</p>
                        <EnumEntryList
                          entries={resolveEnumEntries(
                            field,
                            serviceMetadata?.fields.find((candidate) => candidate.field === field.field)
                          )}
                        />
                      </div>
                    ))}
                  </div>
                )}
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}

function PacketInspector({
  packet,
  scrollClassName,
}: {
  packet: BlePacket;
  scrollClassName: string;
}) {
  const [decodedServiceData, setDecodedServiceData] = useState<DecodedServiceData[]>([]);
  const [decodeState, setDecodeState] = useState<"idle" | "loading" | "ready" | "error">("idle");
  const [decodeError, setDecodeError] = useState<string | null>(null);
  const metadataUuids = useMemo(
    () => decodedServiceData.map((service) => service.service_uuid),
    [decodedServiceData]
  );
  const { metadataByUuid, metadataState, metadataError } = useServiceDataFormatMetadata(metadataUuids);

  useEffect(() => {
    let cancelled = false;

    setDecodeState("loading");
    setDecodeError(null);

    decodeDaemonAdvertisement(packet.raw_data, packet.raw_scan_response)
      .then((decoded) => {
        if (cancelled) {
          return;
        }
        setDecodedServiceData(decoded);
        setDecodeState("ready");
      })
      .catch((error: unknown) => {
        if (cancelled) {
          return;
        }
        setDecodedServiceData([]);
        setDecodeState("error");
        setDecodeError(error instanceof Error ? error.message : String(error));
      });

    return () => {
      cancelled = true;
    };
  }, [packet.id, packet.raw_data, packet.raw_scan_response]);

  return (
    <ScrollArea className={scrollClassName}>
      <div className="space-y-4 pr-4">
        <div className="rounded-lg border border-border/60 bg-muted/20 p-3">
          <div className="flex flex-wrap items-center justify-between gap-3">
            <div>
              <p className="text-sm font-medium">
                {packet.name !== "Unknown" ? packet.name : packet.device_id}
              </p>
              <p className="font-mono text-xs text-muted-foreground">{packet.device_id}</p>
            </div>
            <div className="text-right">
              <p className="text-sm font-medium">{packet.rssi} dBm</p>
              <p className="text-xs text-muted-foreground">{formatPacketTimestamp(packet.timestamp_ms)}</p>
            </div>
          </div>
          <div className="mt-3 grid grid-cols-2 gap-2 text-xs text-muted-foreground">
            <MetaPill label="Addr Type" value={packet.address_type || "unknown"} />
            <MetaPill label="SID" value={packet.sid} />
            <MetaPill label="Adv Type" value={packet.adv_type} />
            <MetaPill label="Adv Props" value={`0x${packet.adv_props.toString(16).toUpperCase()}`} />
            <MetaPill label="Interval" value={packet.interval} />
            <MetaPill label="Primary PHY" value={packet.primary_phy} />
            <MetaPill label="Secondary PHY" value={packet.secondary_phy} />
            <MetaPill label="Tx Power" value={packet.tx_power ?? "unknown"} />
          </div>
        </div>

        <PacketPayloadSection
          title="Advertising Payload"
          description={`${packet.raw_data.length} bytes`}
          bytes={packet.raw_data}
        />

        {packet.raw_scan_response.length > 0 && (
          <PacketPayloadSection
            title="Scan Response"
            description={`${packet.raw_scan_response.length} bytes`}
            bytes={packet.raw_scan_response}
          />
        )}

        <AdvertisementStructuresSection
          title="Advertising AD Structures"
          description="Resolved AD structures from the advertising payload."
          bytes={packet.raw_data}
          serviceLabels={packet.service_labels}
          emptyMessage="No raw advertising payload available for this packet."
        />

        {packet.raw_scan_response.length > 0 && (
          <AdvertisementStructuresSection
            title="Scan Response AD Structures"
            description="Resolved AD structures from the scan response payload."
            bytes={packet.raw_scan_response}
            serviceLabels={packet.service_labels}
            emptyMessage="No scan response payload available for this packet."
          />
        )}

        <DecodedServiceDataSection
          decodeError={decodeError}
          decodeState={decodeState}
          decodedServiceData={decodedServiceData}
          metadataByUuid={metadataByUuid}
          metadataState={metadataState}
          metadataError={metadataError}
        />
      </div>
    </ScrollArea>
  );
}

function AdvertisementStructuresSection({
  title,
  description,
  bytes,
  serviceLabels,
  emptyMessage,
}: {
  title: string;
  description: string;
  bytes: number[];
  serviceLabels: Record<string, string>;
  emptyMessage: string;
}) {
  const fields = useMemo(
    () => parseAdvertisementStructures(bytes, serviceLabels),
    [bytes, serviceLabels]
  );

  return (
    <Card className="border-border/60">
      <CardHeader>
        <CardTitle>{title}</CardTitle>
        <CardDescription>{description}</CardDescription>
      </CardHeader>
      <CardContent className="space-y-2">
        {fields.map((field) => (
          <div key={`${field.offset}-${field.type}-${field.hex}`} className="rounded-md border border-border/60 px-3 py-2">
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
        {fields.length === 0 && (
          <p className="text-sm text-muted-foreground">{emptyMessage}</p>
        )}
      </CardContent>
    </Card>
  );
}

function DecodedServiceDataSection({
  decodedServiceData,
  decodeState,
  decodeError,
  metadataByUuid,
  metadataState,
  metadataError,
}: {
  decodedServiceData: DecodedServiceData[];
  decodeState: "idle" | "loading" | "ready" | "error";
  decodeError: string | null;
  metadataByUuid: Record<string, ServiceDataFormatMetadata>;
  metadataState: "idle" | "loading" | "ready" | "error";
  metadataError: string | null;
}) {
  return (
    <Card className="border-border/60">
      <CardHeader>
        <CardTitle>Resolved Service Data</CardTitle>
        <CardDescription>
          Backend decode for known service-data payloads such as Broadcast Audio Announcement and Public Broadcast Announcement.
        </CardDescription>
      </CardHeader>
      <CardContent className="space-y-3">
        {decodeState === "loading" && (
          <p className="text-sm text-muted-foreground">Resolving service-data payloads via the daemon...</p>
        )}
        {decodeState === "error" && (
          <p className="text-sm text-destructive">
            Failed to resolve service-data payloads: {decodeError ?? "unknown error"}
          </p>
        )}
        {decodeState === "ready" && decodedServiceData.length === 0 && (
          <p className="text-sm text-muted-foreground">No known service-data payloads were decoded from this packet.</p>
        )}
        {metadataState === "error" && (
          <p className="text-sm text-destructive">
            Failed to load service format metadata: {metadataError ?? "unknown error"}
          </p>
        )}
        {decodedServiceData.map((service) => (
          <div key={`${service.service_uuid}:${service.raw_value}`} className="rounded-md border border-border/60 px-3 py-3">
            {(() => {
              const serviceMetadata = metadataByUuid[normalizeMetadataUuid(service.service_uuid)];
              const serviceTitle = serviceMetadata?.service_label || service.service_label;
              return (
                <>
            <div className="flex flex-wrap items-start justify-between gap-3">
              <div className="min-w-0">
                <p className="text-sm font-medium">{serviceTitle}</p>
                <p className="break-all font-mono text-xs text-muted-foreground">{service.service_uuid}</p>
                {serviceMetadata?.service_description && (
                  <p className="mt-1 text-sm text-muted-foreground">{serviceMetadata.service_description}</p>
                )}
              </div>
              <Badge variant={service.status_code === 200 ? "secondary" : "outline"}>
                {service.status_code} {service.status_message}
              </Badge>
            </div>

            <div className="mt-3 grid gap-3 md:grid-cols-2">
              <div className="rounded-md border border-border/60 px-3 py-3">
                <p className="text-xs uppercase tracking-[0.12em] text-muted-foreground">Raw Value</p>
                <p className="mt-1 break-all font-mono text-xs text-muted-foreground">{service.raw_value}</p>
              </div>
              <div className="rounded-md border border-border/60 px-3 py-3">
                <p className="text-xs uppercase tracking-[0.12em] text-muted-foreground">Decoded Fields</p>
                <p className="mt-1 text-sm">{service.fields.length}</p>
              </div>
            </div>

            <div className="mt-3 space-y-2">
              {service.fields.map((field, index) => (
                <div key={`${service.service_uuid}:${field.field}:${index}`} className="rounded-md border border-border/60 px-3 py-2">
                  <div className="flex items-center justify-between gap-3">
                    <p className="text-sm font-medium">{field.field}</p>
                    <Badge variant="secondary">{field.type}</Badge>
                  </div>
                  <p className="mt-1 break-all text-sm text-muted-foreground">{field.value}</p>
                  <EnumEntryList
                    entries={resolveEnumEntries(
                      field,
                      serviceMetadata?.fields.find((candidate) => candidate.field === field.field)
                    )}
                  />
                </div>
              ))}
            </div>
                </>
              );
            })()}
          </div>
        ))}
      </CardContent>
    </Card>
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

function parseBluetoothSigUuid16(uuid: string): number | null {
  const match = uuid.toLowerCase().match(/^0000([0-9a-f]{4})-0000-1000-8000-00805f9b34fb$/);
  const value = match?.[1];
  if (!value) {
    return null;
  }
  return Number.parseInt(value, 16);
}

function decodeAscii(byte: number) {
  return byte >= 0x20 && byte <= 0x7e ? String.fromCharCode(byte) : ".";
}

function parseHexStringBytes(hex: string) {
  const normalized = hex.startsWith("0x") || hex.startsWith("0X") ? hex.slice(2) : hex;
  if (normalized.length === 0 || normalized.length % 2 !== 0) {
    return [];
  }

  const bytes: number[] = [];
  for (let i = 0; i < normalized.length; i += 2) {
    const value = Number.parseInt(normalized.slice(i, i + 2), 16);
    if (Number.isNaN(value)) {
      return [];
    }
    bytes.push(value);
  }
  return bytes;
}

function findServiceDataPayload(bytes: number[], serviceUuid16: number) {
  let offset = 0;

  while (offset < bytes.length) {
    const fieldLength = bytes[offset] ?? 0;
    if (fieldLength === 0) {
      break;
    }

    if (offset + 1 + fieldLength > bytes.length) {
      break;
    }

    const type = bytes[offset + 1] ?? 0;
    const field = bytes.slice(offset + 2, offset + 1 + fieldLength);
    if (type === 0x16 && field.length >= 2) {
      const uuid = (field[0] ?? 0) | ((field[1] ?? 0) << 8);
      if (uuid === serviceUuid16) {
        return field.slice(2);
      }
    }

    offset += fieldLength + 1;
  }

  return null;
}

function findServicePayloadSnapshot(packet: BlePacket, serviceUuid16: number) {
  const advertisingPayload = findServiceDataPayload(packet.raw_data, serviceUuid16);
  if (advertisingPayload) {
    return advertisingPayload;
  }

  return findServiceDataPayload(packet.raw_scan_response, serviceUuid16);
}

function servicePayloadSignature(packet: BlePacket, serviceUuid16: number) {
  const payload = findServicePayloadSnapshot(packet, serviceUuid16);
  if (!payload) {
    return null;
  }

  return payload.map((byte) => byte.toString(16).padStart(2, "0")).join("");
}

function HexAsciiTable({
  bytes,
}: {
  bytes: number[];
}) {
  const rows = useMemo(() => {
    const nextRows: Array<{
      offset: string;
      hexColumns: string[];
      ascii: string;
    }> = [];

    for (let offset = 0; offset < bytes.length; offset += 16) {
      const slice = bytes.slice(offset, offset + 16);
      const hexColumns = Array.from({ length: 16 }, (_, index) => {
        const value = slice[index];
        return value == null ? "  " : value.toString(16).padStart(2, "0").toUpperCase();
      });
      const ascii = slice.map(decodeAscii).join("");
      nextRows.push({
        offset: offset.toString(16).padStart(4, "0").toUpperCase(),
        hexColumns,
        ascii,
      });
    }

    return nextRows;
  }, [bytes]);

  if (bytes.length === 0) {
    return (
      <div className="rounded-md border border-border/60 bg-muted/20 px-3 py-3">
        <p className="text-sm text-muted-foreground">No payload bytes.</p>
      </div>
    );
  }

  return (
    <div className="overflow-x-auto rounded-md border border-border/60 bg-muted/20 px-3 py-2">
      <div className="min-w-[42rem] space-y-1 font-mono text-xs">
        <div className="grid grid-cols-[4rem_1fr_8rem] gap-3 border-b border-border/60 pb-2 text-[10px] uppercase tracking-[0.12em] text-muted-foreground">
          <span>Offset</span>
          <span>Hex</span>
          <span>ASCII</span>
        </div>
        {rows.map((row) => (
          <div key={row.offset} className="grid grid-cols-[4rem_1fr_8rem] gap-3">
            <span className="text-muted-foreground">{row.offset}</span>
            <div className="grid grid-cols-16 gap-1">
              {row.hexColumns.map((column, index) => (
                <span key={`${row.offset}:${index}`}>{column}</span>
              ))}
            </div>
            <span className="tracking-[0.08em] text-muted-foreground">{row.ascii}</span>
          </div>
        ))}
      </div>
    </div>
  );
}
