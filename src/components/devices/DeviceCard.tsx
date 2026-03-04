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
} from "@/lib/ble-utils";

interface DeviceCardProps {
  device: BleDevice;
  expanded: boolean;
  onToggle: () => void;
}

const SERVICE_BADGE_PRIORITY: Record<string, { label: string; variant: "default" | "outline" }> = {
  "Focal Naim Auracast": { label: "Focal Naim Auracast", variant: "outline" },
  "Audio Stream Control": { label: "Audio Stream", variant: "outline" },
  "Broadcast Audio Scan": { label: "BA Scan", variant: "outline" },
  "Published Audio Capabilities": { label: "PAC", variant: "outline" },
  "Basic Audio Profile": { label: "BAP", variant: "outline" },
};

const PRIORITY_ORDER = Object.keys(SERVICE_BADGE_PRIORITY);

export function DeviceCard({ device, expanded, onToggle }: DeviceCardProps) {
  // Resolve services for badge display — include unknown services too
  const resolvedServices = device.services.map((uuid) => {
    const name = resolveServiceName(uuid);
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
                  className="text-[10px]"
                >
                  {count > 1 && <span className="mr-0.5 opacity-60">{count}x</span>}
                  {config?.label ?? name}
                </Badge>
              );
            })}
            {overflowCount > 0 && (
              <Badge variant="secondary" className="text-[10px]">
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
          {companyName && companyHex ? (
            <span className="text-[11px] text-muted-foreground">
              {companyName} - {companyHex}
            </span>
          ) : (
            <span />
          )}
          <span className="font-mono text-[10px] text-muted-foreground">
            {device.id}
          </span>
        </div>
      ) : companyName ? (
        <p className="mt-1 text-[11px] text-muted-foreground">{companyName}</p>
      ) : null}

      {/* Expandable detail section */}
      <div
        className={cn(
          "grid transition-all duration-200 ease-in-out",
          expanded ? "grid-rows-[1fr] opacity-100" : "grid-rows-[0fr] opacity-0"
        )}
      >
        <div className="overflow-hidden">
          <Separator className="my-3" />

          {/* Services table — no section heading */}
          {device.services.length > 0 ? (
            <table className="w-full table-fixed text-left">
              <thead>
                <tr className="border-b text-[10px] text-muted-foreground">
                  <th className="w-[35%] pb-1.5 font-medium">Service</th>
                  <th className="pb-1.5 font-medium">UUID</th>
                  <th className="w-[72px] pb-1.5 text-right font-medium">Type</th>
                </tr>
              </thead>
              <tbody className="text-xs">
                {device.services.map((uuid, i) => {
                  const name = resolveServiceName(uuid);
                  const isKnown = name !== uuid;
                  const standard = isStandardUuid(uuid);
                  return (
                    <tr key={i} className="border-b border-border/50 last:border-0">
                      <td className="py-1.5">
                        {isKnown ? name : (
                          <span className="text-muted-foreground">Unknown Service</span>
                        )}
                      </td>
                      <td className="truncate py-1.5 font-mono text-[10px] text-muted-foreground">
                        {uuid}
                      </td>
                      <td className="py-1.5 text-right text-[10px] text-muted-foreground">
                        {standard ? "Standard" : "Custom"}
                      </td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
          ) : (
            <p className="text-[11px] text-muted-foreground">No services advertised</p>
          )}
        </div>
      </div>
    </div>
  );
}
