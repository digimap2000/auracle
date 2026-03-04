# Feature 002 — Refinement 2: Device ID cleanup

Read CLAUDE.md, then apply these two surgical fixes.

## Fix 1: Remove device ID from list rows

**File**: `src/pages/Devices.tsx`

In the device list rows (inside the `filteredDevices.map()` block), remove the line that displays `device.id` in monospace below the device name. The list rows should show only the device name, RSSI, and last-seen time. The ID is not useful as a user-facing identifier in the list — on macOS it's a CoreBluetooth-assigned UUID, on Linux/Windows it's a MAC address.

Remove this block from the list row:
```tsx
<p className="truncate font-mono text-[10px] text-muted-foreground">
  {device.id}
</p>
```

## Fix 2: Add platform-aware label to ID in detail pane

**File**: `src/pages/Devices.tsx`

In the `DeviceDetail` component header, the device ID is shown as a raw string with no context. Replace it with a labelled field that explains what the ID actually is on each platform.

Replace:
```tsx
<p className="font-mono text-xs text-muted-foreground">{device.id}</p>
```

With something like:
```tsx
<div className="flex items-center gap-1.5">
  <span className="text-[10px] text-muted-foreground">
    {navigator.platform.startsWith("Mac") ? "Peripheral ID (macOS)" : "MAC Address"}
  </span>
</div>
<p className="font-mono text-xs text-muted-foreground">{device.id}</p>
```

The label should be small (`text-[10px]`) and muted, sitting above the ID value. Use `navigator.platform` or `navigator.userAgent` to detect macOS — check for "Mac" prefix. On non-Mac platforms (Linux, Windows), label it "MAC Address" since btleplug exposes the real hardware address there.

## After fixing

1. `npm run build` — zero errors
2. Update `docs/frontend.md` — remove mention of device ID in list rows, add the platform-aware label detail to the detail pane description
