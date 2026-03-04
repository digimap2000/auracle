# 004 — Home Device List — Refinement Prompt

## Before you start

Read these files:
1. `CLAUDE.md` — project rules
2. `docs/features/004-home-device-panels.md` — the consolidated spec (single source of truth)
3. `src/components/devices/DeviceCard.tsx` — current implementation
4. `src/components/devices/SignalBars.tsx` — current implementation
5. `src/lib/ble-utils.ts` — current utilities
6. `src/pages/Home.tsx` — current page

## What's already working

- Home page with auto-scan, no header, no toolbar, centred column of DeviceCards
- Collapsed state: signal bars, name, service badges, manufacturer
- Expanded state: signal bars, name, RSSI number, device ID + manufacturer with hex, services table
- Alphabetical sort, accordion expand/collapse with animation
- `rssiLevel()` shared function returns bars/color/bgColor

## What needs fixing — 5 changes

### 1. Move device ID under RSSI number in expanded state

Currently the expanded line 2 shows `device.id | Manufacturer - 0xHEX` in a single flex row.

Change to: the expanded header has **3 lines**, not 2:

**Line 1**: signal bars + name (left), RSSI number (right) — *already correct*

**Line 2**: device ID in `font-mono text-[10px] text-muted-foreground` — on its own line, left-aligned. This fills the empty space below the signal bars on the left side.

**Line 3**: manufacturer name + hex company ID — e.g. "Focal Naim - 0x029A" in `text-[11px] text-muted-foreground`. If no manufacturer data, this line doesn't render.

This replaces the current layout where device ID and manufacturer are on one line with a pipe separator.

### 2. Show "Unknown" badge for unresolved services

The current code already maps unresolved services to "Unknown" and deduplicates them. Verify this works correctly:

- A device with 3 unresolved services and 1 known should show: `[Known Badge] [Unknown]` (2 badges, no overflow)
- A device with 3 known and 2 unresolved should show: `[Top Known] [Second Known] +2 more` (the +2 counts the third known + the deduplicated Unknown)
- A device with only unresolved services should show: `[Unknown]` (single badge)

The current implementation looks correct — just verify the overflow count math accounts for deduplication properly. The overflow count should be `uniqueBadgeNames.length - 2` (after dedup), not based on raw service count.

### 3. RSSI 3-sample rolling average

Add smoothing to prevent the displayed RSSI and signal bars from jumping between thresholds.

**Implementation**: Add a `useRef` in Home.tsx to store a rolling window of RSSI samples per device:

```typescript
const rssiHistory = useRef<Map<string, number[]>>(new Map());

const smoothedDevices = useMemo(() => {
  return sortedDevices.map((device) => {
    const history = rssiHistory.current.get(device.id) ?? [];
    history.push(device.rssi);
    // Keep only last 3 samples
    if (history.length > 3) history.shift();
    rssiHistory.current.set(device.id, history);
    const avg = Math.round(history.reduce((a, b) => a + b, 0) / history.length);
    return { ...device, rssi: avg };
  });
}, [sortedDevices]);
```

Pass `smoothedDevices` (not `sortedDevices`) to the DeviceCard components. This means both `SignalBars` and the numeric RSSI in expanded state use the smoothed value, guaranteeing colour consistency.

**Important**: The sort should still use the raw `bleDevices` for stability (alphabetical by name doesn't depend on RSSI for named devices). The smoothing only affects display.

### 4. Verify signal colour consistency

Both `SignalBars` and the expanded RSSI number should use the same smoothed RSSI value. Since:
- `SignalBars` uses `rssiLevel(rssi).bgColor` for bar colours
- DeviceCard uses `rssiColor(rssi)` which returns `rssiLevel(rssi).color`
- Both `bg-green-500` and `text-green-500` map to the same green

As long as both receive the same smoothed RSSI, the colours will always match. The 3-sample averaging in change #3 ensures this — the smoothed value is applied before the device object reaches DeviceCard.

No code change needed here beyond #3, but verify after implementing that the colours are visually consistent.

### 5. Verify Type column alignment in services table

The table currently uses `table-fixed` with:
- Service column: `w-[35%]`
- UUID column: fills remaining space
- Type column: `w-[72px]`, header `text-right`, content `text-right`

Verify the Type badge aligns neatly with its header. If the badge content doesn't right-align visually (because the Badge component has internal padding), you may need to wrap it in a `flex justify-end`:

```tsx
<td className="py-1.5">
  <div className="flex justify-end">
    <Badge variant="secondary" className="text-[9px]">
      {standard ? "Standard" : "Custom"}
    </Badge>
  </div>
</td>
```

Check and fix if needed.

## Files to modify

1. `src/pages/Home.tsx` — add RSSI smoothing (useRef + useMemo), pass smoothed devices
2. `src/components/devices/DeviceCard.tsx` — restructure expanded header to 3 lines (device ID on its own line)

## Quality checks

1. `npm run build` — zero errors, zero TS warnings
2. Expanded card: line 1 = signal + name + RSSI, line 2 = device ID, line 3 = manufacturer + hex
3. Signal bars colour matches RSSI number colour in expanded state
4. RSSI value is stable (doesn't flicker between thresholds)
5. Unknown services show as "Unknown" badge in collapsed state
6. Type column badges align right with their header text

## What NOT to do

- Don't add a Header or toolbar
- Don't change the sort order
- Don't add stale device dimming
- Don't modify StatusBar, ActivityRail, or useDevices hook
- Don't modify the Rust backend
