import { Trash2, Download } from "lucide-react";
import { Button } from "@/components/ui/button";
import { Badge } from "@/components/ui/badge";
import { ScrollArea } from "@/components/ui/scroll-area";
import { Header } from "@/components/layout/Header";

interface LogEntry {
  timestamp: string;
  level: "info" | "warn" | "error";
  message: string;
}

const mockLogs: LogEntry[] = [
  {
    timestamp: "2025-01-15T10:23:01.042Z",
    level: "info",
    message: "Auracle service initialised",
  },
  {
    timestamp: "2025-01-15T10:23:01.108Z",
    level: "warn",
    message: "No BLE adapter detected — scanning unavailable",
  },
  {
    timestamp: "2025-01-15T10:23:01.204Z",
    level: "info",
    message: "Serial port monitor started on /dev/tty.*",
  },
];

function formatTimestamp(iso: string): string {
  const d = new Date(iso);
  const hh = String(d.getHours()).padStart(2, "0");
  const mm = String(d.getMinutes()).padStart(2, "0");
  const ss = String(d.getSeconds()).padStart(2, "0");
  const ms = String(d.getMilliseconds()).padStart(3, "0");
  return `${hh}:${mm}:${ss}.${ms}`;
}

const levelVariant: Record<LogEntry["level"], "info" | "warn" | "error"> = {
  info: "info",
  warn: "warn",
  error: "error",
};

export function Logs() {
  return (
    <div className="flex h-full flex-col">
      <Header title="Logs" description="Application event log" />
      <div className="flex items-center gap-2 border-b px-4 py-2">
        <Button variant="ghost" size="sm" className="gap-1.5">
          <Trash2 size={14} />
          Clear
        </Button>
        <Button variant="ghost" size="sm" disabled className="gap-1.5">
          <Download size={14} />
          Export
        </Button>
      </div>
      <ScrollArea className="flex-1">
        <div className="space-y-0 p-2">
          {mockLogs.map((log, i) => (
            <div
              key={i}
              className="flex items-start gap-2 rounded-sm px-2 py-1 font-mono text-xs leading-5 hover:bg-secondary/30"
            >
              <span className="shrink-0 text-muted-foreground">
                {formatTimestamp(log.timestamp)}
              </span>
              <Badge variant={levelVariant[log.level]} className="shrink-0 uppercase">
                {log.level}
              </Badge>
              <span className="text-foreground">{log.message}</span>
            </div>
          ))}
        </div>
      </ScrollArea>
    </div>
  );
}
