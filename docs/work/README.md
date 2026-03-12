# Auracle — Work Tracker

This directory drives development of Auracle. If you are an AI coding agent, read this file first.

## How this works

`plan.md` contains the release plan: stories, milestones, and numbered work items. Each work item has a status column:

- `—` means the item needs doing
- `▶` means someone is working on it
- `✓` means it is done

## Picking your next task

1. Read `plan.md` and find the **first milestone that has incomplete items** (any item marked `—`).
2. Within that milestone, pick the **lowest-numbered `—` item** — milestones are sequenced so earlier items unblock later ones.
3. Before starting, set the item's status to `▶` in `plan.md`.
4. When finished, set the item's status to `✓` in `plan.md`.

Do not skip ahead to a later milestone while the current one has incomplete work.

## Before writing code

This project is specification-driven. Read the relevant docs before changing anything:

- `docs/architecture.md` — system layers, tech stack, data flow
- `docs/backend.md` — daemon and CLI structure
- `docs/frontend.md` — React components, hooks, pages
- `docs/data-models.md` — types across Rust, TypeScript, C++, Protobuf
- `.skills/` — project-specific coding patterns (read the skill matching your artefact)

The work item's **Artefact** column tells you what you're changing: Daemon, CLI, Desktop App, Firmware, or Docs.

## After finishing a work item

- Ensure the build is clean (`cargo build` zero warnings, `npm run build` zero errors where applicable)
- Update `plan.md` with `✓` status
- If a Docs item: update the relevant spec pages and write/update the user guide
- Commit with a message referencing the work item number (e.g. "Complete 2.3 — decoded service data in scan watch")
