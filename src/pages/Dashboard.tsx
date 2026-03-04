import { Radio, Activity } from "lucide-react";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Header } from "@/components/layout/Header";

interface DashboardProps {
  connectedCount: number;
}

export function Dashboard({ connectedCount }: DashboardProps) {
  return (
    <div className="flex h-full flex-col">
      <Header title="Dashboard" />
      <div className="flex-1 space-y-4 p-4">
        <div className="grid grid-cols-2 gap-3">
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
