---
name: auracle-frontend
description: "React/TypeScript frontend development for Auracle. Use this skill whenever writing, modifying, or reviewing TypeScript or React code in the frontend/src/ directory. Covers component patterns, hook architecture, Tauri invoke wrappers, shadcn/ui usage, design system tokens, routing, and layout. Trigger on any frontend work: new pages, new components, new hooks, styling changes, form wiring, or event handling."
---

# Auracle React Frontend

This skill encodes the patterns and constraints for all frontend work in Auracle. The specs in `docs/frontend.md`, `docs/design-system.md`, and `docs/data-models.md` are the source of truth — read them before making changes if you haven't already this session.

The frontend lives in `frontend/` at the repo root. The Tauri Rust shell lives in `frontend/src-tauri/`. All paths below are relative to `frontend/`.

## Architecture Rules

The frontend follows a strict layered architecture. Each layer has one job:

```
Pages (composition) → Hooks (state + data) → Tauri Wrappers (IPC) → Backend
         ↓
  Components (UI primitives from shadcn/ui)
         ↓
  Feature components (e.g., devices/)
```

- **Pages** compose components and consume hooks. They contain layout and conditional rendering but no data fetching or business logic.
- **Hooks** own state and data fetching. One hook per feature area. They call Tauri wrappers and manage loading/error states. No JSX.
- **Tauri wrappers** (`src/lib/tauri.ts`) are typed `invoke()` calls. One function per backend command. No state, no side effects.
- **UI components** (`src/components/ui/`) are shadcn/ui primitives. Don't modify these unless adding a new variant.
- **Feature components** (`src/components/devices/`, etc.) are domain-specific components used by pages. They receive data via props from hooks.

## Directory Structure

```
frontend/
├── src/
│   ├── components/
│   │   ├── devices/         # BLE device display: DeviceCard, SignalBars
│   │   ├── forms/           # Shared form primitives: FormField, DataInput, HexDump
│   │   ├── generators/      # Broadcast generator UI: IdentityTab, types
│   │   ├── layout/          # App shell: Sidebar, Header, StatusBar, ActivityRail
│   │   └── ui/              # shadcn/ui primitives (don't modify without reason)
│   ├── data/
│   │   └── ble-company-ids.json  # Bluetooth SIG company ID lookup (3,972 entries)
│   ├── hooks/               # One file per feature area
│   ├── lib/
│   │   ├── ble-utils.ts     # BLE name resolution (company IDs, service UUIDs)
│   │   ├── icons.ts         # ICON_SIZE constants — standard icon sizes for the app
│   │   ├── tauri.ts         # Typed invoke wrappers + shared interfaces
│   │   └── utils.ts         # cn() helper
│   ├── pages/               # One file per route
│   ├── App.tsx              # Root: routing, layout, hook initialisation
│   ├── index.css            # Design system tokens
│   └── main.tsx             # Entry point
├── src-tauri/               # Tauri Rust shell
│   ├── tauri.conf.json      # Tauri build/app configuration
│   ├── Cargo.toml           # Rust dependencies
│   ├── src/                 # Rust backend code
│   └── capabilities/        # Tauri permissions
└── components.json          # shadcn/ui config
```

## TypeScript Rules

Strict mode is mandatory. These are non-negotiable:
- No `any` types, ever. Use `unknown` and narrow, or define a proper interface.
- All Tauri IPC types have matching interfaces in `frontend/src/lib/tauri.ts` that mirror the Rust structs exactly.
- Tauri auto-camelCases Rust's snake_case fields, so `device_type` in Rust becomes `deviceType` in TypeScript. However, interface field names in `tauri.ts` use snake_case to match the Rust structs as documented in `docs/data-models.md`. Check the existing patterns.

## Adding a Tauri Invoke Wrapper

When a new backend command is added, create its frontend counterpart in `frontend/src/lib/tauri.ts`:

```typescript
import { invoke } from "@tauri-apps/api/core";

// If new types are needed, add the interface here
export interface BluetoothAdapter {
  name: string;
  is_available: boolean;
  address: string;
}

// Wrapper function — one per command
export async function getBluetoothAdapter(): Promise<BluetoothAdapter> {
  return invoke<BluetoothAdapter>("get_bluetooth_adapter");
}

// With parameters
export async function connectDevice(deviceId: string): Promise<void> {
  return invoke<void>("connect_device", { deviceId });
}
```

Keep wrappers minimal — no error handling, no state, no retries. Those belong in hooks.

## Adding a Hook

Hooks follow a consistent pattern:

```typescript
// frontend/src/hooks/useMyFeature.ts
import { useState, useCallback } from "react";
import { myCommand, MyType } from "@/lib/tauri";

interface MyFeatureState {
  data: MyType[];
  loading: boolean;
  error: string | null;
}

export function useMyFeature() {
  const [state, setState] = useState<MyFeatureState>({
    data: [],
    loading: false,
    error: null,
  });

  const fetchData = useCallback(async () => {
    setState(prev => ({ ...prev, loading: true, error: null }));
    try {
      const data = await myCommand();
      setState({ data, loading: false, error: null });
    } catch (err) {
      setState(prev => ({
        ...prev,
        loading: false,
        error: err instanceof Error ? err.message : String(err),
      }));
    }
  }, []);

  return { ...state, fetchData };
}
```

Hooks return a spread of their state plus action functions. They handle all error catching — pages should never need try/catch.

## Adding a Page

1. Create `frontend/src/pages/MyPage.tsx`:

```typescript
import { Header } from "@/components/layout/Header";

export function MyPage() {
  return (
    <>
      <Header title="My Page" description="Optional subtitle" />
      <div className="p-6">
        {/* page content */}
      </div>
    </>
  );
}
```

2. Add the route in `frontend/src/App.tsx` inside `<Routes>`:

```tsx
<Route path="/my-page" element={<MyPage />} />
```

3. Add a nav item in `frontend/src/components/layout/Sidebar.tsx`:

```typescript
const navItems = [
  // ... existing items
  { path: "/my-page", icon: MyIcon, label: "My Page" },
];
```

## Design System — No Local Overrides

The theme is the single source of truth. Every colour, text size, spacing value, and icon size must come from the design system — either a CSS custom property consumed via Tailwind, a standard Tailwind utility class, or a shared constant. **Local style hacks are forbidden.** Specifically:

- **No hardcoded colours.** Never use raw hex values (`#e5484d`), Tailwind palette colours (`red-500`, `yellow-400`, `green-500`), or `text-black`/`text-white` in components. Use semantic tokens: `text-destructive`, `bg-warning`, `text-success`, `text-foreground`, etc. If a semantic token doesn't exist for your use case, add one to the theme in `index.css` — don't work around it.
- **No arbitrary pixel sizes.** Never use Tailwind's bracket syntax for text (`text-[11px]`, `text-[13px]`) or spacing/sizing (`w-[72px]`, `w-[35%]`). Use the standard Tailwind scale: `text-xs`, `text-sm`, `w-20`, `w-1/3`, etc. If the standard sizes look wrong at the current zoom level, the fix is adjusting the global zoom — not poking pixel values into individual controls.
- **No magic numbers for icon sizes.** Never pass raw numbers to icon `size` props. Import and use the `ICON_SIZE` constants from `@/lib/icons`.
- **No child selector hacks.** Don't use arbitrary Tailwind child selectors like `[&>label]:pt-2` to patch alignment. If a component needs a layout variant, add a proper prop (e.g. `multiline`) that handles it internally.
- **No `!important` overrides** unless structurally required by a third-party component (e.g. sidebar collapse animation). Document why with a comment.

If you find yourself reaching for a local override, stop and ask: should this be a theme token, a component prop, or a shared constant?

### Colour Tokens

All colours are CSS custom properties defined in `frontend/src/index.css` and registered in the `@theme inline` block so Tailwind generates utility classes. Key tokens:

| Tailwind class | Purpose |
|---------------|---------|
| `bg-background` | Page background |
| `bg-card` / `bg-surface` | Card/panel background |
| `bg-sidebar` | Sidebar background |
| `text-foreground` | Primary text |
| `text-muted-foreground` | Secondary/dimmed text |
| `border-border` | Default borders |
| `bg-primary` / `text-primary` | Accent/active |
| `bg-destructive` / `text-destructive` | Errors, critical states |
| `bg-warning` / `text-warning` | Warnings, caution states |
| `bg-success` / `text-success` | Healthy, connected, good RSSI |

Each semantic colour has a corresponding `-foreground` variant for text on that background (e.g. `text-destructive-foreground` on `bg-destructive`). See `index.css` for the full set.

### Typography

Two font families, used intentionally:
- **Geist Sans** (`font-sans`): All UI text — labels, titles, descriptions, buttons
- **Geist Mono** (`font-mono`): Technical data — log entries, device IDs, MAC addresses, versions, the "Auracle" brand

Text sizes use the **standard Tailwind type scale only** — no arbitrary pixel values:

| Class | Px | Use |
|-------|-----|-----|
| `text-xs` | 12 | Secondary UI: status bar, badges, log entries, table cells, muted labels |
| `text-sm` | 14 | Primary UI: form labels, nav items, descriptions, page titles |
| `text-base` | 16 | Emphasis: hero headings (rare) |

The app targets a dense, professional feel. If text appears too large or too small, adjust the global zoom level — never poke pixel values into individual components.

### Icons

Lucide React exclusively. Always use the shared `ICON_SIZE` constants — never pass raw numbers:

```tsx
import { Radio } from "lucide-react";
import { ICON_SIZE } from "@/lib/icons";

<Radio size={ICON_SIZE.md} />
```

| Constant | Px | Use |
|----------|-----|-----|
| `ICON_SIZE.xs` | 12 | Status bar indicators, inline tab icons |
| `ICON_SIZE.sm` | 14 | Secondary UI — badges, form labels, tab triggers |
| `ICON_SIZE.md` | 16 | Primary UI — navigation, buttons, toolbar |
| `ICON_SIZE.lg` | 20 | Emphasis — section headers, larger buttons |
| `ICON_SIZE.xl` | 24 | Standalone — dialog icons, empty states |
| `ICON_SIZE.hero` | 40 | Large decorative icons, preview panels |

### Layout Sizing

Use standard Tailwind spacing and fraction utilities for layout:
- Widths: `w-20`, `w-1/3`, `w-2/3`, `flex-1` — never `w-[72px]` or `w-[35%]`
- Gaps/padding: `gap-2`, `px-4`, `py-3` — use the 4px scale
- Flex ratios for proportional columns, not hardcoded percentages

### Panel Dividers

**Borders and separators imply the user can resize.** Only use a visible divider (border, separator, or `ResizableHandle`) between panels that are genuinely resizable. For fixed (non-resizable) panel splits, distinguish the subordinate panel with an alternative background instead — typically `bg-card` against the default `bg-background`. This is the same pattern used by the sidebar (`bg-sidebar`) and the preview pane (`bg-card`).

## shadcn/ui Components

Available components (all in `frontend/src/components/ui/`):

Badge, Button, Card, Collapsible, Input, Label, Resizable, ScrollArea, Select, Separator, Sheet, Sidebar, Skeleton, Tabs, Textarea, Toggle, ToggleGroup, Tooltip

To add a new shadcn component, use the CLI from the `frontend/` directory:
```bash
cd frontend && npx shadcn@latest add [component-name]
```

The config is in `frontend/components.json` — new-york style, neutral base, CSS variables enabled.

### Button sizes for reference

| Size | Use case |
|------|----------|
| `xs` (h-6) | Tight toolbars, inline actions |
| `sm` (h-8) | Secondary actions |
| `default` (h-9) | Primary actions |
| `icon-xs` / `icon-sm` / `icon` | Icon-only buttons at various densities |

### Badge variants

| Variant | Use case |
|---------|----------|
| `info` | Informational log entries |
| `warn` | Warning states |
| `error` | Error states |
| `outline` | Neutral tags |

## Listening to Backend Events

For real-time updates from the Rust backend (as opposed to polling), use Tauri's event system:

```typescript
import { listen } from "@tauri-apps/api/event";

useEffect(() => {
  const unlisten = listen<MyPayload>("event-name", (event) => {
    setState(prev => ({ ...prev, data: event.payload }));
  });
  return () => { unlisten.then(fn => fn()); };
}, []);
```

## Live-Updating Lists

When displaying lists that update in real-time (e.g., BLE devices during a scan), sort order must be **stable**. If items are sorted by a value that changes frequently (like signal strength), the list will constantly reorder and become unusable — users can't click on a device that's jumping around.

The pattern: sort by a stable primary key (discovery order or alphabetical by ID), and use the volatile value (RSSI, last seen) only as a secondary sort or as a visual indicator within the row. If you want to offer "sort by signal strength", make it an explicit user-initiated sort option, not the default.

```typescript
// Bad: RSSI changes every update, list jumps around
const sorted = [...devices].sort((a, b) => b.rssi - a.rssi);

// Good: stable primary sort, RSSI is secondary
const sorted = [...devices].sort((a, b) => {
  const nameA = a.name === "Unknown" ? "\uffff" : a.name;
  const nameB = b.name === "Unknown" ? "\uffff" : b.name;
  const nameCmp = nameA.localeCompare(nameB);
  return nameCmp !== 0 ? nameCmp : b.rssi - a.rssi;
});
```

This principle applies to any list backed by real-time data — device lists, log entries, stream status.

## Empty and Error States

Every page and data display must have purposeful empty and error states — no placeholder text, no lorem ipsum. Empty states should explain what the user should do next. Error states should display the error message from the backend and suggest a remediation.

```tsx
{devices.length === 0 ? (
  <Card className="p-6">
    <p className="text-sm text-muted-foreground">
      No devices found. Click Scan to search for nearby devices.
    </p>
  </Card>
) : (
  // render device list
)}
```

## Build Commands

```bash
npm run --prefix frontend dev    # Vite dev server on port 1420
npm run --prefix frontend build  # TypeScript check + Vite production build
cd frontend && cargo tauri dev   # Full Tauri dev mode (frontend + backend)
cd frontend && cargo tauri build # Production Tauri build
```

## Reference

For the full component variant tables, see `docs/design-system.md`.
For routing and page prop contracts, see `docs/frontend.md`.
For type definitions, see `docs/data-models.md`.
