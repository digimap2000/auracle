import { Badge } from "@/components/ui/badge";
import { Separator } from "@/components/ui/separator";
import { SignalBars } from "@/components/devices/SignalBars";
import { cn } from "@/lib/utils";
import type { BleDevice } from "@/lib/tauri";
import {
  resolveServiceName,
  resolveCompanyName,
  rssiColor,
  isStandardUuid,
  relativeTime,
} from "@/lib/ble-utils";

interface DeviceCardProps {
  device: BleDevice;
  expanded: boolean;
  onToggle: () => void;
  observedFields?: {
    key: string;
    source: "adv" | "scan-response";
    typeName: string;
    typeHex: string;
    summary: string;
    hex: string;
    count: number;
    lastSeenIso: string;
  }[];
}

const SERVICE_BADGE_PRIORITY: Record<string, { label: string; variant: "default" | "outline" }> = {
  "Focal Naim Auracast": { label: "Focal Naim Auracast", variant: "outline" },
  "Audio Stream Control": { label: "Audio Stream", variant: "outline" },
  "Broadcast Audio Scan": { label: "BA Scan", variant: "outline" },
  "Published Audio Capabilities": { label: "PAC", variant: "outline" },
  "Basic Audio Profile": { label: "BAP", variant: "outline" },
};

const PRIORITY_ORDER = Object.keys(SERVICE_BADGE_PRIORITY);

export function DeviceCard({
  device,
  expanded,
  onToggle,
  observedFields = [],
}: DeviceCardProps) {
  // Resolve services for badge display — include unknown services too
  const resolvedServices = device.services.map((uuid) => {
    const name = resolveServiceName(uuid, device.service_labels);
    return { uuid, name, isKnown: name !== uuid };
  });

  // Count occurrences of each service name for badges
  const badgeCounts = new Map<string, number>();
  for (const s of resolvedServices) {
    const name = s.isKnown ? s.name : "Unknown Service";
    badgeCounts.set(name, (badgeCounts.get(name) ?? 0) + 1);
  }

  const sortedBadges = [...badgeCounts.keys()].sort((a, b) => {
    const aIdx = PRIORITY_ORDER.indexOf(a);
    const bIdx = PRIORITY_ORDER.indexOf(b);
    if (aIdx !== -1 && bIdx !== -1) return aIdx - bIdx;
    if (aIdx !== -1) return -1;
    if (bIdx !== -1) return 1;
    // "Unknown Service" always sorts last
    if (a === "Unknown Service") return 1;
    if (b === "Unknown Service") return -1;
    return a.localeCompare(b);
  });

  const visibleBadges = sortedBadges.slice(0, 2);
  const overflowCount = Math.max(0, sortedBadges.length - 2);

  // Manufacturer info
  const companyId = device.manufacturer_data[0]?.company_id;
  const companyName = companyId != null ? resolveCompanyName(companyId) : null;
  const companyHex = companyId != null
    ? `0x${companyId.toString(16).padStart(4, "0").toUpperCase()}`
    : null;

  return (
    <div
      className="cursor-pointer rounded-lg border bg-card p-3 transition-colors hover:border-muted-foreground/30"
      onClick={onToggle}
    >
      {/* Line 1: signal + name + right side */}
      <div className="flex items-center justify-between gap-2">
        <div className="flex min-w-0 items-center gap-2">
          <SignalBars rssi={device.rssi} />
          <span
            className={cn(
              "truncate text-sm font-medium",
              device.name === "Unknown" && "italic text-muted-foreground"
            )}
          >
            {device.name}
          </span>
        </div>

        {/* Right side — collapsed: service badges */}
        {!expanded && (
          <div className="flex shrink-0 items-center gap-1.5">
            {visibleBadges.map((name) => {
              const config = SERVICE_BADGE_PRIORITY[name];
              const count = badgeCounts.get(name) ?? 1;
              return (
                <Badge
                  key={name}
                  variant={config?.variant ?? "outline"}
                  className="text-xs"
                >
                  {count > 1 && <span className="mr-0.5 opacity-60">{count}x</span>}
                  {config?.label ?? name}
                </Badge>
              );
            })}
            {overflowCount > 0 && (
              <Badge variant="secondary" className="text-xs">
                +{overflowCount} more
              </Badge>
            )}
          </div>
        )}

        {/* Right side — expanded: RSSI only */}
        {expanded && (
          <span className={cn("shrink-0 font-mono text-xs font-medium", rssiColor(device.rssi))}>
            {device.rssi} dBm
          </span>
        )}
      </div>

      {/* Line 2: expanded = manufacturer left + device ID right; collapsed = manufacturer only */}
      {expanded ? (
        <div className="mt-1 flex items-center justify-between">
          <span className="text-xs text-muted-foreground">
            {companyHex
              ? `${companyName ?? "Unknown"} - ${companyHex}`
              : "No manufacturer data"}
          </span>
          <span className="font-mono text-xs text-muted-foreground">
            {device.id}
          </span>
        </div>
      ) : (
        <p className="mt-1 text-xs text-muted-foreground">
          {companyName ?? (
            companyHex
              ? <span className="italic">Unknown ({companyHex})</span>
              : <span className="italic">No manufacturer data</span>
          )}
        </p>
      )}

      {/* Expandable detail section */}
      <div
        className={cn(
          "grid transition-all duration-200 ease-in-out",
          expanded ? "grid-rows-[1fr] opacity-100" : "grid-rows-[0fr] opacity-0"
        )}
      >
        <div className="overflow-hidden">
          <Separator className="my-3" />

          {/* Services list */}
          {device.services.length > 0 ? (
            <div className="text-xs">
              <div className="flex border-b pb-1.5 text-muted-foreground">
                <span className="w-1/3 shrink-0 font-medium">Service</span>
                <span className="min-w-0 flex-1 font-medium">UUID</span>
                <span className="shrink-0 text-right font-medium">Type</span>
              </div>
              {device.services.map((uuid, i) => {
                const name = resolveServiceName(uuid, device.service_labels);
                const isKnown = name !== uuid;
                const standard = isStandardUuid(uuid);
                return (
                  <div
                    key={i}
                    className={cn(
                      "flex py-1.5",
                      i < device.services.length - 1 && "border-b border-border/50"
                    )}
                  >
                    <span className="w-1/3 shrink-0 truncate">
                      {isKnown ? name : (
                        <span className="text-muted-foreground">Unknown Service</span>
                      )}
                    </span>
                    <span className="min-w-0 flex-1 truncate font-mono text-muted-foreground">
                      {uuid}
                    </span>
                    <span className="shrink-0 text-right text-muted-foreground">
                      {standard ? "Standard" : "Custom"}
                    </span>
                  </div>
                );
              })}
            </div>
          ) : (
            <p className="text-xs text-muted-foreground">No services advertised</p>
          )}

          <Separator className="my-3" />

          {observedFields.length > 0 ? (
            <div className="text-xs">
              <div className="mb-2 flex items-center justify-between gap-2">
                <span className="font-medium text-foreground">Observed AD Structures</span>
                <span className="text-muted-foreground">
                  {observedFields.length} distinct fields
                </span>
              </div>
              <div className="space-y-2">
                {observedFields.map((field) => (
                  <div
                    key={field.key}
                    className="rounded-md border border-border/60 px-3 py-2"
                  >
                    <div className="flex items-start justify-between gap-3">
                      <div className="min-w-0">
                        <p className="truncate text-sm font-medium">
                          {field.typeName}
                          <span className="ml-2 font-mono text-xs text-muted-foreground">
                            {field.typeHex}
                          </span>
                        </p>
                        <p className="mt-1 text-muted-foreground">{field.summary}</p>
                      </div>
                      <div className="shrink-0 text-right text-muted-foreground">
                        <p>{field.count} seen</p>
                        <p>{field.source}</p>
                      </div>
                    </div>
                    <p className="mt-2 break-all font-mono text-[11px] text-muted-foreground">
                      {field.hex || "No field payload"}
                    </p>
                    <p className="mt-1 text-[11px] text-muted-foreground">
                      last seen {relativeTime(field.lastSeenIso)}
                    </p>
                  </div>
                ))}
              </div>
            </div>
          ) : (
            <p className="text-xs text-muted-foreground">
              No decodable AD structures retained for this device yet.
            </p>
          )}
        </div>
      </div>
    </div>
  );
}
