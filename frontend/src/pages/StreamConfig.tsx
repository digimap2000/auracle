import { Play } from "lucide-react";
import { Button } from "@/components/ui/button";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Separator } from "@/components/ui/separator";
import { Header } from "@/components/layout/Header";

export function StreamConfig() {
  return (
    <div className="flex h-full flex-col">
      <Header title="Stream Config" description="Configure audio stream parameters" />
      <div className="flex-1 p-4">
        <Card className="max-w-lg">
          <CardHeader>
            <CardTitle>Stream Parameters</CardTitle>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="space-y-1.5">
              <label className="text-xs font-medium text-muted-foreground">
                Stream Name
              </label>
              <input
                type="text"
                placeholder="My Audio Stream"
                className="h-8 w-full rounded-md border bg-background px-3 text-xs text-foreground placeholder:text-muted-foreground focus:outline-none focus:ring-1 focus:ring-ring"
              />
            </div>

            <div className="grid grid-cols-2 gap-3">
              <div className="space-y-1.5">
                <label className="text-xs font-medium text-muted-foreground">
                  Codec
                </label>
                <select className="h-8 w-full rounded-md border bg-background px-3 text-xs text-foreground focus:outline-none focus:ring-1 focus:ring-ring">
                  <option>LC3</option>
                  <option>LC3plus</option>
                  <option>SBC</option>
                  <option>AAC</option>
                </select>
              </div>
              <div className="space-y-1.5">
                <label className="text-xs font-medium text-muted-foreground">
                  Sample Rate
                </label>
                <select className="h-8 w-full rounded-md border bg-background px-3 text-xs text-foreground focus:outline-none focus:ring-1 focus:ring-ring">
                  <option>16 kHz</option>
                  <option>24 kHz</option>
                  <option>32 kHz</option>
                  <option>48 kHz</option>
                </select>
              </div>
            </div>

            <div className="space-y-1.5">
              <label className="text-xs font-medium text-muted-foreground">
                Bitrate
              </label>
              <div className="flex items-center gap-3">
                <input
                  type="range"
                  min={16}
                  max={320}
                  defaultValue={96}
                  className="h-1.5 flex-1 cursor-pointer appearance-none rounded-full bg-secondary accent-primary"
                />
                <span className="w-16 text-right font-mono text-xs text-muted-foreground">
                  96 kbps
                </span>
              </div>
            </div>

            <Separator />

            <Button disabled className="w-full gap-1.5">
              <Play size={14} />
              Start Stream
            </Button>
          </CardContent>
        </Card>
      </div>
    </div>
  );
}
