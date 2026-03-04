import { cn } from "@/lib/utils";

interface StatusBarProps {
  connectedCount: number;
}

export function StatusBar({ connectedCount }: StatusBarProps) {
  const isConnected = connectedCount > 0;

  return (
    <div className="flex h-8 shrink-0 items-center justify-between border-t bg-background px-3 text-[11px] text-muted-foreground">
      <div className="flex items-center gap-3">
        <div className="flex items-center gap-1.5">
          <div
            className={cn(
              "h-1.5 w-1.5 rounded-full",
              isConnected ? "animate-pulse bg-green-500" : "bg-muted-foreground/30"
            )}
          />
          <span>
            {connectedCount} device{connectedCount !== 1 ? "s" : ""} connected
          </span>
        </div>
      </div>
      <span className="font-mono text-[10px]">v0.1.0</span>
    </div>
  );
}
