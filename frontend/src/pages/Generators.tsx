import { useState } from "react";
import {
  ChevronRight,
  Plus,
  Radio,
  Fingerprint,
  Megaphone,
  Layers,
  AudioLines,
} from "lucide-react";
import { Button } from "@/components/ui/button";
import { ScrollArea } from "@/components/ui/scroll-area";
import { Separator } from "@/components/ui/separator";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { Header } from "@/components/layout/Header";
import { IdentityTab } from "@/components/generators/IdentityTab";

interface DefinedDevice {
  id: string;
  name: string;
}

const INITIAL_DEVICES: DefinedDevice[] = [
  { id: "1", name: "Mu-so 2 Kitchen" },
  { id: "2", name: "Bathys V2 Proto" },
  { id: "3", name: "nRF5340 DK #1" },
];

export function Generators() {
  const [devices, setDevices] = useState<DefinedDevice[]>(INITIAL_DEVICES);
  const [selectedId, setSelectedId] = useState<string>(INITIAL_DEVICES[0]?.id ?? "");
  const [collapsed, setCollapsed] = useState(false);

  const handleAddDevice = () => {
    const id = crypto.randomUUID();
    const newDevice: DefinedDevice = {
      id,
      name: `Device ${devices.length + 1}`,
    };
    setDevices((prev) => [...prev, newDevice]);
    setSelectedId(id);
  };

  return (
    <div className="flex h-full flex-col">
      <Header
        title="Generators"
        description="Configure broadcast stream generators"
      />
      <div className="flex flex-1 overflow-hidden">
        {/* Device sidebar */}
        <div
          className={`flex shrink-0 flex-col border-r transition-[width] ${collapsed ? "w-10" : "w-56"}`}
        >
          <div className="flex h-9 shrink-0 items-center justify-between px-3">
            {!collapsed && (
              <span className="text-[11px] font-medium text-muted-foreground uppercase tracking-wider">
                Devices
              </span>
            )}
            <Button
              variant="ghost"
              size="icon-xs"
              onClick={() => setCollapsed((c) => !c)}
              className="text-muted-foreground"
            >
              <ChevronRight
                size={14}
                className={`transition-transform ${collapsed ? "" : "rotate-180"}`}
              />
            </Button>
          </div>
          <Separator />
          {!collapsed && (
            <>
              <ScrollArea className="flex-1">
                <div className="p-1.5">
                  {devices.map((device) => (
                    <button
                      key={device.id}
                      onClick={() => setSelectedId(device.id)}
                      className={`flex w-full items-center gap-2 rounded-md px-2.5 py-1.5 text-left text-xs transition-colors ${
                        selectedId === device.id
                          ? "bg-secondary text-foreground"
                          : "text-muted-foreground hover:bg-secondary/50 hover:text-foreground"
                      }`}
                    >
                      <Radio size={14} className="shrink-0" />
                      <span className="truncate">{device.name}</span>
                    </button>
                  ))}
                </div>
              </ScrollArea>
              <Separator />
              <div className="p-1.5">
                <Button
                  variant="ghost"
                  size="xs"
                  className="w-full justify-start gap-2 text-muted-foreground"
                  onClick={handleAddDevice}
                >
                  <Plus size={14} />
                  Add Device
                </Button>
              </div>
            </>
          )}
        </div>

        {/* Tabbed configuration panel */}
        <div className="flex flex-1 flex-col overflow-hidden">
          <Tabs defaultValue="identity" className="flex flex-1 flex-col">
            <div className="shrink-0 border-b px-4">
              <TabsList variant="line" className="h-9">
                <TabsTrigger value="identity" className="gap-1.5 text-xs">
                  <Fingerprint size={14} />
                  Identity
                </TabsTrigger>
                <TabsTrigger value="advertising" className="gap-1.5 text-xs">
                  <Megaphone size={14} />
                  Advertising
                </TabsTrigger>
                <TabsTrigger value="services" className="gap-1.5 text-xs">
                  <Layers size={14} />
                  Services
                </TabsTrigger>
                <TabsTrigger value="streams" className="gap-1.5 text-xs">
                  <AudioLines size={14} />
                  Streams
                </TabsTrigger>
              </TabsList>
            </div>

            <TabsContent value="identity" className="flex-1 overflow-hidden data-[state=inactive]:hidden" forceMount>
              <IdentityTab key={selectedId} deviceId={selectedId} />
            </TabsContent>
            <TabsContent value="advertising" className="flex-1 overflow-auto">
              <div className="p-4 text-center text-muted-foreground">
                <Megaphone className="mx-auto mb-3 size-8 opacity-30" />
                <p className="text-sm">
                  Advertising parameters
                </p>
                <p className="mt-1 text-[11px]">
                  Broadcast name, interval, and advertising data
                  configuration
                </p>
              </div>
            </TabsContent>
            <TabsContent value="services" className="flex-1 overflow-auto">
              <div className="p-4 text-center text-muted-foreground">
                <Layers className="mx-auto mb-3 size-8 opacity-30" />
                <p className="text-sm">GATT services</p>
                <p className="mt-1 text-[11px]">
                  Service definitions and characteristic configuration
                </p>
              </div>
            </TabsContent>
            <TabsContent value="streams" className="flex-1 overflow-auto">
              <div className="p-4 text-center text-muted-foreground">
                <AudioLines className="mx-auto mb-3 size-8 opacity-30" />
                <p className="text-sm">Audio streams</p>
                <p className="mt-1 text-[11px]">
                  Codec, sample rate, bitrate, and BIS configuration
                </p>
              </div>
            </TabsContent>
          </Tabs>
        </div>
      </div>
    </div>
  );
}
