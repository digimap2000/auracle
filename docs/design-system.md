# Design System

## CSS Architecture

**File**: `src/index.css`

The design system uses Tailwind CSS v4 with CSS-based configuration (no `tailwind.config.js`).

```css
@import "tailwindcss";
@import "tw-animate-css";
```

### Theme Mapping

CSS custom properties are mapped to Tailwind color utilities via `@theme inline`:

```css
@theme inline {
  --color-background: var(--background);
  --color-foreground: var(--foreground);
  --color-primary: var(--primary);
  /* ... etc */
}
```

This allows using `bg-background`, `text-foreground`, `bg-primary`, etc. in Tailwind classes.

### Dark Mode

Dark mode is active by default. The `<html>` element has `class="dark"` in `index.html`.

Dark variant is defined as:
```css
@custom-variant dark (&:is(.dark *));
```

### Base Layer

```css
@layer base {
  * { @apply border-border; }
  body {
    @apply bg-background text-foreground font-sans antialiased;
    font-feature-settings: "rlig" 1, "calt" 1;
  }
}
```

## Color Tokens

All tokens are defined as CSS custom properties in `src/index.css`.

### Dark Mode Palette (`.dark`)

| Token | Hex | Usage |
|-------|-----|-------|
| `--background` | `#0a0a0a` | Page background |
| `--foreground` | `#ededed` | Default text |
| `--card` | `#111111` | Card background |
| `--card-foreground` | `#ededed` | Card text |
| `--popover` | `#111111` | Popover background |
| `--popover-foreground` | `#ededed` | Popover text |
| `--primary` | `#0070f3` | Primary actions, links, active states |
| `--primary-foreground` | `#ffffff` | Text on primary background |
| `--secondary` | `#1a1a1a` | Secondary backgrounds |
| `--secondary-foreground` | `#ededed` | Text on secondary background |
| `--muted` | `#1a1a1a` | Muted backgrounds |
| `--muted-foreground` | `#a1a1a1` | Muted text, descriptions, labels |
| `--accent` | `#0070f3` | Accent color (same as primary) |
| `--accent-foreground` | `#ffffff` | Text on accent background |
| `--destructive` | `#e5484d` | Error states, destructive actions |
| `--destructive-foreground` | `#ffffff` | Text on destructive background |
| `--border` | `#1f1f1f` | Default borders |
| `--input` | `#1f1f1f` | Input borders |
| `--ring` | `#0070f3` | Focus ring color |
| `--surface` | `#111111` | Surface elevation |

### Sidebar Tokens (`.dark`)

| Token | Hex | Usage |
|-------|-----|-------|
| `--sidebar` | `#0a0a0a` | Sidebar background |
| `--sidebar-foreground` | `#ededed` | Sidebar text |
| `--sidebar-primary` | `#0070f3` | Active nav item |
| `--sidebar-primary-foreground` | `#ffffff` | Active nav item text |
| `--sidebar-accent` | `#1a1a1a` | Hover state background |
| `--sidebar-accent-foreground` | `#ededed` | Hover state text |
| `--sidebar-border` | `#1f1f1f` | Sidebar borders |
| `--sidebar-ring` | `#0070f3` | Sidebar focus ring |

### Light Mode Tokens (`:root`)

Light mode tokens are defined but the app defaults to dark mode. They use HSL format:

| Token | Value | Usage |
|-------|-------|-------|
| `--sidebar` | `hsl(0 0% 98%)` | Sidebar background |
| `--sidebar-foreground` | `hsl(240 5.3% 26.1%)` | Sidebar text |
| `--sidebar-primary` | `hsl(240 5.9% 10%)` | Active nav item |
| `--sidebar-primary-foreground` | `hsl(0 0% 98%)` | Active nav item text |
| `--sidebar-accent` | `hsl(240 4.8% 95.9%)` | Hover state |
| `--sidebar-accent-foreground` | `hsl(240 5.9% 10%)` | Hover state text |
| `--sidebar-border` | `hsl(220 13% 91%)` | Borders |
| `--sidebar-ring` | `hsl(217.2 91.2% 59.8%)` | Focus ring |

## Typography

### Font Families

Defined in `:root`:
```css
--font-geist-sans: "Geist", ui-sans-serif, system-ui, sans-serif;
--font-geist-mono: "Geist Mono", ui-monospace, monospace;
```

Mapped to Tailwind via `@theme inline`:
- `font-sans` → `var(--font-geist-sans)`
- `font-mono` → `var(--font-geist-mono)`

### Font Files

Variable fonts loaded from `public/fonts/` via `@font-face`:
- `Geist-Variable.woff2` — weight range 100–900
- `GeistMono-Variable.woff2` — weight range 100–900

Both use `font-display: swap`.

### Font Features

Applied to `body`:
```css
font-feature-settings: "rlig" 1, "calt" 1;
```

### Text Scale Used in Components

| Context | Size | Weight | Font |
|---------|------|--------|------|
| Page title (Header) | `text-sm` | `font-medium` | Sans |
| Page description (Header) | `text-[11px]` | Normal | Sans |
| Sidebar brand | `text-sm` | `font-semibold` | Mono |
| Sidebar nav items | Default (via SidebarMenuButton) | Normal | Sans |
| Status bar text | `text-[11px]` | Normal | Sans |
| Status bar version | `text-[10px]` | Normal | Mono |
| Log entries | `text-[11px]` | Normal | Mono |
| Card stat numbers | `text-2xl` | `font-semibold` | Sans |
| Badge text | `text-[10px]` | `font-medium` | Sans |

## Spacing & Radius

### Border Radius Scale

Defined in `:root`:
```css
--radius: 0.5rem;
```

Mapped in `@theme inline`:
```css
--radius-sm: calc(var(--radius) - 4px);  /* 0.25rem */
--radius-md: calc(var(--radius) - 2px);  /* 0.375rem */
--radius-lg: var(--radius);               /* 0.5rem */
--radius-xl: calc(var(--radius) + 4px);  /* 0.75rem */
```

Use via Tailwind: `rounded-sm`, `rounded-md`, `rounded-lg`, `rounded-xl`.

## Component Variants

### Button (`src/components/ui/button.tsx`)

Built with `class-variance-authority` (CVA).

**Style variants**:
| Variant | Appearance |
|---------|-----------|
| `default` | Primary bg, white text |
| `destructive` | Red bg, white text |
| `outline` | Border, transparent bg, hover accent |
| `secondary` | Secondary bg |
| `ghost` | No bg, hover accent |
| `link` | Text with underline on hover |

**Size variants**:
| Size | Height | Padding |
|------|--------|---------|
| `default` | h-9 | px-4 py-2 |
| `xs` | h-6 | px-2, text-xs |
| `sm` | h-8 | px-3 |
| `lg` | h-10 | px-6 |
| `icon` | size-9 | — |
| `icon-xs` | size-6 | — |
| `icon-sm` | size-8 | — |
| `icon-lg` | size-10 | — |

Supports `asChild` prop for composition via Radix Slot.

### Badge (`src/components/ui/badge.tsx`)

**Variants**:
| Variant | Appearance |
|---------|-----------|
| `default` | Primary bg |
| `secondary` | Secondary bg |
| `destructive` | Destructive bg |
| `outline` | Border only |
| `info` | Blue tinted bg, blue text |
| `warn` | Yellow tinted bg, yellow text |
| `error` | Red tinted bg, red text |

### Sidebar (`src/components/ui/sidebar.tsx`)

Fixed width: 220px (`SIDEBAR_WIDTH` constant). The sidebar width is also set as a CSS variable `--sidebar-width`.

## Patterns

### Adding a new color token

1. Add the CSS variable to the `.dark` block in `src/index.css`:
   ```css
   --my-token: #hexvalue;
   ```
2. Map it in `@theme inline`:
   ```css
   --color-my-token: var(--my-token);
   ```
3. Use in Tailwind classes: `bg-my-token`, `text-my-token`, etc.

### Adding a new component variant

For CVA-based components (button, badge):
1. Add the variant to the `variants` object in the CVA definition
2. The variant becomes available as a prop value automatically
