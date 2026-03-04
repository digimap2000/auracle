# 004 — Home: Device Discovery View

## Summary

The Home page (`/`) is a live BLE device discovery view. It shows a single centred column of device cards that progressively reveal detail on click. Scanning starts automatically. There is no page header, no toolbar, no buttons — just the devices.

## Page layout

```
ActivityRail │  Content area
             │
   [Home]    │  ┌──────────────────────────────┐
   Browse    │  │ ▐█ Bathys V2    Auracast      │ ← collapsed
   Generate  │  │    Focal Naim                  │
   Compliance│  └──────────────────────────────┘
             │
             │  ┌──────────────────────────────┐
             │  │ ▐█ Muso 3        -42 dBm     │ ← expanded
             │  │    AA:BB:CC:DD:EE:FF          │
             │  │    Focal Naim - 0x029A        │
             │  ├──────────────────────────────┤
             │  │ Service     │ UUID    │ Type  │
             │  │ Auracast    │ 3e1d... │Custom │
             │  │ Audio Strm  │ 0000... │  Std  │
             │  └──────────────────────────────┘
             │
             │  ┌──────────────────────────────┐
             │  │ ▐  Unknown                    │ ← collapsed
             │  │    Apple Inc.                  │
             │  └──────────────────────────────┘
             │
             │  StatusBar
```

### Structure

- No `<Header>` component
- No toolbar or buttons
- Full-height `<ScrollArea>` containing a centred column of device cards
- Column: `max-w-2xl mx-auto w-full px-6 py-6`, cards separated by `space-y-3`
- Flow layout — no fixed pixel widths, centres naturally on wide screens, fills on narrow

### Empty state

When no devices have been discovered:
- Centred Bluetooth icon (24px) with pulse animation at 50% opacity
- Text: "Scanning for devices..."

## Auto-scan

- `useEffect` calls `onStartScan()` on mount and `onStopScan()` on cleanup
- No manual scan controls — scanning is always active while Home is visible
- Props: `bleDevices: BleDevice[]`, `scanning: boolean`, `onStartScan: () => void`, `onStopScan: () => void`

## Sort order

Alphabetical by name (case-insensitive). Stable — never reorders due to signal changes.
- Named devices: A–Z
- "Unknown" devices: sorted to the bottom; RSSI (strongest first) as tiebreaker within the Unknown group

## RSSI smoothing

The displayed RSSI value and the signal bars use a **3-sample rolling average** to reduce jitter. This is implemented in `ble-utils.ts` as a utility, or as a `useMemo`/ref-based cache in the DeviceCard or Home component, keyed by device ID. Each time a device's RSSI updates, the new value is averaged with the previous 2 readings. This smooths the display without adding significant lag.

The smoothed RSSI is used for:
- The `SignalBars` component (bar count and colour)
- The numeric RSSI display in expanded state
- This ensures the icon colour and the number colour always match, since both derive from the same smoothed value via `rssiLevel()`

## Device card — collapsed state

Clickable card: `bg-card border rounded-lg p-3 cursor-pointer hover:border-muted-foreground/30 transition-colors`

### Line 1 (flex row, items-center, justify-between)

**Left group** (`flex items-center gap-2`):
- `<SignalBars rssi={smoothedRssi} />` — 4-bar signal indicator
- Device name: `text-sm font-medium`. If name is "Unknown": `italic text-muted-foreground`

**Right group** (`flex shrink-0 items-center gap-1.5`):
- Up to 2 service badges, priority ordered (see Service Badge Logic below)
- If total badge-worthy services > 2: a `+{N} more` badge in `variant="secondary"`

### Line 2 (`text-[11px] text-muted-foreground`, `mt-1`)

- Manufacturer company name from `resolveCompanyName(device.manufacturer_data[0]?.company_id)`
- If no manufacturer data, this line does not render

## Device card — expanded state

Clicking a collapsed card expands it. Clicking again collapses. Only one card expanded at a time — expanding one closes the other.

The card header **transforms** when expanded to show richer information. The service badges are removed (services now visible in the table below).

### Line 1 (flex row, items-center, justify-between)

**Left group**: `<SignalBars>` + device name (same as collapsed)

**Right**: RSSI value in coloured monospace — `font-mono text-xs font-medium` using `rssiColor(smoothedRssi)`. e.g. "-42 dBm"

### Line 2 (`text-[11px] text-muted-foreground`, `mt-1`)

- Device ID (peripheral ID / MAC address) in `font-mono text-[10px] text-muted-foreground`
- This is the GUID on macOS or the MAC address on other platforms

### Line 3 (`text-[11px] text-muted-foreground`)

- Manufacturer company name + hex company ID: e.g. "Focal Naim - 0x029A"
- If no manufacturer data, this line does not render

### Detail area (collapsible, animated)

Below the header, separated by `<Separator className="my-3" />`.

Smooth expand/collapse animation using the CSS grid `grid-rows-[0fr]` → `grid-rows-[1fr]` technique with `transition-all duration-200`.

Contains **only the services table**. No other sections.

#### Services table

No section heading — the table is self-explanatory.

| Column | Header text | Content | Style |
|--------|-------------|---------|-------|
| Service | "Service" | Resolved name, or "Unknown Service" if unresolved | `text-xs` |
| UUID | "UUID" | Full UUID string | `font-mono text-[10px] text-muted-foreground`, truncated |
| Type | "Type" | "Standard" or "Custom" badge | `text-right`, Badge `variant="secondary" text-[9px]` |

- Standard UUIDs match `0000XXXX-0000-1000-8000-00805f9b34fb`. Everything else is Custom.
- Table uses `table-fixed` with Service at `w-[35%]`, UUID fills remaining space, Type at `w-[72px]`
- Type column: header and content both `text-right` for proper alignment
- If no services: "No services advertised" in muted text

## Service badge logic (collapsed state only)

Resolve all service UUIDs to names. Build badges for **all** services — both known and unknown:

**Priority order** (known services first, then unknowns):
1. "Focal Naim Auracast" → badge "Auracast", `variant="default"` (primary blue)
2. "Audio Stream Control" → badge "Audio Stream", `variant="outline"`
3. "Broadcast Audio Scan" → badge "BA Scan", `variant="outline"`
4. "Published Audio Capabilities" → badge "PAC", `variant="outline"`
5. "Basic Audio Profile" → badge "BAP", `variant="outline"`
6. Any other known service → badge with resolved name, `variant="outline"`
7. Unresolved services → badge "Unknown", `variant="outline"`

Multiple unresolved services should each contribute to the count but display as a single "Unknown" badge (deduplicated for display, but counted individually for the overflow number).

Show up to 2 badges. If there are more: append `+{N} more` badge in `variant="secondary"` where N is the remaining count (after deduplication of display badges).

These badges are **only shown when collapsed**. When expanded, they are replaced by the RSSI reading.

## Signal strength bars

`SignalBars` component — 4 vertical bars indicating signal quality.

Uses `rssiLevel(rssi)` from `ble-utils.ts` which returns `{ bars, color, bgColor }`:
- rssi > -50 → 4 bars, green (`bg-green-500` / `text-green-500`)
- rssi > -60 → 3 bars, green
- rssi > -70 → 2 bars, yellow (`bg-yellow-500` / `text-yellow-500`)
- else → 1 bar, red (`bg-red-500` / `text-red-500`)

Inactive bars: `bg-muted-foreground/20`. Bar widths: `w-1`. Heights: `h-1`, `h-2`, `h-3`, `h-4`. Container: `flex items-end gap-0.5`. Each bar: `rounded-sm`.

Both SignalBars and the numeric RSSI display MUST use the same `rssiLevel()` function with the same smoothed RSSI input to guarantee colour consistency.

## Shared utilities (`src/lib/ble-utils.ts`)

| Export | Purpose |
|--------|---------|
| `KNOWN_SERVICES` | UUID → service name lookup map |
| `resolveServiceName(uuid)` | Returns name or the UUID itself if unknown |
| `resolveCompanyName(id)` | Company ID → name, or null |
| `formatHexBytes(bytes)` | Byte array → "0A FF 12" hex string |
| `rssiLevel(rssi)` | Returns `{ bars, color, bgColor }` — single source of truth for thresholds |
| `rssiColor(rssi)` | Convenience — returns `rssiLevel(rssi).color` |
| `relativeTime(isoString)` | ISO timestamp → "3s ago" |
| `isStale(isoString)` | True if >30s since timestamp |
| `isStandardUuid(uuid)` | True if UUID matches Bluetooth SIG base pattern |

## Component files

| Component | File | Purpose |
|-----------|------|---------|
| `Home` | `src/pages/Home.tsx` | Page: auto-scan, sort, expandedId state, renders DeviceCards in ScrollArea |
| `DeviceCard` | `src/components/devices/DeviceCard.tsx` | Dual-state card: collapsed summary ↔ expanded detail |
| `SignalBars` | `src/components/devices/SignalBars.tsx` | 4-bar signal strength indicator |

## Acceptance criteria

1. Home has no Header and no toolbar — just device cards in a centred column
2. Scanning starts on mount, stops on unmount, no manual controls
3. **Collapsed card**: signal bars, name, up to 2 service badges (including "Unknown" for unresolved) + overflow count, manufacturer name
4. **Expanded card**: signal bars, name, RSSI number (right), device ID (line 2 left), manufacturer + hex ID (line 3), services table
5. Service badges disappear when expanded (replaced by RSSI)
6. Signal bars colour and RSSI number colour always match (both from same smoothed value via `rssiLevel()`)
7. RSSI display uses 3-sample rolling average to reduce jitter
8. Only one card expanded at a time, smooth animated transition
9. Services table: Service / UUID / Type columns, Type right-aligned with badge
10. Alphabetical sort, stable, "Unknown" at bottom
11. Flow layout: `max-w-2xl mx-auto`, no fixed pixel widths
12. `npm run build` and `cargo build` produce zero errors and zero warnings
