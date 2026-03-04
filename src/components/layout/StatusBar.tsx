import { Download, RefreshCw, RotateCcw } from "lucide-react";
import { cn } from "@/lib/utils";
import type { useUpdater } from "@/hooks/useUpdater";

interface StatusBarProps {
  connectedCount: number;
  updater: ReturnType<typeof useUpdater>;
}

export function StatusBar({ connectedCount, updater }: StatusBarProps) {
  const isConnected = connectedCount > 0;
  const { status, checkForUpdate, downloadAndInstall, restartApp } = updater;

  return (
    <div className="flex h-8 shrink-0 items-center justify-between border-t bg-background px-3 text-[11px] text-muted-foreground">
      <div className="flex items-center gap-3">
        <div className="flex items-center gap-1.5">
          <div
            className={cn(
              "h-1.5 w-1.5 rounded-full",
              isConnected
                ? "animate-pulse bg-green-500"
                : "bg-muted-foreground/30"
            )}
          />
          <span>
            {connectedCount} device{connectedCount !== 1 ? "s" : ""} connected
          </span>
        </div>
      </div>

      <div className="flex items-center gap-3">
        {status.state === "available" && (
          <button
            onClick={downloadAndInstall}
            className="flex items-center gap-1 text-primary hover:text-primary/80"
          >
            <Download size={12} />
            Update to {status.update.version}
          </button>
        )}
        {status.state === "downloading" && (
          <span className="flex items-center gap-1 text-primary">
            <RefreshCw size={12} className="animate-spin" />
            Updating {Math.round(status.progress * 100)}%
          </span>
        )}
        {status.state === "ready" && (
          <button
            onClick={restartApp}
            className="flex items-center gap-1 text-primary hover:text-primary/80"
          >
            <RotateCcw size={12} />
            Restart to finish update
          </button>
        )}
        {status.state === "error" && (
          <button
            onClick={checkForUpdate}
            className="flex items-center gap-1 text-destructive hover:text-destructive/80"
          >
            Update failed — retry
          </button>
        )}
        <span className="font-mono text-[10px]">v0.1.0</span>
      </div>
    </div>
  );
}
