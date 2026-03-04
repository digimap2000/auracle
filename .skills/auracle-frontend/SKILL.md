---
name: auracle-frontend
description: "React/TypeScript frontend development for Auracle. Use this skill whenever writing, modifying, or reviewing TypeScript or React code in the src/ directory. Covers component patterns, hook architecture, Tauri invoke wrappers, shadcn/ui usage, design system tokens, routing, and layout. Trigger on any frontend work: new pages, new components, new hooks, styling changes, form wiring, or event handling."
---

# Auracle React Frontend

This skill encodes the patterns and constraints for all frontend work in Auracle. The specs in `docs/frontend.md`, `docs/design-system.md`, and `docs/data-models.md` are the source of truth — read them before making changes if you haven't already this session.

## Architecture Rules

The frontend follows a strict layered architecture. Each layer has one job:

```
Pages (composition) → Hooks (state + data) → Tauri Wrappers (IPC) → Backend
         ↓
  Components (UI primitives from shadcn/ui)
```

- **Pages** compose components and consume hooks. They contain layout and conditional rendering but no data fetching or business logic.
- **Hooks** own state and data fetching. One hook per feature area. They call Tauri wrappers and manage loading/error states. No JSX.
- **Tauri wrappers** (`src/lib/tauri.ts`) are typed `invoke()` calls. One function per backend command. No state, no side effects.
- **UI components** (`src/components/ui/`) are shadcn/ui primitives. Don't modify these unless adding a new variant.

## Directory Structure

```
src/
├── components/
│   ├── layout/          # App shell: Sidebar, Header, StatusBar
│   └── ui/              # shadcn/ui primitives (don't modify without reason)
├── hooks/               # One file per feature area
├── lib/
│   ├── tauri.ts         # Typed invoke wrappers + shared interfaces
│   └── utils.ts         # cn() helper
├── pages/               # One file per route
├── App.tsx              # Root: routing, layout, hook initialisation
├── index.css            # Design system tokens
└── main.tsx             # Entry point
```

## TypeScript Rules

Strict mode is mandatory. These are non-negotiable:
- No `any` types, ever. Use `unknown` and narrow, or define a proper interface.
- All Tauri IPC types have matching interfaces in `src/lib/tauri.ts` that mirror the Rust structs exactly.
- Tauri auto-camelCases Rust's snake_case fields, so `device_type` in Rust becomes `deviceType` in TypeScript. However, interface field names in `tauri.ts` use snake_case to match the Rust structs as documented in `docs/data-models.md`. Check the existing patterns.

## Adding a Tauri Invoke Wrapper

When a new backend command is added, create its frontend counterpart in `src/lib/tauri.ts`:

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
// src/hooks/useMyFeature.ts
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

1. Create `src/pages/MyPage.tsx`:

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

2. Add the route in `src/App.tsx` inside `<Routes>`:

```tsx
<Route path="/my-page" element={<MyPage />} />
```

3. Add a nav item in `src/components/layout/Sidebar.tsx`:

```typescript
const navItems = [
  // ... existing items
  { path: "/my-page", icon: MyIcon, label: "My Page" },
];
```

## Design System

The app is dark-mode only. All colours are CSS custom properties consumed through Tailwind. Never use raw hex values in components — always use the token names.

Key tokens (see `docs/design-system.md` for the full set):

| Tailwind class | Purpose |
|---------------|---------|
| `bg-background` | Page background (#0a0a0a) |
| `bg-card` / `bg-surface` | Card/panel background (#111111) |
| `text-foreground` | Primary text (#ededed) |
| `text-muted-foreground` | Secondary text (#a1a1a1) |
| `border-border` | Default borders (#1f1f1f) |
| `bg-primary` | Accent/active (#0070f3) |
| `bg-destructive` | Errors (#e5484d) |

### Typography

Two font families, used intentionally:
- **Geist Sans** (`font-sans`): All UI text — labels, titles, descriptions, buttons
- **Geist Mono** (`font-mono`): Technical data — log entries, device IDs, MAC addresses, versions, the "Auracle" brand

Text sizes are deliberately small for a dense, professional feel:
- Page titles: `text-sm font-medium`
- Descriptions: `text-[11px] text-muted-foreground`
- Status bar: `text-[11px]`, version `text-[10px] font-mono`
- Log entries: `text-[11px] font-mono`
- Badges: `text-[10px] font-medium`

### Icons

Lucide React exclusively. Use 14px or 16px consistently — never mix sizes in the same visual context.

```tsx
import { Radio } from "lucide-react";
<Radio className="h-4 w-4" />
```

## shadcn/ui Components

Available components (all in `src/components/ui/`):

Badge, Button, Card, Input, ScrollArea, Separator, Sheet, Sidebar, Skeleton, Tooltip

To add a new shadcn component, use the CLI:
```bash
npx shadcn@latest add [component-name]
```

The config is in `components.json` — new-york style, neutral base, CSS variables enabled.

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

## Reference

For the full component variant tables, see `docs/design-system.md`.
For routing and page prop contracts, see `docs/frontend.md`.
For type definitions, see `docs/data-models.md`.
