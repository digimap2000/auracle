import {
  Bluetooth,
  Cpu,
  Loader2,
  RefreshCw,
  TriangleAlert,
} from "lucide-react";
import { cn } from "@/lib/utils";
import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import { ScrollArea } from "@/components/ui/scroll-area";
import { ICON_SIZE } from "@/lib/icons";
import type { DaemonUnit } from "@/lib/tauri";

interface HomeProps {
  units: DaemonUnit[];
  loading: boolean;
  error: string | null;
  refresh: () => void;
  activeUnitId: string | null;
  onSelectUnit: (id: string) => void;
}

function kindIcon(kind: string) {
  switch (kind) {
    case "host-bluetooth":
      return <Bluetooth size={ICON_SIZE.md} />;
    default:
      return <Cpu size={ICON_SIZE.md} />;
  }
}

function kindLabel(kind: string): string {
  switch (kind) {
    case "host-bluetooth":
      return "Host Bluetooth Adapter";
    case "nrf5340-audio-dk":
      return "nRF5340 Audio DK";
    default:
      return kind;
  }
}

function capabilityLabel(cap: string): string {
  switch (cap) {
    case "ble-scan":
      return "BLE Scan";
    case "le-audio-sink":
      return "LE Audio Sink";
    case "le-audio-source":
      return "LE Audio Source";
    case "unicast-client":
      return "Unicast Client";
    default:
      return cap;
  }
}

interface UnitCardProps {
  unit: DaemonUnit;
  active: boolean;
  onSelect: () => void;
}

function UnitCard({ unit, active, onSelect }: UnitCardProps) {
  const displayName = unit.product || kindLabel(unit.kind);
  const selectable = unit.present;

  return (
    <button
      type="button"
      disabled={!selectable}
      onClick={onSelect}
      className={cn(
        "w-full rounded-lg border p-4 text-left transition-colors",
        selectable
          ? "cursor-pointer hover:bg-secondary/50"
          : "cursor-default opacity-50",
        active ? "border-primary bg-primary/5" : "bg-card"
      )}
    >
      <div className="flex items-start gap-3">
        <div
          className={cn(
            "mt-0.5",
            active ? "text-primary" : "text-muted-foreground"
          )}
        >
          {kindIcon(unit.kind)}
        </div>
        <div className="min-w-0 flex-1">
          <div className="flex items-center gap-2">
            <span className="text-sm font-medium text-foreground">
              {displayName}
            </span>
            <div
              className={cn(
                "size-2 rounded-full",
                unit.present ? "bg-success" : "bg-muted-foreground"
              )}
            />
          </div>
          <p className="text-xs text-muted-foreground">{kindLabel(unit.kind)}</p>

          {unit.capabilities.length > 0 && (
            <div className="mt-2 flex flex-wrap gap-1">
              {unit.capabilities.map((cap) => (
                <Badge key={cap} variant="outline" className="text-xs">
                  {capabilityLabel(cap)}
                </Badge>
              ))}
            </div>
          )}

          <div className="mt-2 space-y-0.5 font-mono text-xs text-muted-foreground">
            {unit.serial && <p>Serial: {unit.serial}</p>}
            {unit.firmware_version && <p>Firmware: {unit.firmware_version}</p>}
            {unit.vendor && <p>Vendor: {unit.vendor}</p>}
          </div>
        </div>
      </div>
    </button>
  );
}

export function Home({
  units,
  loading,
  error,
  refresh,
  activeUnitId,
  onSelectUnit,
}: HomeProps) {
  // Error state
  if (error) {
    return (
      <div className="flex h-full flex-col">
        <div className="flex flex-1 items-center justify-center">
          <div className="text-center">
            <TriangleAlert
              size={ICON_SIZE.xl}
              className="mx-auto mb-3 text-destructive opacity-70"
            />
            <p className="text-sm text-destructive">{error}</p>
            <Button variant="outline" size="sm" className="mt-4" onClick={refresh}>
              <RefreshCw size={ICON_SIZE.sm} />
              Retry
            </Button>
          </div>
        </div>
      </div>
    );
  }

  // Loading state (initial load only)
  if (loading && units.length === 0) {
    return (
      <div className="flex h-full flex-col">
        <div className="flex flex-1 items-center justify-center">
          <div className="text-center text-muted-foreground">
            <Loader2
              size={ICON_SIZE.xl}
              className="mx-auto mb-3 animate-spin opacity-50"
            />
            <p className="text-sm">Connecting to daemon...</p>
          </div>
        </div>
      </div>
    );
  }

  // Empty state
  if (units.length === 0) {
    return (
      <div className="flex h-full flex-col">
        <div className="flex flex-1 items-center justify-center">
          <div className="text-center text-muted-foreground">
            <Cpu size={ICON_SIZE.xl} className="mx-auto mb-3 opacity-30" />
            <p className="text-sm">No units found</p>
            <p className="mt-1 text-xs">
              Ensure auracle-daemon is running on port 50051
            </p>
            <Button variant="outline" size="sm" className="mt-4" onClick={refresh}>
              <RefreshCw size={ICON_SIZE.sm} />
              Refresh
            </Button>
          </div>
        </div>
      </div>
    );
  }

  // Unit list
  return (
    <div className="flex h-full flex-col">
      <div className="flex-1 overflow-hidden">
        <ScrollArea className="h-full">
          <div className="mx-auto w-full max-w-2xl space-y-3 px-6 py-6">
            {units.map((unit) => (
              <UnitCard
                key={unit.id}
                unit={unit}
                active={unit.id === activeUnitId}
                onSelect={() => onSelectUnit(unit.id)}
              />
            ))}
          </div>
        </ScrollArea>
      </div>
    </div>
  );
}
