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
<SidebarProvider>
  <AppSidebar />
  <SidebarInset>
    <main>
      <Routes> ... </Routes>
    </main>
    <StatusBar />
  </SidebarInset>
</SidebarProvider>
```

## Routing

| Path | Component | Props |
|------|-----------|-------|
| `/` | `Dashboard` | `connectedCount: number`, `bluetoothAdapter: { adapter, loading, error, refresh }` |
| `/devices` | `Devices` | `bleDevices: BleDevice[]`, `scanning: boolean`, `onStartScan: () => void`, `onStopScan: () => void` |
| `/stream-config` | `StreamConfig` | none |
| `/logs` | `Logs` | none |

All routes are defined in `src/App.tsx` using `<Routes>` and `<Route>`.

## Pages

### Dashboard (`src/pages/Dashboard.tsx`)

Shows three stat cards in a 3-column grid:
- **Bluetooth Adapter** — shows adapter status via `bluetoothAdapter` prop (Bluetooth icon). Loading state uses Skeleton, error state shows message with Retry button, available state shows adapter name with green badge.
- **Connected Devices** — displays `connectedCount` prop (Radio icon)
- **Active Streams** — hardcoded to `0` (Activity icon)

Below the stats: an empty-state card with "No active streams" message.

**Status**: Bluetooth adapter card is live (uses real btleplug data). Active streams count is hardcoded.

### Devices (`src/pages/Devices.tsx`)

Two-panel BLE explorer. Left panel: device list. Right panel: detail pane.

**Toolbar**:
- Start/Stop toggle button: "Start Scan" (Play icon, primary) / "Stop" (Square icon, destructive)
- Filter input: instant client-side filter by name or ID (uses `useState` + `useMemo`)
- Device count: "{N} devices" in muted text. Shows pulsing "Scanning" badge when active.

**Device list (left panel)**:
- ScrollArea containing rows sorted alphabetically by name ("Unknown" pushed to bottom), RSSI as tiebreaker
- Each row: device name (or "Unknown" in italic), RSSI with colour coding (green > -50, yellow -50 to -70, red < -70), relative time since last seen
- Selected row has `bg-accent/50` + left border accent
- Rows dim (50% opacity) when stale (>30 seconds since last seen)

**Detail pane (right panel)**:
- Empty state: "Select a device to view details" with Bluetooth icon
- When selected: Header (name + platform-aware ID label — "Peripheral ID (macOS)" on Mac, "MAC Address" elsewhere), Signal (RSSI + TX power), Services (UUID always shown in monospace, with name badge alongside when resolved from SIG standard or custom UUIDs — duplicates are shown faithfully, never deduplicated), Manufacturer Data (company name lookup from full SIG database via `src/data/ble-company-ids.json` + hex dump), Timing (last seen)
- Reactively updates as new scan data arrives for the selected device

**Empty state** (no scan run): "Click Start Scan to discover nearby Bluetooth devices"

**Scanning indicator**: Subtle ring around content area (`ring-1 ring-primary/20`)

**Status**: Live — uses real btleplug data via `ble-devices-updated` events.

### StreamConfig (`src/pages/StreamConfig.tsx`)

Audio stream configuration form inside a Card (max-w-lg):
- **Stream Name** — text input, placeholder "My Audio Stream"
- **Codec** — select: LC3, LC3plus, SBC, AAC
- **Sample Rate** — select: 16 kHz, 24 kHz, 32 kHz, 48 kHz
- **Bitrate** — range slider 16–320 kbps, default 96
- **Start Stream** button — disabled

**Status**: UI complete. Form is not connected to state or backend. All inputs are uncontrolled. Start Stream button is permanently disabled.

### Logs (`src/pages/Logs.tsx`)

Application event log viewer:
- **Toolbar** — Clear button (ghost), Export button (ghost, disabled)
- **Log list** — ScrollArea with monospace rows showing timestamp, level badge, message
- **Timestamp format** — `HH:MM:SS.mmm` via `formatTimestamp()` helper

Uses mock data (3 entries). Log levels: `info`, `warn`, `error` rendered as Badge variants.

**Status**: UI complete. Uses hardcoded mock data. Clear button is not wired. Export is disabled.

## Layout Components

### AppSidebar (`src/components/layout/Sidebar.tsx`)

Fixed-width (220px) sidebar using shadcn/ui Sidebar primitives. `collapsible="none"`.

Structure:
- `SidebarHeader` — "Auracle" branding in monospace font
- `SidebarSeparator`
- `SidebarContent` → `SidebarGroup` → `SidebarMenu`

Navigation items:

| Path | Icon | Label |
|------|------|-------|
| `/` | `LayoutDashboard` | Dashboard |
| `/devices` | `Radio` | Devices |
| `/stream-config` | `Settings2` | Stream Config |
| `/logs` | `ScrollText` | Logs |

Each item uses `SidebarMenuButton asChild` wrapping a `NavLink`. Active state styling is handled by the shadcn sidebar's `data-active` attribute reacting to NavLink's active class.

### Header (`src/components/layout/Header.tsx`)

Page header bar (h-12) with bottom border.

Props:
- `title: string` (required) — rendered as `<h1>` in text-sm font-medium
- `description?: string` — rendered as `<p>` in text-[11px] text-muted-foreground

Used by all 4 pages.

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
4. Add a nav item to the `navItems` array in `src/components/layout/Sidebar.tsx` with path, Lucide icon, and label

### Adding a new Tauri command (frontend side)

1. Add the TypeScript return type interface to `src/lib/tauri.ts` if new
2. Add an async wrapper function calling `invoke<ReturnType>("command_name", { params })`
3. Consume via a hook in `src/hooks/` or directly in a component

### Adding a new hook

1. Create `src/hooks/useMyHook.ts`
2. Define state interface
3. Export a function `useMyHook()` returning state + actions
4. Consume in `App.tsx` or in the relevant page component
