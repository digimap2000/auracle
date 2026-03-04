# Activity Rail

## Summary

The activity rail runs the full height of the left edge of the window. The rail uses an icon-above-label layout and selects between top-level activities. Each activity renders its own full-width content area. There is no secondary sidebar — sub-navigation within activities is not our concern.

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

### Rail

- **Width**: `w-[72px]`, fixed, not collapsible
- **Height**: full window height (`h-full`), flex column
- **Background**: `bg-background`, visually continuous with the content area
- **Right border**: theme `border-r`
- **Header spacer**: `h-12` empty div at top, aligns items below macOS window chrome
- **Item container**: flex column, `items-center`, `gap-1`, `p-2`

### Item

Each item is a `NavLink` — full width, flex column, content centred.

- **Icon**: Lucide, 16px
- **Label**: `text-[10px]`, `font-medium`, below icon with `gap-1`
- **Padding**: `py-3 px-2`
- **Left border**: `border-l-2`, always present (transparent when inactive, provides consistent alignment)
- **Transition**: `transition-colors`
- **Home route**: uses `end` prop to prevent matching all routes

### States

| State | Left border | Text colour | Background |
|-------|-------------|-------------|------------|
| Active | `border-primary` | `text-foreground` | none |
| Inactive | `border-transparent` | `text-muted-foreground` | none |
| Hover (inactive only) | `border-transparent` | `text-muted-foreground` | `bg-secondary` `rounded-md` |

### Content area

- Fills remaining width via flex
- Each activity is a full page component rendered via React Router
- StatusBar spans the content area bottom (not the rail)
