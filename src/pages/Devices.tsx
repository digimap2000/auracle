import { RefreshCw, Search, Radio } from "lucide-react";
import { Button } from "@/components/ui/button";
import { Card, CardContent } from "@/components/ui/card";
import { Header } from "@/components/layout/Header";

interface DevicesProps {
  scanning: boolean;
  onScan: () => void;
}

export function Devices({ scanning, onScan }: DevicesProps) {
  return (
    <div className="flex h-full flex-col">
      <Header title="Devices" description="Discover and manage BLE and serial devices" />
      <div className="flex-1 space-y-3 p-4">
        <div className="flex items-center gap-2">
          <Button
            size="sm"
            onClick={onScan}
            disabled={scanning}
            className="gap-1.5"
          >
            <RefreshCw
              size={14}
              className={scanning ? "animate-spin" : undefined}
            />
            {scanning ? "Scanning..." : "Scan"}
          </Button>
          <div className="relative flex-1">
            <Search
              size={14}
              className="absolute left-2.5 top-1/2 -translate-y-1/2 text-muted-foreground"
            />
            <input
              type="text"
              placeholder="Filter devices..."
              className="h-7 w-full rounded-md border bg-background pl-8 pr-3 text-xs text-foreground placeholder:text-muted-foreground focus:outline-none focus:ring-1 focus:ring-ring"
            />
          </div>
        </div>

        <Card>
          <CardContent className="flex h-48 items-center justify-center p-4">
            <div className="flex flex-col items-center gap-2">
              <Radio size={16} className="text-muted-foreground" />
              <p className="text-center text-xs text-muted-foreground">
                No devices found. Start a scan to discover
                <br />
                BLE devices and serial ports.
              </p>
            </div>
          </CardContent>
        </Card>
      </div>
    </div>
  );
}
