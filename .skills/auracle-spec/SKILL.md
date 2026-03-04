---
name: auracle-spec
description: "Specification-driven development for Auracle. Use this skill before starting any implementation work, when reviewing code for spec compliance, when planning a new feature, or when checking whether the codebase has drifted from its specifications. The docs/ folder is the source of truth for all development decisions. Trigger on: feature planning, implementation review, architecture questions, 'does the code match the spec', or any uncertainty about how something should work."
---

# Auracle Spec-Driven Development

The `docs/` folder is the single source of truth for Auracle. Every implementation decision should trace back to a specification. When a spec and the code disagree, the spec wins (unless the spec itself needs updating, which should be an explicit, deliberate act).

## The Specification Documents

| Document | Covers | Read when... |
|----------|--------|-------------|
| `docs/architecture.md` | Tech stack, directory structure, build system, window config, plugins, capabilities | Starting any work, adding dependencies, changing project structure |
| `docs/backend.md` | Rust module map, command signatures, error handling, plugin registration, patterns | Writing or modifying Rust code |
| `docs/frontend.md` | React entry point, routing, pages, layout components, hooks, UI components, patterns | Writing or modifying TypeScript/React code |
| `docs/design-system.md` | CSS architecture, color tokens, typography, spacing, component variants | Any UI/styling work |
| `docs/data-models.md` | All types (Rust + TypeScript), trait definitions, IPC contract, frontend-only types | Adding or changing data structures |
| `docs/ci-cd.md` | GitHub Actions workflow, build matrix, release process, auto-updater, code signing | Release or deployment work |

## Writing Specs

Specs are concrete, concise statements of what is required. They describe the target state — what the thing *is* — not what changed or what was removed. A reader coming to the spec fresh should understand the design without needing to know its history.

**State what it is, not what it isn't.** Say "Slack-style filled pill highlight" not "Slack-style filled pill highlight, no left-edge border". The coder reads the spec and builds what's described. If something isn't mentioned, it doesn't exist.

**Be descriptive, not prescriptive.** Specs describe intent and constraints. They are not pseudocode in plain English. Say "subtle filled background, rounded" not "`bg-secondary/50 rounded-md px-2 py-1.5`". The coder picks the implementation that satisfies the description.

**Keep them stable.** A spec should read cleanly at any point in time. When requirements change, rewrite the relevant section to reflect the new reality. Don't leave revision history or "previously we had X" commentary in the spec itself.

## Before Starting Implementation

Every feature or change should follow this sequence:

1. **Read the relevant specs** — identify which docs cover the area you're about to change
2. **Check current state** — read the actual source files to understand what's already built vs stubbed
3. **Identify the gap** — what does the spec say should exist that doesn't yet?
4. **Plan the change** — list the files that need to change, in order
5. **Implement** — following the patterns described in the specs
6. **Verify** — check that the implementation matches the spec

## Spec Compliance Checklist

Use this when reviewing any implementation:

### Backend (Rust)
- [ ] Types match `docs/data-models.md` exactly (field names, types, derives)
- [ ] Commands follow the pattern in `docs/backend.md` (async, proper return types)
- [ ] Commands are registered in `generate_handler!`
- [ ] Errors use `AuracleError` variants from `docs/data-models.md`
- [ ] No business logic in command handlers — delegation only
- [ ] Zero compiler warnings
- [ ] Error messages are specific and actionable

### Frontend (TypeScript/React)
- [ ] TypeScript interfaces mirror Rust structs per `docs/data-models.md`
- [ ] Invoke wrappers exist in `src/lib/tauri.ts` for every command
- [ ] Hooks own state and data fetching, pages own composition
- [ ] No `any` types
- [ ] Design tokens from `docs/design-system.md` used (no raw hex values)
- [ ] Correct font family (sans vs mono) per context
- [ ] Icon sizes consistent (14px or 16px)
- [ ] Empty and error states are purposeful, not placeholder

### Cross-cutting
- [ ] IPC contract matches between frontend and backend per `docs/data-models.md`
- [ ] New capabilities registered in `src-tauri/capabilities/default.json` if needed
- [ ] Failures surface to the UI — never silent

## When the Spec Doesn't Cover Something

Sometimes you'll need to implement something the specs don't explicitly address. In that order:

1. **Infer from existing patterns** — the specs establish conventions. Follow them.
2. **Check the project brief** (provided to you as context) for high-level guidance.
3. **Ask** — if genuinely ambiguous, ask before implementing.
4. **Update the spec** — if you implement something new, the spec should be updated to reflect it. This keeps the docs folder evergreen.

## Updating Specifications

When the implementation legitimately needs to diverge from the spec (new feature, better approach discovered, requirement change):

1. Make the change in the relevant `docs/*.md` file **in the same commit** as the code change
2. Update all affected docs — a change to data models might require updates to `data-models.md`, `backend.md`, and `frontend.md`
3. Keep the same documentation style: tables for structured data, code blocks for examples, clear status markers

## Status Markers

The specs use status markers to track implementation state. Maintain these as you work:

- **Status: UI complete** — visual implementation done, not wired to backend
- **Status: Stub** — function exists but returns mock/hardcoded data
- **Status: Returns mock data** — same as stub, emphasising the fake data aspect
- **Status: Complete** — fully implemented and verified

When you complete a feature, update its status marker in the relevant spec document.

## Feature Implementation Order

Features are specified and tracked in `docs/features/`. Each has a spec, a coding prompt, and optional refinement prompts.

| # | Feature | Status |
|---|---------|--------|
| 001 | Bluetooth adapter detection | Complete |
| 002 | BLE device discovery | Complete |
| 003+ | TBD | Not started |

Remaining areas to implement (order TBD):
- Serial port enumeration
- Device connection lifecycle
- Stream configuration and launch
- Session logging
- Preset management

Each feature should be fully specified, implemented, and verified before moving to the next. "Fully" means both backend and frontend, with the IPC contract working end-to-end.

## Test Tool Principle

Auracle is a test harness for firmware developers. Specifications must never require hiding, deduplicating, or sanitising data reported by devices. If a device advertises unexpected or duplicate data, the spec should require it to be displayed faithfully. Bugs in device firmware are what this tool exists to find.
