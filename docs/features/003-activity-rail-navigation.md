# 003 — Activity Rail Navigation

## Summary

Replace the existing sidebar navigation with a vertical activity rail on the left edge of the window. The rail uses an icon-above-label layout (~72px wide) and selects between four top-level activities. Each activity renders its own full-width content area. There is no secondary sidebar — sub-navigation within activities uses tabs or inline panels.

## Activities

| Position | ID | Label | Icon (Lucide) | Route | Description |
|----------|----|-------|---------------|-------|-------------|
| 1 | `home` | Home | `Bluetooth` | `/` | Hero overview of the Bluetooth world — adapter status, nearby device summary, signal landscape |
| 2 | `browse` | Browse | `Search` | `/browse` | Service browser — the existing two-panel BLE device explorer (formerly Devices page) |
| 3 | `generate` | Generate | `Radio` | `/generate` | Auracast stream generation — configure and broadcast LE Audio streams |
| 4 | `compliance` | Compliance | `ClipboardCheck` | `/compliance` | Batched certification test runner for LE Audio compliance |

## Layout

```
┌──────┬────────────────────────────────────────┐
│      │                                        │
│ [BT] │                                        │
│ Home │          Content Area                  │
│      │          (full width, per activity)    │
│ [🔍] │                                        │
│Browse│                                        │
│      │                                        │
│ [📻] │                                        │
│ Gen  │                                        │
│      │                                        │
│ [✓]  │                                        │
│ Cert │                                        │
│      │                                        │
│      ├────────────────────────────────────────┤
│      │ StatusBar                              │
└──────┴────────────────────────────────────────┘
```

### Rail specification

- **Width**: 72px fixed, not collapsible
- **Background**: `var(--background)` (`#0a0a0a`) — same as page background, no visual separation from content except a right border
- **Right border**: `var(--border)` (`#1f1f1f`)
- **Item layout**: icon (16px) centred above label text, stacked vertically
- **Label font**: Geist Sans, `text-[10px]`, `font-medium`, `text-muted-foreground`
- **Active state**: left 2px accent border (`var(--primary)`), icon and label switch to `text-foreground`
- **Hover state**: `bg-secondary` (`#1a1a1a`) with `rounded-md`
- **Item padding**: `py-3 px-2`, centred content
- **Items aligned**: top of the rail, below a 12px-tall header spacer (aligns with content area header)
- **Gap between items**: `gap-1`

### Content area

- Fills remaining width (`flex-1`)
- Each activity is a full page component rendered via React Router
- StatusBar remains at the bottom, spanning the content area (not the rail)

## Routing changes

### Remove
- `/` → Dashboard (replaced by Home activity)
- `/devices` → Devices (replaced by Browse activity)
- `/stream-config` → StreamConfig (becomes Generate activity)
- `/logs` → Logs (removed as a top-level route; logging will be integrated into activities later)

### Add
- `/` → Home (new hero page — initially shows adapter status card and device summary from old Dashboard, plus placeholder for signal landscape)
- `/browse` → Browse (existing Devices page, moved to new route)
- `/generate` → Generate (existing StreamConfig page, moved to new route)
- `/compliance` → Compliance (new stub page)

## Component changes

### Delete
- `src/components/layout/Sidebar.tsx` — replaced entirely by ActivityRail

### Create
- `src/components/layout/ActivityRail.tsx` — the new rail component

### Modify
- `src/App.tsx` — replace SidebarProvider/AppSidebar with ActivityRail, update routes
- `src/index.css` — add `--rail-width: 72px` CSS variable, add rail-specific tokens if needed
- `src/pages/Dashboard.tsx` → rename/repurpose as `src/pages/Home.tsx`
- `src/pages/Devices.tsx` — no internal changes, just mounted at `/browse`
- `src/pages/StreamConfig.tsx` — no internal changes, just mounted at `/generate`

### Create (stub)
- `src/pages/Compliance.tsx` — stub page with Header + empty state ("Compliance test runner coming soon")

## ActivityRail component spec

```tsx
// src/components/layout/ActivityRail.tsx

interface ActivityItem {
  to: string;
  icon: LucideIcon;
  label: string;
}

const activities: ActivityItem[] = [
  { to: "/", icon: Bluetooth, label: "Home" },
  { to: "/browse", icon: Search, label: "Browse" },
  { to: "/generate", icon: Radio, label: "Generate" },
  { to: "/compliance", icon: ClipboardCheck, label: "Compliance" },
];
```

Each item renders as a `NavLink` with:
- `end` prop on the Home (`/`) route to prevent matching all routes
- Active state detected via NavLink's `className` callback (`isActive` parameter)
- Icon at 16px, label below at `text-[10px]`
- Active: left-2 primary border, `text-foreground` on icon and label
- Inactive: no border, `text-muted-foreground`
- Hover: `bg-secondary` rounded

## Migration notes

- The shadcn `Sidebar`, `SidebarProvider`, `SidebarInset` components are no longer used in the layout. They can remain in `src/components/ui/` for potential future use but are not imported in the layout.
- The `useIsMobile` hook is no longer needed for sidebar collapse behaviour.
- The Logs page is not mapped to any activity. It stays in `src/pages/Logs.tsx` for now but has no route. It will be reintegrated later as a panel within activities.

## Acceptance criteria

1. The app shows a 72px icon+label rail on the left edge
2. Four activities are listed: Home, Browse, Generate, Compliance
3. Clicking an activity switches the content area via React Router
4. The active activity has a blue left border and white text
5. The existing BLE explorer works identically at `/browse`
6. StatusBar remains at the bottom of the content area
7. No regressions in BLE scanning, adapter detection, or device detail display
8. `npm run build` and `cargo build` produce zero errors and zero warnings
