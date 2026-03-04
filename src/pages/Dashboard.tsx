import { Radio, Activity, Bluetooth, RefreshCw } from "lucide-react";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import { Skeleton } from "@/components/ui/skeleton";
import { Header } from "@/components/layout/Header";
import type { BluetoothAdapter } from "@/lib/tauri";

interface DashboardProps {
  connectedCount: number;
  bluetoothAdapter: {
    adapter: BluetoothAdapter | null;
    loading: boolean;
    error: string | null;
    refresh: () => Promise<void>;
  };
}

export function Dashboard({ connectedCount, bluetoothAdapter }: DashboardProps) {
  const { adapter, loading, error, refresh } = bluetoothAdapter;

  return (
    <div className="flex h-full flex-col">
      <Header title="Dashboard" />
      <div className="flex-1 space-y-4 p-4">
        <div className="grid grid-cols-3 gap-3">
          <Card>
            <CardHeader>
              <CardTitle className="flex items-center gap-2 text-muted-foreground">
                <Bluetooth size={14} />
                Bluetooth Adapter
              </CardTitle>
            </CardHeader>
            <CardContent>
              {loading ? (
                <Skeleton className="h-6 w-32" />
              ) : error ? (
                <div className="space-y-2">
                  <Badge variant="error" className="text-[10px]">Unavailable</Badge>
                  <p className="text-[11px] text-muted-foreground">{error}</p>
                  <Button variant="ghost" size="xs" className="gap-1" onClick={refresh}>
                    <RefreshCw size={12} />
                    Retry
                  </Button>
                </div>
              ) : adapter ? (
                <div className="space-y-1">
                  <Badge variant="info" className="text-[10px]">Available</Badge>
                  <p className="text-[11px] font-mono text-muted-foreground">
                    {adapter.name}
                  </p>
                </div>
              ) : null}
            </CardContent>
          </Card>
          <Card>
            <CardHeader>
              <CardTitle className="flex items-center gap-2 text-muted-foreground">
                <Radio size={14} />
                Connected Devices
              </CardTitle>
            </CardHeader>
            <CardContent>
              <span className="text-2xl font-semibold tabular-nums">
                {connectedCount}
              </span>
            </CardContent>
          </Card>
          <Card>
            <CardHeader>
              <CardTitle className="flex items-center gap-2 text-muted-foreground">
                <Activity size={14} />
                Active Streams
              </CardTitle>
            </CardHeader>
            <CardContent>
              <span className="text-2xl font-semibold tabular-nums">0</span>
            </CardContent>
          </Card>
        </div>

        <Card className="flex-1">
          <CardContent className="flex h-48 items-center justify-center p-4">
            <div className="flex flex-col items-center gap-2 rounded-lg border border-dashed border-border p-8">
              <Activity size={16} className="text-muted-foreground" />
              <span className="text-xs text-muted-foreground">
                No active streams
              </span>
            </div>
          </CardContent>
        </Card>
      </div>
    </div>
  );
}
