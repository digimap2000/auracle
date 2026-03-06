import { useState } from "react";
import {
  ChevronDown,
  Plus,
  EllipsisVertical,
  Fingerprint,
  Megaphone,
  Layers,
  AudioLines,
} from "lucide-react";
import { cn } from "@/lib/utils";
import {
  Collapsible,
  CollapsibleContent,
  CollapsibleTrigger,
} from "@/components/ui/collapsible";
import {
  Sidebar,
  SidebarContent,
  SidebarGroup,
  SidebarHeader,
  SidebarMenu,
  SidebarMenuItem,
  SidebarMenuSub,
  SidebarMenuSubButton,
  SidebarMenuSubItem,
  SidebarProvider,
  SidebarTrigger,
} from "@/components/ui/sidebar";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { IdentityTab } from "@/components/generators/IdentityTab";

// ── Sidebar data types ──────────────────────────────────────────────

interface TestPeer {
  id: string;
  name: string;
}

interface Scenario {
  id: string;
  name: string;
}

interface BehaviourOverlay {
  id: string;
  name: string;
}

// ── Placeholder data ────────────────────────────────────────────────

const INITIAL_SCENARIOS: Scenario[] = [
  { id: "s1", name: "Single speaker broadcast" },
  { id: "s2", name: "Multi-room sync" },
];

const INITIAL_PEERS: TestPeer[] = [
  { id: "1", name: "Mu-so 2 Kitchen" },
  { id: "2", name: "Bathys V2 Proto" },
  { id: "3", name: "nRF5340 DK #1" },
];

const INITIAL_OVERLAYS: BehaviourOverlay[] = [
  { id: "o1", name: "Disconnect after 30 s" },
  { id: "o2", name: "Corrupt metadata" },
];

// ── Collapsible section heading ─────────────────────────────────────

const headingButtonClass =
  "flex size-5 items-center justify-center rounded-md text-sidebar-foreground/70 hover:bg-sidebar-accent hover:text-sidebar-accent-foreground";

interface SectionHeadingProps {
  title: string;
  onAdd?: () => void;
}

function SectionHeading({ title, onAdd }: SectionHeadingProps) {
  return (
    <div className="flex h-8 items-center gap-1 px-2">
      <CollapsibleTrigger className="flex flex-1 items-center gap-1.5 text-sm">
        <ChevronDown className="size-3 transition-transform group-data-[state=closed]/collapsible:-rotate-90" />
        <span className="truncate font-medium">{title}</span>
      </CollapsibleTrigger>
      {onAdd && (
        <button
          title={`Add ${title.toLowerCase()}`}
          onClick={onAdd}
          className={headingButtonClass}
        >
          <Plus className="size-4" />
        </button>
      )}
      <button title="More options" className={headingButtonClass}>
        <EllipsisVertical className="size-4" />
      </button>
    </div>
  );
}

// ── Page ─────────────────────────────────────────────────────────────

export function Generators() {
  const [scenarios] = useState<Scenario[]>(INITIAL_SCENARIOS);
  const [peers, setPeers] = useState<TestPeer[]>(INITIAL_PEERS);
  const [overlays] = useState<BehaviourOverlay[]>(INITIAL_OVERLAYS);
  const [selectedId, setSelectedId] = useState<string>(
    INITIAL_PEERS[0]?.id ?? "",
  );
  const [sidebarOpen, setSidebarOpen] = useState(true);

  const handleAddPeer = () => {
    const id = crypto.randomUUID();
    const newPeer: TestPeer = {
      id,
      name: `Peer ${peers.length + 1}`,
    };
    setPeers((prev) => [...prev, newPeer]);
    setSelectedId(id);
  };

  return (
    <SidebarProvider
      className="h-full !min-h-0 bg-sidebar"
      open={sidebarOpen}
      onOpenChange={setSidebarOpen}
    >
      <Sidebar
        collapsible="none"
        className={cn(
          "shrink-0 overflow-hidden transition-[width] duration-200 ease-linear",
          sidebarOpen ? "!w-52" : "!w-0",
        )}
      >
        <SidebarHeader>
          <div className="flex flex-col gap-0 px-0.5">
            <span className="text-sm font-medium">Generators</span>
            <span className="text-[11px] text-sidebar-foreground/50">
              Broadcast stream generators
            </span>
          </div>
        </SidebarHeader>
        <SidebarContent>
          <SidebarGroup>
            <SidebarMenu>
              {/* Scenarios */}
              <Collapsible defaultOpen className="group/collapsible">
                <SidebarMenuItem>
                  <SectionHeading title="Scenarios" />
                  <CollapsibleContent>
                    <SidebarMenuSub>
                      {scenarios.map((scenario) => (
                        <SidebarMenuSubItem key={scenario.id}>
                          <SidebarMenuSubButton asChild>
                            <button>
                              <span>{scenario.name}</span>
                            </button>
                          </SidebarMenuSubButton>
                        </SidebarMenuSubItem>
                      ))}
                    </SidebarMenuSub>
                  </CollapsibleContent>
                </SidebarMenuItem>
              </Collapsible>

              {/* Test Peers */}
              <Collapsible defaultOpen className="group/collapsible">
                <SidebarMenuItem>
                  <SectionHeading title="Test Peers" onAdd={handleAddPeer} />
                  <CollapsibleContent>
                    <SidebarMenuSub>
                      {peers.map((peer) => (
                        <SidebarMenuSubItem key={peer.id}>
                          <SidebarMenuSubButton
                            asChild
                            isActive={selectedId === peer.id}
                          >
                            <button onClick={() => setSelectedId(peer.id)}>
                              <span>{peer.name}</span>
                            </button>
                          </SidebarMenuSubButton>
                        </SidebarMenuSubItem>
                      ))}
                    </SidebarMenuSub>
                  </CollapsibleContent>
                </SidebarMenuItem>
              </Collapsible>

              {/* Behaviour Overlays */}
              <Collapsible defaultOpen className="group/collapsible">
                <SidebarMenuItem>
                  <SectionHeading title="Behaviour Overlays" />
                  <CollapsibleContent>
                    <SidebarMenuSub>
                      {overlays.map((overlay) => (
                        <SidebarMenuSubItem key={overlay.id}>
                          <SidebarMenuSubButton asChild>
                            <button>
                              <span>{overlay.name}</span>
                            </button>
                          </SidebarMenuSubButton>
                        </SidebarMenuSubItem>
                      ))}
                    </SidebarMenuSub>
                  </CollapsibleContent>
                </SidebarMenuItem>
              </Collapsible>
            </SidebarMenu>
          </SidebarGroup>
        </SidebarContent>
      </Sidebar>

      {/* Main content — rounded inset card */}
      <div
        className={cn(
          "flex flex-1 flex-col overflow-hidden rounded-xl bg-background shadow-sm transition-[margin] duration-200 ease-linear",
          sidebarOpen ? "my-2 mr-2" : "m-2",
        )}
      >
        <Tabs defaultValue="identity" className="flex flex-1 flex-col">
          <div className="flex shrink-0 items-center gap-1 px-2 pt-1">
            <SidebarTrigger className="size-7" />
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

          <TabsContent
            value="identity"
            className="flex-1 overflow-hidden data-[state=inactive]:hidden"
            forceMount
          >
            <IdentityTab key={selectedId} peerId={selectedId} />
          </TabsContent>
          <TabsContent value="advertising" className="flex-1 overflow-auto">
            <div className="p-4 text-center text-muted-foreground">
              <Megaphone className="mx-auto mb-3 size-8 opacity-30" />
              <p className="text-sm">Advertising parameters</p>
              <p className="mt-1 text-[11px]">
                Broadcast name, interval, and advertising data configuration
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
    </SidebarProvider>
  );
}
