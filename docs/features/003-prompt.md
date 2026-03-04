# 003 — Activity Rail Navigation — Coding Prompt

## Before you start

Read these files in order:
1. `CLAUDE.md` — project rules
2. `.skills/auracle-frontend/SKILL.md` — frontend coding patterns
3. `docs/design-system.md` — color tokens, typography, spacing
4. `docs/frontend.md` — current layout, routing, hooks
5. `docs/features/003-activity-rail-navigation.md` — the spec for this feature

## What you're building

Replace the existing 220px sidebar navigation with a 72px activity rail. The rail is a simple vertical strip of icon+label buttons that switch between four activities via React Router.

## Step-by-step

### 1. Create the ActivityRail component

**File**: `src/components/layout/ActivityRail.tsx`

Build a new component that renders a fixed-width 72px vertical rail. It does NOT use any shadcn sidebar primitives — it's a plain `<nav>` element with Tailwind styling.

```
Structure:
<nav>  (w-[72px], h-full, border-r, flex flex-col, bg-background)
  <div>  (h-12 spacer — aligns with content header height)
  <div>  (flex flex-col, gap-1, p-2, items-center)
    {activities.map → NavLink}
  </div>
</nav>
```

Each activity item is a `NavLink` with:
- `end` prop on `/` to prevent matching all routes
- Vertical flex layout: icon (16px) above label (`text-[10px] font-medium`)
- Full width within the rail, centred content
- **Active**: `border-l-2 border-primary text-foreground`
- **Inactive**: `border-l-2 border-transparent text-muted-foreground`
- **Hover**: `hover:bg-secondary rounded-md`
- Padding: `py-3 px-2`
- Transition on colors: `transition-colors`

Activities array:
| Route | Icon (from lucide-react) | Label |
|-------|--------------------------|-------|
| `/` | `Bluetooth` | Home |
| `/browse` | `Search` | Browse |
| `/generate` | `Radio` | Generate |
| `/compliance` | `ClipboardCheck` | Compliance |

### 2. Update App.tsx layout

Replace the entire `SidebarProvider` / `AppSidebar` / `SidebarInset` structure with:

```tsx
<div className="flex h-screen w-screen overflow-hidden">
  <ActivityRail />
  <div className="flex flex-1 flex-col overflow-hidden">
    <main className="flex flex-1 flex-col overflow-hidden">
      <Routes>...</Routes>
    </main>
    <StatusBar ... />
  </div>
</div>
```

Remove imports: `AppSidebar`, `SidebarInset`, `SidebarProvider`.
Add import: `ActivityRail`.

### 3. Update routes

| Old route | New route | Component |
|-----------|-----------|-----------|
| `/` (Dashboard) | `/` | `Home` (renamed from Dashboard) |
| `/devices` | `/browse` | `Devices` (no internal changes) |
| `/stream-config` | `/generate` | `StreamConfig` (no internal changes) |
| `/logs` | *removed* | — |
| — | `/compliance` | `Compliance` (new stub) |

### 4. Rename Dashboard to Home

Rename `src/pages/Dashboard.tsx` to `src/pages/Home.tsx`. Update the component name and the Header title from "Dashboard" to "Home". Keep all existing content (adapter status cards, etc.) — this is still the hero view for now. Update the import in App.tsx.

### 5. Create Compliance stub page

**File**: `src/pages/Compliance.tsx`

```tsx
export function Compliance() {
  return (
    <>
      <Header title="Compliance" description="LE Audio certification test runner" />
      <div className="flex flex-1 items-center justify-center">
        <div className="text-center text-muted-foreground">
          <ClipboardCheck className="mx-auto mb-3 size-8 opacity-30" />
          <p className="text-sm">Compliance test runner coming soon</p>
        </div>
      </div>
    </>
  );
}
```

### 6. Clean up

- Remove the `navItems` array and `AppSidebar` from `src/components/layout/Sidebar.tsx` (delete the file)
- The shadcn sidebar UI components in `src/components/ui/sidebar.tsx` can stay — don't delete them
- Remove the `/logs` route from App.tsx (the Logs page file stays, just no route)
- Update any Header titles if needed (Devices page header can say "Browse" or stay as "Devices" — keep "Devices" for now since it's still the device explorer)

## Quality checks

Before finishing:
1. `npm run build` — zero errors, zero TS warnings
2. Verify no unused imports or variables
3. Confirm the active state works: clicking each rail item should highlight it and show the correct content
4. Confirm the BLE scanner still works on the Browse page (it's the same Devices component, just at a new route)
5. StatusBar should still be visible at the bottom of the content area, not overlapping the rail

## What NOT to do

- Don't add any secondary sidebar or sub-navigation — each activity is a single full-width page
- Don't modify the Devices page internals — it just moves to `/browse`
- Don't modify the StreamConfig page internals — it just moves to `/generate`
- Don't delete `src/components/ui/sidebar.tsx` — leave shadcn components alone
- Don't add Settings to the rail — that will come later
- Don't change StatusBar — it stays exactly as-is
