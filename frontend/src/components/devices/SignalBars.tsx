import { cn } from "@/lib/utils";
import { rssiLevel } from "@/lib/ble-utils";

interface SignalBarsProps {
  rssi: number;
  className?: string;
}

const BAR_HEIGHTS = ["h-1", "h-2", "h-3", "h-4"] as const;

export function SignalBars({ rssi, className }: SignalBarsProps) {
  const { bars, bgColor } = rssiLevel(rssi);

  return (
    <div className={cn("flex items-end gap-0.5", className)}>
      {BAR_HEIGHTS.map((height, i) => (
        <div
          key={i}
          className={cn(
            "w-1 rounded-sm",
            height,
            i < bars ? bgColor : "bg-muted-foreground/20"
          )}
        />
      ))}
    </div>
  );
}
