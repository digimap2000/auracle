import { useState, useCallback, useMemo } from "react";
import {
  Info,
  TriangleAlert,
  CircleX,
  Speaker,
  Headphones,
  Ear,
  Mic,
  Radio,
  Volume2,
  Bluetooth,
} from "lucide-react";
import type { LucideIcon } from "lucide-react";
import { Input } from "@/components/ui/input";
import { Textarea } from "@/components/ui/textarea";
import { Badge } from "@/components/ui/badge";
import { ScrollArea } from "@/components/ui/scroll-area";
import { Separator } from "@/components/ui/separator";
import {
  Tabs,
  TabsList,
  TabsTrigger,
  TabsContent,
} from "@/components/ui/tabs";
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/select";
import {
  ResizablePanelGroup,
  ResizablePanel,
  ResizableHandle,
} from "@/components/ui/resizable";
import { cn } from "@/lib/utils";
import { FormField } from "@/components/forms/FormField";
import { DataInput } from "@/components/forms/DataInput";
import { HexDump } from "@/components/forms/HexDump";
import {
  DEFAULT_DEVICE_IDENTITY,
  MANUFACTURER_OPTIONS,
  APPEARANCE_OPTIONS,
  type DeviceIdentity,
} from "./types";

// ── Diagnostics ─────────────────────────────────────────────────────

type DiagnosticLevel = "warning" | "error";

interface Diagnostic {
  level: DiagnosticLevel;
  field: string;
  message: string;
}

function validate(identity: DeviceIdentity): Diagnostic[] {
  const diags: Diagnostic[] = [];

  if (!identity.advertisedName.trim()) {
    diags.push({
      level: "warning",
      field: "advertisedName",
      message: "Advertised Name is empty — scanners will not display a name for this device.",
    });
  }

  return diags;
}

// ── Help content keyed by field name ────────────────────────────────

interface FieldHelp {
  title: string;
  hint: string;
  detail: string;
  example?: string;
}

const FIELD_HELP: Record<string, FieldHelp> = {
  advertisedName: {
    title: "Advertised Name",
    hint: "Complete Local Name included in advertising data",
    detail:
      "The GAP Complete Local Name (AD Type 0x09) that BLE scanners will see when discovering this device. It appears in scan results and is the primary human-readable identifier for the broadcast. Maximum 248 bytes UTF-8 encoded.",
    example: "Mu-so 2 Kitchen, Bathys V2, nRF5340 DK Auracast",
  },
  appearanceCode: {
    title: "Appearance",
    hint: "GAP Appearance value describing device category",
    detail:
      "The Appearance characteristic (AD Type 0x19) helps scanners display an appropriate icon or description for this device. Values are defined in the Bluetooth SIG Assigned Numbers document, Section 6. The 16-bit value encodes both a category and sub-category.",
    example: "0x0042 Standalone Speaker, 0x0841 Headphones, 0x0B41 Generic Broadcast Device",
  },
  manufacturerId: {
    title: "Manufacturer",
    hint: "Bluetooth SIG registered company identifier",
    detail:
      "The company ID is included in manufacturer-specific data (AD Type 0xFF). The first two bytes of the manufacturer data field carry this 16-bit identifier, followed by any additional payload bytes. Values are assigned by the Bluetooth SIG — see bluetooth.com/specifications/assigned-numbers for the full registry.",
    example: "0x029A Focal Naim, 0x0059 Nordic Semiconductor, 0x004C Apple",
  },
  manufacturerData: {
    title: "Manufacturer Data",
    hint: "Raw hex bytes appended after the company ID",
    detail:
      "This data follows the 2-byte company ID in the manufacturer-specific AD structure (AD Type 0xFF). The content and format are defined by the manufacturer. For Focal Naim devices, this carries Auracast broadcast configuration. Enter hex values only — the 2-byte company ID prefix is added automatically.",
    example: "0A FF 12 B4 00 01",
  },
  description: {
    title: "Description",
    hint: "Internal note — not included in broadcast data",
    detail:
      "This description is stored locally and not included in any BLE advertising data. Use it to document the purpose of this configuration — for example, which test scenario or certification requirement it supports.",
    example: "Kitchen speaker test config for Auracast certification run #3",
  },
};

// ── Appearance icon mapping ─────────────────────────────────────────

const APPEARANCE_ICONS: Record<string, LucideIcon> = {
  "0x0000": Bluetooth,
  "0x0041": Volume2,
  "0x0042": Speaker,
  "0x0043": Speaker,
  "0x0044": Speaker,
  "0x0045": Volume2,
  "0x0841": Headphones,
  "0x0842": Headphones,
  "0x0843": Ear,
  "0x0941": Ear,
  "0x0942": Ear,
  "0x0943": Ear,
  "0x0A41": Mic,
  "0x0B41": Radio,
};

// ── Component ───────────────────────────────────────────────────────

interface IdentityTabProps {
  deviceId: string;
}

export function IdentityTab({ deviceId: _deviceId }: IdentityTabProps) {
  const [identity, setIdentity] = useState<DeviceIdentity>(
    DEFAULT_DEVICE_IDENTITY
  );
  const [hoveredField, setHoveredField] = useState<string | null>(null);

  const update = useCallback(
    <K extends keyof DeviceIdentity>(field: K, value: DeviceIdentity[K]) => {
      setIdentity((prev) => ({ ...prev, [field]: value }));
    },
    []
  );

  const help = hoveredField ? FIELD_HELP[hoveredField] : null;

  const appearanceLabel = useMemo(
    () =>
      APPEARANCE_OPTIONS.find((o) => o.code === identity.appearanceCode)
        ?.label ?? "Unknown",
    [identity.appearanceCode]
  );

  const manufacturerName = useMemo(
    () =>
      MANUFACTURER_OPTIONS.find((o) => o.id === identity.manufacturerId)
        ?.name ?? null,
    [identity.manufacturerId]
  );

  const AppearanceIcon =
    APPEARANCE_ICONS[identity.appearanceCode] ?? Bluetooth;

  const diagnostics = useMemo(() => validate(identity), [identity]);
  const warnings = useMemo(() => diagnostics.filter((d) => d.level === "warning"), [diagnostics]);
  const errors = useMemo(() => diagnostics.filter((d) => d.level === "error"), [diagnostics]);

  return (
    <ResizablePanelGroup orientation="vertical" className="h-full">
      {/* Top: form + preview */}
      <ResizablePanel defaultSize={75} minSize={40}>
        <div className="flex h-full">
          {/* Form — 2/3 */}
          <div className="flex w-2/3 flex-col border-r">
            <ScrollArea className="h-full">
              <div className="px-4 py-3">
                {/* Order matches preview: name, appearance, manufacturer, mfr data, description */}
                <FormField
                  label="Advertised Name"
                  htmlFor="advertised-name"
                  empty={!identity.advertisedName}
                  onMouseEnter={() => setHoveredField("advertisedName")}
                >
                  <Input
                    id="advertised-name"
                    value={identity.advertisedName}
                    onChange={(e) => update("advertisedName", e.target.value)}
                    placeholder="e.g. Mu-so 2 Kitchen"
                  />
                </FormField>

                <FormField
                  label="Appearance"
                  htmlFor="appearance"
                  empty={!identity.appearanceCode}
                  onMouseEnter={() => setHoveredField("appearanceCode")}
                >
                  <Select
                    value={identity.appearanceCode}
                    onValueChange={(v) => update("appearanceCode", v)}
                  >
                    <SelectTrigger id="appearance" className="w-full">
                      <SelectValue placeholder="Select appearance..." />
                    </SelectTrigger>
                    <SelectContent>
                      {APPEARANCE_OPTIONS.map((opt) => (
                        <SelectItem key={opt.code} value={opt.code}>
                          <span>{opt.label}</span>
                          <span className="ml-2 font-mono text-muted-foreground">
                            {opt.code}
                          </span>
                        </SelectItem>
                      ))}
                    </SelectContent>
                  </Select>
                </FormField>

                <FormField
                  label="Manufacturer"
                  htmlFor="manufacturer"
                  empty={!identity.manufacturerId}
                  onMouseEnter={() => setHoveredField("manufacturerId")}
                >
                  <Select
                    value={identity.manufacturerId}
                    onValueChange={(v) => update("manufacturerId", v)}
                  >
                    <SelectTrigger id="manufacturer" className="w-full">
                      <SelectValue placeholder="Select manufacturer..." />
                    </SelectTrigger>
                    <SelectContent>
                      {MANUFACTURER_OPTIONS.map((opt) => (
                        <SelectItem key={opt.id} value={opt.id}>
                          <span>{opt.name}</span>
                          <span className="ml-2 font-mono text-muted-foreground">
                            {opt.id}
                          </span>
                        </SelectItem>
                      ))}
                    </SelectContent>
                  </Select>
                </FormField>

                <FormField
                  label="Manufacturer Data"
                  htmlFor="mfr-data"
                  empty={!identity.manufacturerData}
                  onMouseEnter={() => setHoveredField("manufacturerData")}
                >
                  <DataInput
                    id="mfr-data"
                    value={identity.manufacturerData}
                    onChange={(v) => update("manufacturerData", v)}
                  />
                </FormField>

                <FormField
                  label="Description"
                  htmlFor="description"
                  empty={!identity.description}
                  className="items-start [&>label]:pt-2"
                  onMouseEnter={() => setHoveredField("description")}
                >
                  <Textarea
                    id="description"
                    value={identity.description}
                    onChange={(e) => update("description", e.target.value)}
                    placeholder="e.g. Kitchen speaker test config for Auracast certification"
                    rows={6}
                  />
                </FormField>
              </div>
            </ScrollArea>
          </div>

          {/* Preview — 1/3 */}
          <div className="flex w-1/3 flex-col">
            <ScrollArea className="h-full">
              <div className="flex flex-col items-center px-4 py-8">
                {/* Appearance icon */}
                <div className="flex size-20 items-center justify-center rounded-2xl bg-secondary">
                  <AppearanceIcon size={40} className="text-muted-foreground" />
                </div>

                {/* Device name */}
                <p className="mt-4 text-center text-base font-medium">
                  {identity.advertisedName || (
                    <span className="text-muted-foreground/40">Untitled</span>
                  )}
                </p>

                {/* Appearance label */}
                <p className="mt-0.5 text-center text-xs text-muted-foreground">
                  {appearanceLabel}
                </p>

                {/* Manufacturer badge */}
                {manufacturerName && (
                  <Badge variant="outline" className="mt-3 text-[11px]">
                    {manufacturerName}
                  </Badge>
                )}

                {/* Manufacturer data hex dump */}
                {identity.manufacturerData && (
                  <>
                    <Separator className="my-4 w-full" />
                    <div className="w-full">
                      <p className="mb-1.5 text-[10px] font-medium uppercase tracking-wider text-muted-foreground">
                        Manufacturer Data
                      </p>
                      <HexDump data={identity.manufacturerData} bytesPerLine={8} />
                    </div>
                  </>
                )}
              </div>
            </ScrollArea>
          </div>
        </div>
      </ResizablePanel>

      {/* Resize handle */}
      <ResizableHandle withHandle />

      {/* Info panel with tabs */}
      <ResizablePanel defaultSize={25} minSize={10}>
        <Tabs defaultValue="help" className="flex h-full flex-col gap-0">
          <TabsList className="shrink-0 px-1">
            <TabsTrigger value="help" className="gap-1 text-[11px]">
              <Info size={12} />
              Help
            </TabsTrigger>
            <TabsTrigger
              value="warnings"
              className={cn(
                "gap-1 text-[11px]",
                warnings.length > 0 && "!text-yellow-500"
              )}
            >
              {warnings.length > 0 ? (
                <Badge className="size-4 justify-center rounded-full bg-yellow-500 p-0 text-[10px] text-black">
                  {warnings.length}
                </Badge>
              ) : (
                <TriangleAlert size={12} />
              )}
              Warnings
            </TabsTrigger>
            <TabsTrigger
              value="errors"
              className={cn(
                "gap-1 text-[11px]",
                errors.length > 0 && "!text-destructive"
              )}
            >
              {errors.length > 0 ? (
                <Badge className="size-4 justify-center rounded-full bg-destructive p-0 text-[10px] text-white">
                  {errors.length}
                </Badge>
              ) : (
                <CircleX size={12} />
              )}
              Errors
            </TabsTrigger>
          </TabsList>

          <TabsContent value="help" className="min-h-0 flex-1 overflow-hidden">
            <ScrollArea className="h-full">
              <div className="px-4 py-3">
                {help ? (
                  <div className="space-y-2">
                    <div>
                      <p className="text-xs font-medium">{help.title}</p>
                      <p className="text-[11px] text-muted-foreground/60">
                        {help.hint}
                      </p>
                    </div>
                    <p className="text-[11px] leading-relaxed text-muted-foreground">
                      {help.detail}
                    </p>
                    {help.example && (
                      <p className="text-[11px] leading-relaxed text-muted-foreground/70">
                        <span className="font-medium text-muted-foreground">e.g. </span>
                        {help.example}
                      </p>
                    )}
                  </div>
                ) : (
                  <p className="text-[11px] text-muted-foreground/50">
                    Hover over a field to see details
                  </p>
                )}
              </div>
            </ScrollArea>
          </TabsContent>

          <TabsContent value="warnings" className="min-h-0 flex-1 overflow-hidden">
            <ScrollArea className="h-full">
              <div className="px-4 py-3">
                {warnings.length === 0 ? (
                  <p className="text-[11px] text-muted-foreground/50">
                    No warnings
                  </p>
                ) : (
                  <ul className="space-y-2">
                    {warnings.map((d, i) => (
                      <li key={i} className="flex items-start gap-2">
                        <TriangleAlert size={14} className="mt-px shrink-0 text-yellow-500" />
                        <span className="text-[11px] leading-relaxed text-muted-foreground">
                          {d.message}
                        </span>
                      </li>
                    ))}
                  </ul>
                )}
              </div>
            </ScrollArea>
          </TabsContent>

          <TabsContent value="errors" className="min-h-0 flex-1 overflow-hidden">
            <ScrollArea className="h-full">
              <div className="px-4 py-3">
                {errors.length === 0 ? (
                  <p className="text-[11px] text-muted-foreground/50">
                    No errors
                  </p>
                ) : (
                  <ul className="space-y-2">
                    {errors.map((d, i) => (
                      <li key={i} className="flex items-start gap-2">
                        <CircleX size={14} className="mt-px shrink-0 text-destructive" />
                        <span className="text-[11px] leading-relaxed text-muted-foreground">
                          {d.message}
                        </span>
                      </li>
                    ))}
                  </ul>
                )}
              </div>
            </ScrollArea>
          </TabsContent>
        </Tabs>
      </ResizablePanel>
    </ResizablePanelGroup>
  );
}
