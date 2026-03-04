# Frontend

## Entry Point

**File**: `src/main.tsx`

Renders the React app into `#root` with:
- `BrowserRouter` (react-router-dom)
- `TooltipProvider` (delayDuration: 200ms)
- Global CSS from `src/index.css`

## Root Component

**File**: `src/App.tsx`

Initialises three hooks at the top level:
- `useBluetoothAdapter()` — Bluetooth adapter detection state
- `useDevices()` — device scanning and connection state
- `useUpdater()` — auto-update state machine

Layout structure:
```
<div flex h-screen w-screen>
  <ActivityRail />
  <div flex-1 flex-col>
    <main>
      <Routes> ... </Routes>
    </main>
    <StatusBar />
  </div>
</div>
```

## Routing

| Path | Component | Props |
|------|-----------|-------|
| `/` | `Home` | `bleDevices: BleDevice[]`, `scanning: boolean`, `onStartScan: () => void`, `onStopScan: () => void` |
| `/browse` | `Devices` | none |
| `/generate` | `StreamConfig` | none |
| `/compliance` | `Compliance` | none |

All routes are defined in `src/App.tsx` using `<Routes>` and `<Route>`.

## Pages

### Home (`src/pages/Home.tsx`)

Live Bluetooth device list. Auto-scans on mount, stops on unmount. No Header or toolbar — just the device card list.

**Auto-scan**: `useEffect` calls `onStartScan()` on mount, `onStopScan()` on unmount. A tick timer updates relative times every second.

**Empty state**: Centred pulsing Bluetooth icon + "Scanning for devices..." text.

**Device list**: `ScrollArea` containing `DeviceCard` components in a `max-w-2xl mx-auto` column with `space-y-3` gaps. Sorted alphabetically by name (case-insensitive), "Unknown" at bottom, RSSI as tiebreaker within Unknown. Sort is stable — list never reorders as RSSI fluctuates.

**Expand/collapse**: `expandedId` state implements accordion behaviour — clicking a card expands it (showing service table, manufacturer data, TX power, peripheral ID), clicking again collapses it. Only one card can be expanded at a time.

**Status**: Live — uses real btleplug data via `ble-devices-updated` events.

### Devices (`src/pages/Devices.tsx`)

Stub page at `/browse`. Shows Header "Browse" with description "Service browser" and centred empty state: Search icon + "Service browser coming soon".

**Status**: Stub — no functionality. Will be rebuilt as a service browser in a future feature.

### StreamConfig (`src/pages/StreamConfig.tsx`)

Audio stream configuration form inside a Card (max-w-lg):
- **Stream Name** — text input, placeholder "My Audio Stream"
- **Codec** — select: LC3, LC3plus, SBC, AAC
- **Sample Rate** — select: 16 kHz, 24 kHz, 32 kHz, 48 kHz
- **Bitrate** — range slider 16–320 kbps, default 96
- **Start Stream** button — disabled

**Status**: UI complete. Form is not connected to state or backend. All inputs are uncontrolled. Start Stream button is permanently disabled.

### Compliance (`src/pages/Compliance.tsx`)

Stub page at `/compliance`. Shows Header "Compliance" with description "LE Audio certification test runner" and centred empty state: ClipboardCheck icon + "Compliance test runner coming soon".

**Status**: Stub — no functionality.

### Logs (`src/pages/Logs.tsx`)

Application event log viewer (no route — will be reintegrated as a panel within activities later):
- **Toolbar** — Clear button (ghost), Export button (ghost, disabled)
- **Log list** — ScrollArea with monospace rows showing timestamp, level badge, message
- **Timestamp format** — `HH:MM:SS.mmm` via `formatTimestamp()` helper

Uses mock data (3 entries). Log levels: `info`, `warn`, `error` rendered as Badge variants.

**Status**: UI complete. Uses hardcoded mock data. No route mapped.

## Layout Components

### ActivityRail (`src/components/layout/ActivityRail.tsx`)

Fixed-width (72px) vertical navigation rail using plain `<nav>` with Tailwind styling. Not collapsible.

Activities:

| Route | Icon | Label |
|-------|------|-------|
| `/` | `Bluetooth` | Home |
| `/browse` | `Search` | Browse |
| `/generate` | `Radio` | Generate |
| `/compliance` | `ClipboardCheck` | Compliance |

Each item is a `NavLink` with `end` prop on `/`. Icon (16px) above label (`text-[10px] font-medium`). Active state: left-2 primary border + `text-foreground`. Inactive: `text-muted-foreground`. Hover: `bg-secondary rounded-md`.

### Header (`src/components/layout/Header.tsx`)

Page header bar (h-12) with bottom border.

Props:
- `title: string` (required) — rendered as `<h1>` in text-sm font-medium
- `description?: string` — rendered as `<p>` in text-[11px] text-muted-foreground

Used by all pages.

### StatusBar (`src/components/layout/StatusBar.tsx`)

Bottom bar (h-8) with top border. Three sections:

**Left**: Bluetooth indicator + connection indicator
- Bluetooth icon (12px): `text-primary` when adapter available, `text-muted-foreground/30` otherwise. Tooltip shows adapter name or error.
- Green pulsing dot when `connectedCount > 0`, muted dot otherwise
- Text: "{n} device(s) connected"

**Right**: Update status + version
- `available` state: Download button with version
- `downloading` state: Spinner with progress percentage
- `ready` state: Restart button
- `error` state: Retry button in destructive color
- Version label: `v0.1.0` in monospace

Props: `connectedCount: number`, `updater: ReturnType<typeof useUpdater>`, `bluetoothAdapter: { adapter, loading, error }`

## Device Components

### DeviceCard (`src/components/devices/DeviceCard.tsx`)

Clickable card for a single BLE device with dual-state header and expand/collapse detail.

Props: `device: BleDevice`, `expanded: boolean`, `onToggle: () => void`.

**Collapsed header**:
- **Line 1**: SignalBars + device name (left), up to 2 service badges with overflow count (right). Badge priority: Auracast (default) > Audio Stream > BA Scan > PAC > BAP > alphabetical. Overflow shown as "+N more" secondary badge.
- **Line 2**: manufacturer company name only. Omitted if no manufacturer data.

**Expanded header** (badges replaced):
- **Line 1**: SignalBars + device name (left), device ID in monospace + RSSI in coloured monospace (right).
- **Line 2**: manufacturer company name + hex company ID. Omitted if no manufacturer data.

**Expanded detail** (smooth CSS grid height animation, below header):
- Services table only: Service name, UUID (monospace), Type badge (Standard/Custom via `isStandardUuid`). No section heading — table column header "Service" is self-explanatory. No manufacturer data, TX power, or peripheral ID sections (those are in the header).

Outer: `rounded-lg border bg-card p-3 cursor-pointer`. Hover: `border-muted-foreground/30`.

### SignalBars (`src/components/devices/SignalBars.tsx`)

4-bar signal strength indicator. Bar heights: 4px, 8px, 12px, 16px. Color and active bar count based on RSSI thresholds (green > -50, green > -60, yellow > -70, red otherwise).

## Shared Utilities

### ble-utils (`src/lib/ble-utils.ts`)

Shared BLE utility functions and lookup maps:
- `KNOWN_SERVICES` — map of UUID → service name (including Focal Naim Auracast)
- `resolveServiceName(uuid)` — resolve UUID to name or return UUID as-is
- `resolveCompanyName(id)` — resolve company ID from SIG database (`src/data/ble-company-ids.json`)
- `formatHexBytes(bytes)` — format byte array as hex string
- `rssiColor(rssi)` — return Tailwind color class for RSSI value
- `relativeTime(isoString)` — format ISO timestamp as "now", "Xs ago", "Xm ago", etc.
- `isStale(isoString)` — true if >30 seconds since timestamp
- `isStandardUuid(uuid)` — true if UUID matches Bluetooth SIG base UUID pattern

## Hooks

### useBluetoothAdapter (`src/hooks/useBluetoothAdapter.ts`)

Detects the host machine's Bluetooth adapter on mount.

**State**: `BluetoothAdapterState` (see data-models.md)

**Actions**:
| Function | Description | Status |
|----------|-------------|--------|
| `refresh()` | Re-calls `getBluetoothAdapter()` | Live — real btleplug data |

**Side effect**: Calls `getBluetoothAdapter()` once on mount.

**Return value**: `{ adapter, loading, error, refresh }`

### useDevices (`src/hooks/useDevices.ts`)

Manages device discovery and connection state.

**State**: `DevicesState` (see data-models.md)

**Actions**:
| Function | Description | Status |
|----------|-------------|--------|
| `startScan()` | Calls `startBleScan()`, sets `scanning: true` | Live — real btleplug scanning |
| `stopScan()` | Calls `stopBleScan()`, sets `scanning: false` | Live |
| `refreshSerialPorts()` | Calls `scanSerialPorts()` | Returns mock data |
| `connect(deviceId)` | Calls `connectDevice()` then refreshes connected list | Stub — no-op backend |
| `disconnect(deviceId)` | Calls `disconnectDevice()` then refreshes connected list | Stub — no-op backend |
| `refreshConnected()` | Calls `getConnectedDevices()` | Returns empty array |

**Side effects**:
- Listens to `ble-devices-updated` Tauri event, updates `bleDevices` in state reactively
- Calls `stopBleScan()` on unmount (cleanup)

**Return value**: Spread of `DevicesState` fields + all 6 action functions.

### useUpdater (`src/hooks/useUpdater.ts`)

Auto-update state machine using `@tauri-apps/plugin-updater`.

**State transitions**: `idle → checking → available → downloading → ready`
- Any state can transition to `error`
- `error` can retry back to `checking`

**Actions**:
| Function | Description |
|----------|-------------|
| `checkForUpdate()` | Checks for update via Tauri updater plugin |
| `downloadAndInstall()` | Downloads update with progress tracking |
| `restartApp()` | Calls `relaunch()` from `@tauri-apps/plugin-process` |

**Side effect**: Checks for update on mount (silently ignores errors).

**Return value**: `{ status: UpdateStatus, checkForUpdate, downloadAndInstall, restartApp }`

### useIsMobile (`src/hooks/use-mobile.ts`)

Responsive breakpoint hook. Breakpoint: 768px. Returns `boolean`.
Generated by shadcn sidebar CLI. Used internally by the sidebar component.

## UI Components (shadcn/ui)

All in `src/components/ui/`. These are shadcn/ui components using the new-york style with Tailwind v4 CSS variables.

| Component | File | Variants |
|-----------|------|----------|
| Button | `button.tsx` | variant: default, destructive, outline, secondary, ghost, link / size: default, xs, sm, lg, icon, icon-xs, icon-sm, icon-lg |
| Badge | `badge.tsx` | variant: default, secondary, destructive, outline, info, warn, error |
| Card | `card.tsx` | Card, CardHeader, CardTitle, CardDescription, CardContent |
| Input | `input.tsx` | Single variant |
| ScrollArea | `scroll-area.tsx` | Radix scroll area + scrollbar |
| Separator | `separator.tsx` | orientation: horizontal (default), vertical |
| Tooltip | `tooltip.tsx` | TooltipProvider, Tooltip, TooltipTrigger, TooltipContent |
| Sheet | `sheet.tsx` | side: top, bottom, left, right |
| Sidebar | `sidebar.tsx` | Sidebar, SidebarContent, SidebarGroup, SidebarHeader, SidebarMenu, SidebarMenuItem, SidebarMenuButton, SidebarInset, SidebarProvider, etc. |
| Skeleton | `skeleton.tsx` | Single variant |

## Patterns

### Adding a new page

1. Create `src/pages/NewPage.tsx` exporting a named function component
2. Use `<Header title="..." />` as the first child
3. Add a route in `src/App.tsx` inside `<Routes>`
4. Add an activity item to the `activities` array in `src/components/layout/ActivityRail.tsx` with route, Lucide icon, and label

### Adding a new Tauri command (frontend side)

1. Add the TypeScript return type interface to `src/lib/tauri.ts` if new
2. Add an async wrapper function calling `invoke<ReturnType>("command_name", { params })`
3. Consume via a hook in `src/hooks/` or directly in a component

### Adding a new hook

1. Create `src/hooks/useMyHook.ts`
2. Define state interface
3. Export a function `useMyHook()` returning state + actions
4. Consume in `App.tsx` or in the relevant page component
