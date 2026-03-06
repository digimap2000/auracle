import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Separator } from "@/components/ui/separator";
import { ScrollArea } from "@/components/ui/scroll-area";
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/select";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import {
  Info,
  TriangleAlert,
  CircleX,
  Check,
  Bluetooth,
  Radio,
} from "lucide-react";

// ── Colour swatch ───────────────────────────────────────────────────

interface SwatchProps {
  name: string;
  variable: string;
  className: string;
  foregroundClass?: string;
}

function Swatch({ name, variable, className, foregroundClass }: SwatchProps) {
  return (
    <div className="flex items-center gap-3">
      <div
        className={`size-10 shrink-0 rounded-lg border ${className}`}
      />
      <div className="min-w-0">
        <p className={`text-xs font-medium ${foregroundClass ?? "text-foreground"}`}>
          {name}
        </p>
        <p className="font-mono text-xs text-muted-foreground">{variable}</p>
      </div>
    </div>
  );
}

// ── Spacing scale ───────────────────────────────────────────────────

const SPACING = [0.5, 1, 1.5, 2, 3, 4, 5, 6, 8, 10, 12, 16] as const;

function SpacingScale() {
  return (
    <div className="flex flex-col gap-1.5">
      {SPACING.map((s) => (
        <div key={s} className="flex items-center gap-3">
          <span className="w-8 text-right font-mono text-xs text-muted-foreground">
            {s}
          </span>
          <div
            className="h-3 rounded-sm bg-primary"
            style={{ width: `${s * 0.25}rem` }}
          />
          <span className="font-mono text-xs text-muted-foreground">
            {s * 0.25}rem
          </span>
        </div>
      ))}
    </div>
  );
}

// ── Radius scale ────────────────────────────────────────────────────

function RadiusScale() {
  return (
    <div className="flex flex-wrap gap-3">
      {(
        [
          ["sm", "rounded-sm"],
          ["md", "rounded-md"],
          ["lg", "rounded-lg"],
          ["xl", "rounded-xl"],
          ["2xl", "rounded-2xl"],
          ["full", "rounded-full"],
        ] as const
      ).map(([label, cls]) => (
        <div key={label} className="flex flex-col items-center gap-1">
          <div
            className={`size-12 border-2 border-primary bg-secondary ${cls}`}
          />
          <span className="font-mono text-xs text-muted-foreground">
            {label}
          </span>
        </div>
      ))}
    </div>
  );
}

// ── Typography samples ──────────────────────────────────────────────

function TypographySamples() {
  return (
    <div className="space-y-3">
      <p className="text-xs text-muted-foreground">
        Root font size: 14px — all rem values resolve against this base.
      </p>
      {(
        [
          ["text-2xl font-bold", "2xl bold — Page heading", "1.5rem"],
          ["text-xl font-semibold", "xl semibold — Section heading", "1.25rem"],
          ["text-lg font-medium", "lg medium — Sub-heading", "1.125rem"],
          ["text-base", "base — Body text", "1rem"],
          ["text-sm", "sm — Secondary text", "0.875rem"],
          ["text-xs", "xs — Captions & labels", "0.75rem"],
        ] as const
      ).map(([cls, label, rem]) => (
        <div key={cls} className="flex items-baseline gap-3">
          <p className={`font-sans ${cls}`}>{label}</p>
          <span className="font-mono text-xs text-muted-foreground/50">{rem}</span>
        </div>
      ))}
      <Separator />
      <p className="text-xs text-muted-foreground">Monospace (Geist Mono)</p>
      {(
        [
          ["text-sm font-mono", "sm mono — Code / data", "0.875rem"],
          ["text-xs font-mono", "xs mono — IDs, hex, byte counts", "0.75rem"],
        ] as const
      ).map(([cls, label, rem]) => (
        <div key={cls} className="flex items-baseline gap-3">
          <p className={cls}>{label}</p>
          <span className="font-mono text-xs text-muted-foreground/50">{rem}</span>
        </div>
      ))}
    </div>
  );
}

// ── Page ─────────────────────────────────────────────────────────────

export function ThemeReference() {
  return (
    <div className="flex h-full flex-col overflow-hidden">
      <div className="px-6 pt-6 pb-2">
        <h1 className="text-xl font-semibold">Theme Reference</h1>
        <p className="mt-1 text-sm text-muted-foreground">
          Design tokens, colour palette, spacing, typography, and component
          variants for the current theme.
        </p>
      </div>

      <Tabs defaultValue="colours" className="flex min-h-0 flex-1 flex-col">
        <div className="px-6">
          <TabsList variant="line">
            <TabsTrigger value="colours">Colours</TabsTrigger>
            <TabsTrigger value="typography">Typography</TabsTrigger>
            <TabsTrigger value="spacing">Spacing</TabsTrigger>
            <TabsTrigger value="components">Components</TabsTrigger>
            <TabsTrigger value="icons">Icons</TabsTrigger>
            <TabsTrigger value="semantic">Semantic</TabsTrigger>
            <TabsTrigger value="variables">Variables</TabsTrigger>
          </TabsList>
        </div>

        <ScrollArea className="min-h-0 flex-1">
          <div className="max-w-4xl p-6">
            {/* ── Colours ──────────────────────────────────────── */}
            <TabsContent value="colours" className="mt-0 space-y-6">
              <div>
                <h3 className="mb-2 text-sm font-medium text-muted-foreground">
                  Core
                </h3>
                <div className="grid grid-cols-2 gap-3 sm:grid-cols-3">
                  <Swatch name="Background" variable="--background" className="bg-background" />
                  <Swatch name="Foreground" variable="--foreground" className="bg-foreground" foregroundClass="text-foreground" />
                  <Swatch name="Card" variable="--card" className="bg-card" />
                  <Swatch name="Surface" variable="--surface" className="bg-surface" />
                  <Swatch name="Border" variable="--border" className="bg-border" />
                  <Swatch name="Input" variable="--input" className="bg-input" />
                </div>
              </div>

              <div>
                <h3 className="mb-2 text-sm font-medium text-muted-foreground">
                  Semantic
                </h3>
                <div className="grid grid-cols-2 gap-3 sm:grid-cols-3">
                  <Swatch name="Primary" variable="--primary" className="bg-primary" />
                  <Swatch name="Primary FG" variable="--primary-foreground" className="bg-primary-foreground" />
                  <Swatch name="Secondary" variable="--secondary" className="bg-secondary" />
                  <Swatch name="Secondary FG" variable="--secondary-foreground" className="bg-secondary-foreground" />
                  <Swatch name="Muted" variable="--muted" className="bg-muted" />
                  <Swatch name="Muted FG" variable="--muted-foreground" className="bg-muted-foreground" />
                  <Swatch name="Accent" variable="--accent" className="bg-accent" />
                  <Swatch name="Destructive" variable="--destructive" className="bg-destructive" />
                  <Swatch name="Warning" variable="--warning" className="bg-warning" />
                  <Swatch name="Success" variable="--success" className="bg-success" />
                  <Swatch name="Ring" variable="--ring" className="bg-ring" />
                </div>
              </div>

              <div>
                <h3 className="mb-2 text-sm font-medium text-muted-foreground">
                  Sidebar
                </h3>
                <div className="grid grid-cols-2 gap-3 sm:grid-cols-3">
                  <Swatch name="Sidebar" variable="--sidebar" className="bg-sidebar" />
                  <Swatch name="Sidebar FG" variable="--sidebar-foreground" className="bg-sidebar-foreground" />
                  <Swatch name="Sidebar Primary" variable="--sidebar-primary" className="bg-sidebar-primary" />
                  <Swatch name="Sidebar Accent" variable="--sidebar-accent" className="bg-sidebar-accent" />
                  <Swatch name="Sidebar Border" variable="--sidebar-border" className="bg-sidebar-border" />
                </div>
              </div>
            </TabsContent>

            {/* ── Typography ────────────────────────────────────── */}
            <TabsContent value="typography" className="mt-0">
              <TypographySamples />
            </TabsContent>

            {/* ── Spacing & Radius ──────────────────────────────── */}
            <TabsContent value="spacing" className="mt-0 space-y-8">
              <div className="space-y-4">
                <h3 className="text-sm font-medium text-muted-foreground">
                  Spacing Scale
                </h3>
                <p className="text-xs text-muted-foreground">
                  Tailwind spacing units (1 unit = 0.25rem = 3.5px at 14px root)
                </p>
                <SpacingScale />
              </div>

              <Separator />

              <div className="space-y-4">
                <h3 className="text-sm font-medium text-muted-foreground">
                  Border Radius
                </h3>
                <RadiusScale />
              </div>
            </TabsContent>

            {/* ── Components ────────────────────────────────────── */}
            <TabsContent value="components" className="mt-0 space-y-8">
              {/* Buttons */}
              <div className="space-y-3">
                <h3 className="text-sm font-medium text-muted-foreground">
                  Buttons
                </h3>
                <div>
                  <p className="mb-2 text-xs text-muted-foreground">Variants</p>
                  <div className="flex flex-wrap items-center gap-2">
                    <Button>Default</Button>
                    <Button variant="secondary">Secondary</Button>
                    <Button variant="outline">Outline</Button>
                    <Button variant="ghost">Ghost</Button>
                    <Button variant="destructive">Destructive</Button>
                    <Button variant="link">Link</Button>
                  </div>
                </div>
                <div>
                  <p className="mb-2 text-xs text-muted-foreground">Sizes</p>
                  <div className="flex flex-wrap items-center gap-2">
                    <Button size="lg">Large</Button>
                    <Button size="default">Default</Button>
                    <Button size="sm">Small</Button>
                    <Button size="xs">XS</Button>
                    <Button size="icon"><Bluetooth size={16} /></Button>
                    <Button size="icon-xs"><Bluetooth size={14} /></Button>
                  </div>
                </div>
                <div>
                  <p className="mb-2 text-xs text-muted-foreground">States</p>
                  <div className="flex flex-wrap items-center gap-2">
                    <Button>Enabled</Button>
                    <Button disabled>Disabled</Button>
                  </div>
                </div>
              </div>

              <Separator />

              {/* Badges */}
              <div className="space-y-3">
                <h3 className="text-sm font-medium text-muted-foreground">
                  Badges
                </h3>
                <div className="flex flex-wrap items-center gap-2">
                  <Badge>Default</Badge>
                  <Badge variant="secondary">Secondary</Badge>
                  <Badge variant="outline">Outline</Badge>
                  <Badge variant="destructive">Destructive</Badge>
                  <Badge className="bg-warning text-warning-foreground">Warning</Badge>
                </div>
              </div>

              <Separator />

              {/* Form Controls */}
              <div className="space-y-3">
                <h3 className="text-sm font-medium text-muted-foreground">
                  Form Controls
                </h3>
                <div className="grid max-w-md grid-cols-1 gap-3">
                  <div>
                    <label className="mb-1 block text-xs text-muted-foreground">
                      Input
                    </label>
                    <Input placeholder="Placeholder text..." />
                  </div>
                  <div>
                    <label className="mb-1 block text-xs text-muted-foreground">
                      Input (disabled)
                    </label>
                    <Input placeholder="Disabled" disabled />
                  </div>
                  <div>
                    <label className="mb-1 block text-xs text-muted-foreground">
                      Select
                    </label>
                    <Select>
                      <SelectTrigger>
                        <SelectValue placeholder="Choose an option..." />
                      </SelectTrigger>
                      <SelectContent>
                        <SelectItem value="a">Option A</SelectItem>
                        <SelectItem value="b">Option B</SelectItem>
                        <SelectItem value="c">Option C</SelectItem>
                      </SelectContent>
                    </Select>
                  </div>
                </div>
              </div>

              <Separator />

              {/* Tabs demo */}
              <div className="space-y-3">
                <h3 className="text-sm font-medium text-muted-foreground">
                  Tabs
                </h3>
                <div className="space-y-4">
                  <div>
                    <p className="mb-2 text-xs text-muted-foreground">
                      Default (pill)
                    </p>
                    <Tabs defaultValue="one">
                      <TabsList>
                        <TabsTrigger value="one">Tab One</TabsTrigger>
                        <TabsTrigger value="two">Tab Two</TabsTrigger>
                        <TabsTrigger value="three">Tab Three</TabsTrigger>
                      </TabsList>
                      <TabsContent value="one">
                        <p className="p-2 text-sm text-muted-foreground">
                          Default tab content
                        </p>
                      </TabsContent>
                    </Tabs>
                  </div>
                  <div>
                    <p className="mb-2 text-xs text-muted-foreground">
                      Line variant
                    </p>
                    <Tabs defaultValue="one">
                      <TabsList variant="line">
                        <TabsTrigger value="one">Tab One</TabsTrigger>
                        <TabsTrigger value="two">Tab Two</TabsTrigger>
                        <TabsTrigger value="three">Tab Three</TabsTrigger>
                      </TabsList>
                      <TabsContent value="one">
                        <p className="p-2 text-sm text-muted-foreground">
                          Line variant tab content
                        </p>
                      </TabsContent>
                    </Tabs>
                  </div>
                </div>
              </div>
            </TabsContent>

            {/* ── Icons ─────────────────────────────────────────── */}
            <TabsContent value="icons" className="mt-0">
              <p className="mb-4 text-xs text-muted-foreground">
                Lucide React — 14px and 16px standard sizes
              </p>
              <div className="flex flex-wrap items-end gap-4">
                {(
                  [
                    ["14px", 14],
                    ["16px", 16],
                    ["20px", 20],
                    ["24px", 24],
                  ] as const
                ).map(([label, size]) => (
                  <div key={label} className="flex flex-col items-center gap-1">
                    <div className="flex items-center gap-2 text-foreground">
                      <Info size={size} />
                      <TriangleAlert size={size} />
                      <CircleX size={size} />
                      <Check size={size} />
                      <Bluetooth size={size} />
                      <Radio size={size} />
                    </div>
                    <span className="font-mono text-xs text-muted-foreground">
                      {label}
                    </span>
                  </div>
                ))}
              </div>
            </TabsContent>

            {/* ── Semantic ──────────────────────────────────────── */}
            <TabsContent value="semantic" className="mt-0 space-y-8">
              <div className="space-y-3">
                <h3 className="text-sm font-medium text-muted-foreground">
                  Colours in Context
                </h3>
                <div className="space-y-2">
                  <div className="flex items-center gap-2 rounded-md bg-secondary p-3">
                    <Info size={14} className="shrink-0 text-primary" />
                    <p className="text-xs">
                      <span className="font-medium text-foreground">Info</span>{" "}
                      <span className="text-muted-foreground">
                        — Primary accent on secondary background
                      </span>
                    </p>
                  </div>
                  <div className="flex items-center gap-2 rounded-md bg-warning/10 p-3">
                    <TriangleAlert
                      size={14}
                      className="shrink-0 text-warning"
                    />
                    <p className="text-xs">
                      <span className="font-medium text-warning">Warning</span>{" "}
                      <span className="text-muted-foreground">
                        — Warning on warning/10 background
                      </span>
                    </p>
                  </div>
                  <div className="flex items-center gap-2 rounded-md bg-destructive/10 p-3">
                    <CircleX size={14} className="shrink-0 text-destructive" />
                    <p className="text-xs">
                      <span className="font-medium text-destructive">Error</span>{" "}
                      <span className="text-muted-foreground">
                        — Destructive on destructive/10 background
                      </span>
                    </p>
                  </div>
                  <div className="flex items-center gap-2 rounded-md bg-success/10 p-3">
                    <Check size={14} className="shrink-0 text-success" />
                    <p className="text-xs">
                      <span className="font-medium text-success">Success</span>{" "}
                      <span className="text-muted-foreground">
                        — Success on success/10 background
                      </span>
                    </p>
                  </div>
                </div>
              </div>

              <Separator />

              <div className="space-y-3">
                <h3 className="text-sm font-medium text-muted-foreground">
                  Text Opacity Levels
                </h3>
                <div className="space-y-1">
                  <p className="text-sm text-foreground">
                    foreground — Primary text
                  </p>
                  <p className="text-sm text-foreground/80">
                    foreground/80 — Secondary emphasis
                  </p>
                  <p className="text-sm text-muted-foreground">
                    muted-foreground — De-emphasised labels
                  </p>
                  <p className="text-sm text-muted-foreground/70">
                    muted-foreground/70 — Hints and examples
                  </p>
                  <p className="text-sm text-muted-foreground/50">
                    muted-foreground/50 — Offsets, byte counts
                  </p>
                  <p className="text-sm text-muted-foreground/40">
                    muted-foreground/40 — Placeholder / empty state
                  </p>
                </div>
              </div>
            </TabsContent>

            {/* ── Variables ─────────────────────────────────────── */}
            <TabsContent value="variables" className="mt-0 space-y-4">
              <p className="text-xs text-muted-foreground">
                Computed values from the current theme
              </p>
              <CssVariablesDump />
            </TabsContent>
          </div>
        </ScrollArea>
      </Tabs>
    </div>
  );
}

// ── Live CSS variable reader ────────────────────────────────────────

const CSS_VARS = [
  "--background",
  "--foreground",
  "--card",
  "--card-foreground",
  "--popover",
  "--popover-foreground",
  "--primary",
  "--primary-foreground",
  "--secondary",
  "--secondary-foreground",
  "--muted",
  "--muted-foreground",
  "--accent",
  "--accent-foreground",
  "--destructive",
  "--destructive-foreground",
  "--warning",
  "--warning-foreground",
  "--success",
  "--success-foreground",
  "--border",
  "--input",
  "--ring",
  "--surface",
  "--radius",
  "--sidebar",
  "--sidebar-foreground",
  "--sidebar-primary",
  "--sidebar-primary-foreground",
  "--sidebar-accent",
  "--sidebar-accent-foreground",
  "--sidebar-border",
  "--sidebar-ring",
] as const;

function CssVariablesDump() {
  const style = getComputedStyle(document.documentElement);

  return (
    <div className="overflow-x-auto rounded-md bg-secondary/50 px-3 py-2">
      <pre className="font-mono text-xs leading-relaxed">
        {CSS_VARS.map((v) => {
          const val = style.getPropertyValue(v).trim() || "(not set)";
          return (
            <div key={v} className="flex">
              <span className="text-muted-foreground/70">{v}:</span>
              <span className="ml-2 text-foreground/80">{val}</span>
            </div>
          );
        })}
      </pre>
    </div>
  );
}
