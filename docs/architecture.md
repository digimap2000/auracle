# Architecture

## Product

Auracle is a Tauri 2 desktop application for managing BLE and serial audio devices. It targets macOS (arm64 + x86_64). The app identifier is `com.auracle.app`.

## Tech Stack

| Layer | Technology | Version |
|-------|-----------|---------|
| Desktop framework | Tauri | 2 |
| Backend language | Rust | Edition 2021 |
| Frontend framework | React | 18.3.1 |
| Language | TypeScript | 5.7.3 |
| Build tool | Vite | 6.0.0 |
| CSS framework | Tailwind CSS | 4.0.0 |
| Component library | shadcn/ui | new-york style |
| Icons | lucide-react | 0.469.0 |
| Font | Geist (variable) | 1.3.1 |
| Router | react-router-dom | 7.1.0 |
| BLE | btleplug | 0.11 |
| Serial | serialport | 4 |
| Async runtime | tokio | 1 (full features) |
| Error handling | thiserror | 2 |
| Serialization | serde + serde_json | 1 |

## Directory Structure

```
auracle/
├── docs/                          # Specification documents (this folder)
├── public/
│   └── fonts/
│       ├── Geist-Variable.woff2   # Sans-serif variable font
│       └── GeistMono-Variable.woff2
├── src/                           # React frontend
│   ├── components/
│   │   ├── layout/                # App shell components
│   │   │   ├── Header.tsx
│   │   │   ├── Sidebar.tsx        # Exports AppSidebar
│   │   │   └── StatusBar.tsx
│   │   └── ui/                    # shadcn/ui primitives
│   │       ├── badge.tsx
│   │       ├── button.tsx
│   │       ├── card.tsx
│   │       ├── input.tsx
│   │       ├── scroll-area.tsx
│   │       ├── separator.tsx
│   │       ├── sheet.tsx
│   │       ├── sidebar.tsx
│   │       ├── skeleton.tsx
│   │       └── tooltip.tsx
│   ├── hooks/
│   │   ├── use-mobile.ts          # Responsive breakpoint hook
│   │   ├── useDevices.ts          # Device state management
│   │   └── useUpdater.ts          # Auto-update state machine
│   ├── lib/
│   │   ├── tauri.ts               # Typed Tauri invoke wrappers
│   │   └── utils.ts               # cn() classname helper
│   ├── pages/
│   │   ├── Dashboard.tsx
│   │   ├── Devices.tsx
│   │   ├── Logs.tsx
│   │   └── StreamConfig.tsx
│   ├── App.tsx                    # Root component (routing, layout)
│   ├── index.css                  # Design system (tokens, fonts, base styles)
│   ├── main.tsx                   # React entry point
│   └── vite-env.d.ts
├── src-tauri/                     # Rust backend
│   ├── capabilities/
│   │   └── default.json           # Tauri ACL permissions
│   ├── icons/                     # App icons (all platforms)
│   ├── src/
│   │   ├── ble/
│   │   │   └── mod.rs             # BLE scanning (btleplug)
│   │   ├── devices/
│   │   │   ├── mod.rs             # Device trait + types
│   │   │   └── nrf5340.rs         # nRF5340 Audio DK implementation
│   │   ├── serial/
│   │   │   └── mod.rs             # Serial port scanning
│   │   ├── error.rs               # AuracleError enum
│   │   ├── lib.rs                 # Tauri commands + app setup
│   │   └── main.rs                # Entry point
│   ├── Cargo.toml
│   ├── build.rs
│   └── tauri.conf.json
├── .github/workflows/
│   └── release.yml                # CI/CD build + release
├── components.json                # shadcn/ui configuration
├── index.html                     # HTML entry (class="dark")
├── package.json
├── tsconfig.json
├── tsconfig.app.json
├── tsconfig.node.json
└── vite.config.ts
```

## Build System

### Frontend
- **Dev server**: `npm run dev` → Vite on port 1420 (strict)
- **Production build**: `npm run build` → `tsc -b && vite build` → output in `dist/`
- **Path alias**: `@/` maps to `./src`

### Backend
- **Crate name**: `auracle_lib`
- **Crate types**: `staticlib`, `cdylib`, `rlib`
- **Build dependency**: `tauri-build` v2

### Tauri Integration
- **Dev command**: `npm run dev` (Vite)
- **Build command**: `npm run build`
- **Dev URL**: `http://localhost:1420`
- **Frontend dist**: `../dist`

## Window Configuration

| Property | Value |
|----------|-------|
| Title | Auracle |
| Default size | 1280 x 800 |
| Minimum size | 960 x 600 |
| CSP | null (local resources) |

## Tauri Plugins

| Plugin | Version | Purpose |
|--------|---------|---------|
| tauri-plugin-opener | 2 | Open URLs and files |
| tauri-plugin-process | 2.3.1 | App relaunch after update (desktop only) |
| tauri-plugin-updater | 2.10.0 | Auto-update from GitHub Releases (desktop only) |

## Tauri Capabilities

File: `src-tauri/capabilities/default.json`

Permissions granted to the main window:
- `core:default`
- `opener:default`
- `updater:default`
- `process:allow-restart`

## shadcn/ui Configuration

File: `components.json`

| Setting | Value |
|---------|-------|
| Style | new-york |
| RSC | false |
| TSX | true |
| Base color | neutral |
| CSS variables | true |
| Icon library | lucide |
| CSS file | src/index.css |

Import aliases:
- Components: `@/components`
- UI: `@/components/ui`
- Lib: `@/lib`
- Hooks: `@/hooks`
- Utils: `@/lib/utils`
