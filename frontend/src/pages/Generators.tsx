import { useState, useCallback } from "react";
import {
  ChevronDown,
  Plus,
  EllipsisVertical,
  Fingerprint,
  Megaphone,
  Layers,
  AudioLines,
  PanelLeftClose,
  PanelLeftOpen,
} from "lucide-react";
import { cn } from "@/lib/utils";
import {
  Collapsible,
  CollapsibleContent,
  CollapsibleTrigger,
} from "@/components/ui/collapsible";
import {
  ResizablePanelGroup,
  ResizablePanel,
  ResizableHandle,
} from "@/components/ui/resizable";
import { ScrollArea } from "@/components/ui/scroll-area";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { IdentityTab } from "@/components/generators/IdentityTab";
import { ICON_SIZE } from "@/lib/icons";
import { CapLabel } from "@/components/ui/cap-label";

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

const actionButtonClass =
  "flex size-5 items-center justify-center rounded-md text-muted-foreground hover:bg-accent hover:text-accent-foreground";

interface SectionHeadingProps {
  title: string;
  isActive?: boolean;
  onAdd?: () => void;
  onClick?: () => void;
}

function SectionHeading({ title, isActive, onAdd, onClick }: SectionHeadingProps) {
  return (
    <div
      className={cn(
        "group/heading flex h-8 items-center gap-1 rounded-md px-2 transition-colors hover:bg-secondary/50",
        isActive && "bg-secondary",
      )}
    >
      <CollapsibleTrigger
        className="flex flex-1 items-center gap-1.5 text-sm"
        onClick={onClick}
      >
        <ChevronDown className="size-3 transition-transform group-data-[state=closed]/collapsible:-rotate-90" />
        <CapLabel className="font-medium">{title}</CapLabel>
      </CollapsibleTrigger>
      {onAdd && (
        <button
          title={`Add ${title.toLowerCase()}`}
          onClick={onAdd}
          className={cn(actionButtonClass, "opacity-0 group-hover/heading:opacity-100")}
        >
          <Plus className="size-4" />
        </button>
      )}
      <button
        title="More options"
        className={cn(actionButtonClass, "opacity-0 group-hover/heading:opacity-100")}
      >
        <EllipsisVertical className="size-4" />
      </button>
    </div>
  );
}

// ── Sidebar menu item ───────────────────────────────────────────────

interface MenuItemProps {
  label: string;
  isActive?: boolean;
  onClick?: () => void;
}

function MenuItem({ label, isActive, onClick }: MenuItemProps) {
  return (
    <button
      onClick={onClick}
      className={cn(
        "flex h-8 w-full items-center rounded-md px-2 text-sm transition-colors hover:bg-secondary/50",
        isActive
          ? "bg-accent/15 text-accent"
          : "text-muted-foreground hover:text-foreground",
      )}
    >
      <CapLabel>{label}</CapLabel>
    </button>
  );
}

// ── Sidebar layout persistence ──────────────────────────────────────

const SIDEBAR_STORAGE_KEY = "auracle-gen-sidebar-v2";
const DEFAULT_SIDEBAR_PCT = "18%";

function getSavedSidebarSize(): string {
  const stored = localStorage.getItem(SIDEBAR_STORAGE_KEY);
  if (stored) {
    const parsed = parseFloat(stored);
    if (!isNaN(parsed) && parsed >= 10 && parsed <= 30) return `${parsed}%`;
  }
  return DEFAULT_SIDEBAR_PCT;
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
  const [sidebarSize] = useState(getSavedSidebarSize);

  const handleLayoutChanged = useCallback(
    (layout: Record<string, number>) => {
      const size = layout["sidebar"];
      if (size != null) {
        localStorage.setItem(SIDEBAR_STORAGE_KEY, String(size));
      }
    },
    [],
  );

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
    <ResizablePanelGroup
      orientation="horizontal"
      onLayoutChanged={handleLayoutChanged}
      className="h-full"
    >
      {/* ── Sidebar ─────────────────────────────────────────── */}
      <ResizablePanel
        id="sidebar"
        defaultSize={sidebarSize}
        minSize="10%"
        maxSize="30%"
        className={cn(
          "transition-[flex] duration-200 ease-linear",
          !sidebarOpen && "!flex-[0]",
        )}
      >
        <nav className="flex h-full flex-col overflow-hidden bg-sidebar text-sidebar-foreground">
          <div className="flex flex-col gap-0 p-3">
            <span className="text-sm font-medium">Generators</span>
            <span className="text-xs text-muted-foreground">
              Broadcast stream generators
            </span>
          </div>
          <ScrollArea className="flex-1">
            <div className="space-y-1 px-1">
              <Collapsible defaultOpen className="group/collapsible">
                <SectionHeading title="Scenarios" />
                <CollapsibleContent>
                  <div className="ml-3 space-y-0.5 border-l pl-2">
                    {scenarios.map((scenario) => (
                      <MenuItem key={scenario.id} label={scenario.name} />
                    ))}
                  </div>
                </CollapsibleContent>
              </Collapsible>

              <Collapsible defaultOpen className="group/collapsible">
                <SectionHeading title="Test Peers" onAdd={handleAddPeer} />
                <CollapsibleContent>
                  <div className="ml-3 space-y-0.5 border-l pl-2">
                    {peers.map((peer) => (
                      <MenuItem
                        key={peer.id}
                        label={peer.name}
                        isActive={selectedId === peer.id}
                        onClick={() => setSelectedId(peer.id)}
                      />
                    ))}
                  </div>
                </CollapsibleContent>
              </Collapsible>

              <Collapsible defaultOpen className="group/collapsible">
                <SectionHeading title="Behaviour Overlays" />
                <CollapsibleContent>
                  <div className="ml-3 space-y-0.5 border-l pl-2">
                    {overlays.map((overlay) => (
                      <MenuItem key={overlay.id} label={overlay.name} />
                    ))}
                  </div>
                </CollapsibleContent>
              </Collapsible>
            </div>
          </ScrollArea>
        </nav>
      </ResizablePanel>

      <ResizableHandle />

      {/* ── Main content ────────────────────────────────────── */}
      <ResizablePanel id="content">
        <Tabs defaultValue="identity" className="flex h-full flex-col overflow-hidden">
          <div className="flex shrink-0 items-center gap-1 px-2 pt-1">
            <button
              onClick={() => setSidebarOpen((prev) => !prev)}
              className="flex size-7 items-center justify-center rounded-md text-muted-foreground hover:bg-accent hover:text-foreground"
              title={sidebarOpen ? "Close sidebar" : "Open sidebar"}
            >
              {sidebarOpen ? (
                <PanelLeftClose size={ICON_SIZE.sm} />
              ) : (
                <PanelLeftOpen size={ICON_SIZE.sm} />
              )}
            </button>
            <TabsList variant="line" className="h-9">
              <TabsTrigger value="identity" className="gap-1.5 text-xs">
                <Fingerprint size={ICON_SIZE.sm} />
                Identity
              </TabsTrigger>
              <TabsTrigger value="advertising" className="gap-1.5 text-xs">
                <Megaphone size={ICON_SIZE.sm} />
                Advertising
              </TabsTrigger>
              <TabsTrigger value="services" className="gap-1.5 text-xs">
                <Layers size={ICON_SIZE.sm} />
                Services
              </TabsTrigger>
              <TabsTrigger value="streams" className="gap-1.5 text-xs">
                <AudioLines size={ICON_SIZE.sm} />
                Streams
              </TabsTrigger>
            </TabsList>
          </div>

          <TabsContent
            value="identity"
            className="min-h-0 flex-1 overflow-hidden data-[state=inactive]:hidden"
            forceMount
          >
            <IdentityTab key={selectedId} peerId={selectedId} />
          </TabsContent>
          <TabsContent value="advertising" className="flex-1 overflow-auto">
            <div className="p-4 text-center text-muted-foreground">
              <Megaphone className="mx-auto mb-3 size-8 opacity-30" />
              <p className="text-sm">Advertising parameters</p>
              <p className="mt-1 text-xs">
                Broadcast name, interval, and advertising data configuration
              </p>
            </div>
          </TabsContent>
          <TabsContent value="services" className="flex-1 overflow-auto">
            <div className="p-4 text-center text-muted-foreground">
              <Layers className="mx-auto mb-3 size-8 opacity-30" />
              <p className="text-sm">GATT services</p>
              <p className="mt-1 text-xs">
                Service definitions and characteristic configuration
              </p>
            </div>
          </TabsContent>
          <TabsContent value="streams" className="flex-1 overflow-auto">
            <div className="p-4 text-center text-muted-foreground">
              <AudioLines className="mx-auto mb-3 size-8 opacity-30" />
              <p className="text-sm">Audio streams</p>
              <p className="mt-1 text-xs">
                Codec, sample rate, bitrate, and BIS configuration
              </p>
            </div>
          </TabsContent>
        </Tabs>
      </ResizablePanel>
    </ResizablePanelGroup>
  );
}
