# Auracle

Professional desktop test harness for Auracast LE Audio broadcast streams. Built with Tauri 2 (Rust) + React 18 (TypeScript). Developed by Focal Naim for use during development of Muso 3 and Bathys V2.

## Development Workflow

This project uses two AI tools with distinct roles:

**Cowork** (planning and design):
- Feature planning and specification writing
- Architecture decisions and trade-offs
- Code review and issue triage
- Refinement: categorising fixes as skill gaps (reusable), spec gaps (permanent), or implementation bugs (one-off)
- Writing coding prompts for the coder

**Claude Code** (implementation):
- Reads this file, the specs, and the skills before writing any code
- Implements features from prompts in `docs/features/`
- Updates specs after implementation
- Must pass all build checks before finishing

The cycle: **Cowork plans → writes spec → writes prompt → Code implements → Cowork reviews → repeats.**

## Before You Start Coding

This project is specification-driven. The `docs/` folder is the source of truth for all decisions. Read the relevant spec before writing any code:

| Document | When to read |
|----------|-------------|
| `docs/architecture.md` | Project structure, tech stack, build system |
| `docs/backend.md` | Rust modules, Tauri commands, error handling |
| `docs/frontend.md` | React components, hooks, routing, pages |
| `docs/design-system.md` | Colors, typography, spacing, component variants |
| `docs/data-models.md` | All types (Rust + TypeScript), IPC contract |
| `docs/ci-cd.md` | Build pipeline, releases, auto-updater |

## Feature Specs and Prompts

Feature specifications and coding prompts live in `docs/features/`. Each feature has:
- A spec file (e.g., `001-bluetooth-adapter-detection.md`) — the permanent requirements
- A prompt file (e.g., `001-prompt.md`) — instructions for the coding AI
- Optional refinement prompts (e.g., `002-refinement-2-prompt.md`) — surgical follow-up fixes

When pointed at a prompt file, read it and follow its instructions. The prompt will tell you which specs and skills to read.

## Skills

Project-specific skills live in `.skills/`. Read the relevant skill before starting work in that area:

| Skill | File | Use when |
|-------|------|----------|
| auracle-rust | `.skills/auracle-rust/SKILL.md` | Writing or modifying Rust backend code |
| auracle-frontend | `.skills/auracle-frontend/SKILL.md` | Writing or modifying React/TypeScript code |
| auracle-devices | `.skills/auracle-devices/SKILL.md` | Working on hardware communication (BLE, serial, devices) |
| auracle-spec | `.skills/auracle-spec/SKILL.md` | Planning features, checking spec compliance |

**Read the appropriate skill(s) before writing code.** Most tasks require at least two skills (e.g., a backend change needs both auracle-rust and auracle-devices).

## Architecture Rules

- **Rust owns all domain logic.** Tauri commands are a thin translation layer — no business logic in command handlers.
- **TypeScript mirrors Rust types exactly.** Interfaces in `frontend/src/lib/tauri.ts` match Rust structs from `docs/data-models.md`.
- **Hooks own state and data fetching.** Pages own composition and layout. No data fetching in pages, no JSX in hooks.
- **Failures always surface to the UI.** Never silently swallow errors. Error messages must be specific and actionable.
- **This is a test tool.** Never hide, deduplicate, or sanitise data reported by devices. If firmware sends duplicates or unexpected values, show them exactly as received. Bugs in device firmware are what this tool exists to find.

## Code Quality (non-negotiable)

- `cargo build` must produce zero warnings
- TypeScript strict mode, no `any` types
- No placeholder or lorem ipsum content — all empty/error states must be purposeful
- Dark mode only — use design tokens from CSS custom properties, never raw hex values
- Lucide React icons at 14px or 16px consistently
- Geist Sans for UI text, Geist Mono for technical data (logs, IDs, versions)

## Common Tasks

### Adding a Tauri command (full stack)
1. Rust: define `#[tauri::command] async fn` in `lib.rs`, register in `generate_handler!`
2. TypeScript: add invoke wrapper in `frontend/src/lib/tauri.ts`
3. Hook: consume the wrapper in the appropriate hook in `frontend/src/hooks/`
4. Types: ensure Rust struct and TS interface match per `docs/data-models.md`

### Adding a page
1. Create `frontend/src/pages/MyPage.tsx` with `<Header>` as first child
2. Add route in `frontend/src/App.tsx`
3. Add nav item in `frontend/src/components/layout/Sidebar.tsx`

### Adding a shadcn component
```bash
cd frontend && npx shadcn@latest add [component-name]
```

## Build Commands

```bash
npm run --prefix frontend dev    # Vite dev server on port 1420
npm run --prefix frontend build  # TypeScript check + Vite production build
cargo build                      # Rust backend (from src-tauri/)
cargo tauri dev                  # Full Tauri dev mode (frontend + backend)
```

## Current Status

Features 001 (Bluetooth adapter detection) and 002 (BLE device discovery) are implemented and live. The Devices page is a functional two-panel BLE explorer with real btleplug scanning, company ID resolution from the full Bluetooth SIG database (3,972 entries), and known service UUID mapping including Focal Naim Auracast. Stream config, logging, and device connections remain stubbed.
