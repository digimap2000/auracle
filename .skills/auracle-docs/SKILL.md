---
name: auracle-docs
description: "Confluence documentation maintenance for Auracle. Use this skill when updating the external-facing project documentation on Confluence after a batch of development work. Covers the update cycle: reviewing git history since the last documentation pass, cross-referencing against the Confluence pages, identifying drift, and making corrections. Trigger on: 'update the docs', 'sync Confluence', 'documentation cycle', or when a milestone of commits has landed and the external documentation needs to catch up."
---

# Auracle Documentation Maintenance

This skill defines the process for keeping Auracle's Confluence documentation in sync with the codebase. The Confluence pages are the external-facing view of the project — written for peer developers and technical management. They are not a line-by-line mirror of `docs/` but a curated summary that stays accurate as the project evolves.

## Confluence Location

- **Site**: verventaudio.atlassian.net
- **Cloud ID**: `a8e1f69d-3de6-409a-8c1f-bd7530498f58`
- **Space**: Andrew Shelley's personal space (`~ashelley`, space ID `11337729`)
- **Parent page**: "Auracle — Specifications" (page ID `5861212175`)

### Page inventory

| Page | Page ID | Parent | Covers |
|------|---------|--------|--------|
| Overview | `5859999768` | Parent | Problem statement, target users, system-at-a-glance, current status |
| Architecture | `5860556806` | Parent | System layers, tech stack, data flow, directory layout, key decisions |
| Desktop Application | `5861376017` | Parent | Tauri shell, Rust commands/events, React pages/hooks/components |
| Design System | `5861113876` | Parent | Colour tokens, typography, icons, spacing, component variants |
| Backend Daemon | `5859639315` | Parent | C++ gRPC server, inventory model, providers, probers, CLI |
| Firmware | `5860851753` | Parent | nRF5340 roles/build/flash, ESP32 bridge, hardware |
| Wire Protocol | `5861179413` | Parent | NDJSON message catalogue, transport, upstream/downstream messages |
| Data Models | `5859770386` | Parent | Types across Rust/TypeScript/C++/Protobuf boundaries |
| Build & Development | `5859672079` | Parent | Prerequisites, build commands, dev workflow, quality gates |
| Releases & CI/CD | `5860753421` | Parent | Pipeline, signing, auto-updater, versioning |
| Appendices | `5861507092` | Parent | Container page for metadata children |
| AI Appendix | `5859737628` | Appendices | Document history, image descriptions — machine-maintained metadata |
| Diagrams | `5861736499` | Appendices | PNG uploads for all diagrams — no body text, images only |

## The Update Cycle

A documentation cycle runs after a batch of commits has landed — typically after a feature is complete or a milestone is reached. It is not run after every commit. The cycle has five steps:

### Step 1: Review git history

Identify what changed since the last documentation update. The last update timestamp or commit hash should be noted at the end of each cycle.

```bash
git log --oneline --since="<last-cycle-date>"
```

Or if tracking by commit:

```bash
git log --oneline <last-cycle-commit>..HEAD
```

Categorise each commit by which documentation page(s) it affects:

| Commit touches... | Likely affects page... |
|-------------------|----------------------|
| `frontend/src/pages/`, `frontend/src/components/` | Desktop Application |
| `frontend/src/hooks/` | Desktop Application, Data Models |
| `frontend/src-tauri/src/` | Desktop Application, Data Models |
| `frontend/src/index.css`, design tokens | Design System |
| `backend/` | Backend Daemon |
| `proto/` | Backend Daemon, Data Models |
| `firmware/rf5340/` | Firmware |
| `firmware/esp32/` | Firmware |
| `src/modules/wire.c`, wire protocol changes | Wire Protocol |
| `docs/features/` | Overview (status table), relevant feature page |
| `docs/diagrams/` | AI Appendix (image descriptions/status), affected content pages |
| `.github/workflows/` | Releases & CI/CD |
| `Cargo.toml`, `package.json`, build config | Build & Development |
| `docs/architecture.md` | Architecture |
| `docs/data-models.md` | Data Models |
| Multiple areas, new feature | Overview (status table) |

### Step 2: Re-read affected source files

For each page identified in Step 1, re-read the current state of the relevant source files and `docs/` specs. Do not rely on memory from previous sessions — the codebase may have changed in ways that aren't obvious from commit messages alone.

Key files to re-read per page:

| Page | Source files |
|------|-------------|
| Desktop Application | `frontend/src-tauri/src/lib.rs`, `frontend/src/App.tsx`, `frontend/src/lib/tauri.ts`, pages, hooks |
| Design System | `frontend/src/index.css`, `docs/design-system.md` |
| Backend Daemon | `backend/daemon/main.cpp`, `backend/inventory/`, `backend/rpc/`, `proto/` |
| Firmware | `firmware/rf5340/apps/sample/src/`, Kconfig/overlay files |
| Wire Protocol | `docs/features/005-wire-protocol.md`, firmware wire.c/wire.h |
| Data Models | `docs/data-models.md`, `frontend/src/lib/tauri.ts`, Rust structs |
| Build & Development | `docs/build.md`, `docs/architecture.md`, Cargo.toml, package.json |
| Releases & CI/CD | `.github/workflows/`, `docs/ci-cd.md` |

### Step 3: Identify drift

Compare what the Confluence pages say against what the code actually does. Look for:

- **Stale status markers** — features marked "Stub" that are now live, or vice versa
- **Missing entries** — new commands, pages, hooks, components, message types not listed
- **Removed items** — things listed in docs that no longer exist in code
- **Changed signatures** — command parameters, type fields, or API shapes that evolved
- **New architectural decisions** — tech stack changes, new modules, changed patterns
- **Resource changes** — flash/RAM utilisation, dependency versions, build requirements

### Step 4: Update Confluence pages

Use the `updateConfluencePage` tool with the page IDs from the inventory above. Write updates in markdown format.

**Critical: `updateConfluencePage` requires the full body.** The `body` parameter is mandatory — you cannot update just the title or parent without also providing the complete page content. Always fetch the current page content first with `getConfluencePage`, then send the modified body back. Omitting or placeholder-ing the body will **wipe the page**.

**Rules for updates:**

- **Omit the H1 heading** — the page title is displayed prominently by Confluence. Starting the body with a matching `# Title` creates visible duplication. Begin the page body with the first paragraph or H2 section instead.
- **Match the existing tone** — concise, technical, table-heavy, aimed at developers and tech management. Not tutorial-style, not exhaustive.
- **Update tables in place** — don't append "Update: now X is Y". Rewrite the row to reflect current reality.
- **Maintain status columns** — keep status markers current (Live, Stub, Mock data, Working, Defined, Planned).
- **Image references** — don't remove `[PAGE-N]` placeholders from the body. If a diagram's scope changes, update the description in the AI Appendix page (which flags it for regeneration).
- **Add new sections** if a genuinely new area has appeared (e.g., a new page, a new firmware role, a new provider). Place them in logical order, not at the bottom.
- **Remove dead sections** if something has been fully removed from the project. Don't leave stale documentation.
- **Update the parent page** table of contents if the set of pages has changed.

### Step 5: Record the cycle

After completing updates, append rows to the **Document history** table in the **AI Appendix** page (page ID `5859737628`). Add one row per page that was updated, recording the date, page name, HEAD commit hash, author, and a brief summary. The most recent row for a given page is the reference point for the next cycle.

## When to Add a New Page

Add a new child page under the parent when:

- A genuinely new system layer appears (not just a new feature within an existing layer)
- A topic grows too large for its current page (e.g., if firmware splits into separate nRF and ESP32 concerns)
- A cross-cutting concern deserves standalone treatment (e.g., security, testing strategy)

Don't create pages for individual features — those live in the `docs/features/` folder in the repo. The Confluence pages describe the system as it is, not the history of how it got there.

## Appendices

The **Appendices** page (`5861507092`) is a container under the parent with two children:

- **AI Appendix** (`5859737628`) — machine-maintained metadata (document history, image descriptions)
- **Diagrams** (`5861736499`) — PNG attachments for all diagrams (no body text, images only)

This keeps operational/machine content separate from the human-readable content pages.

## AI Appendix

The **AI Appendix** holds all machine-maintained metadata for the documentation set — document history and image descriptions. Human readers can ignore it entirely; AI agents use it as operational metadata.

### Why a separate page

Individual content pages are written for human readers. Mixing machine metadata into every page creates noise. A single appendix page keeps the content pages clean while centralising all tracking data in one place.

### Structure

The AI Appendix page has three sections:

1. **Document history** — a table tracking every documentation cycle, with one row per page updated. Columns: Date, Page, Commit, Author, Summary.
2. **Image descriptions** — grouped by page, each with a `[PAGE-N]` reference and a full generation description in a blockquote.
3. **Image status** — a summary table listing all images with their current status (Pending, Generated, Outdated).

### Reference ID format

Each image has a unique short reference: `[PAGE-N]` where:
- `PAGE` is a short code for the page (see table below)
- `N` is a sequential number within that page

| Page | Code |
|------|------|
| Overview | `OVR` |
| Architecture | `ARCH` |
| Desktop Application | `APP` |
| Design System | `DS` |
| Backend Daemon | `DAEMON` |
| Firmware | `FW` |
| Wire Protocol | `WIRE` |
| Data Models | `DM` |
| Build & Development | `BUILD` |
| Releases & CI/CD | `CICD` |

### How image references work

In the **content page body**, an image appears in one of two forms:

**Before upload** — a captioned placeholder:
```markdown
> **[ARCH-1]** Layered architecture diagram
```

**After upload** — an embedded image linking to the Diagrams page attachment:
```markdown
![ARCH-1: Layered architecture diagram](https://verventaudio.atlassian.net/wiki/download/attachments/5861736499/ARCH-1-layered-architecture.png?api=v2)
```

In the **AI Appendix page**, the same reference holds the full generation description:

```markdown
**[ARCH-1]** Layered architecture diagram
> Three horizontal bands: Desktop App (top), Backend Daemon (middle),
> Firmware (bottom). ...
```

This separation keeps content pages clean for human readers while preserving enough detail to regenerate or update the image. The short reference (`[ARCH-1]`) is the stable link between the two.

### Rules for image descriptions

- **The description is the source of truth for the image.** If the diagram needs to change, update the description in the AI Appendix page first, then regenerate the image from it.
- **Never delete a description** unless the image is being permanently removed from a content page.
- **Descriptions must be self-contained** — include enough detail that someone (or an image generation tool) could produce the diagram without reading the content page.
- **Keep captions short** — the caption in the content page body (e.g., "Layered architecture diagram") should be 3–6 words. The full description lives in the appendix page.
- **When adding a new image**, assign the next sequential number for that page's code (e.g., if `[ARCH-1]` and `[ARCH-2]` exist, the next is `[ARCH-3]`).

### Document history rules

- **Every documentation cycle** adds rows to the document history table — one row per content page that was updated.
- **Commit** is the HEAD commit hash at the time of the update — this is the reference point for the next cycle.
- **Author** is who performed the update (e.g., `Claude`, or a person's name).
- **Summary** briefly describes what changed (e.g., "Added scan_start command, updated status table").
- The history is append-only. Don't edit or remove previous entries.
- The **most recent row for a given page** tells you when that page was last updated and relative to which commit.

### During an update cycle

When updating content pages (Step 4 of the update cycle), also update the AI Appendix page:

1. **Update image descriptions** if the diagram scope has changed — modify the description text, which flags the image for regeneration.
2. **Add new image entries** if new diagrams are needed — add both the body placeholder on the content page and the description on the AI Appendix page.
3. **Append history rows** with the current date, page name, HEAD commit, author, and a brief summary.
4. **Update the image status table** if images have been added, removed, or regenerated.

### Image generation workflow

Image generation is a separate task from text updates. Diagrams are **proprietary** — they must never be rendered by external services (e.g., mermaid.ink). All rendering happens locally.

#### Diagram source files

Diagram sources live in `docs/diagrams/` in the repo. Choose the format that best suits the diagram:

- **Mermaid** (`.mmd`) — good for flowcharts, sequence diagrams, state machines, and other structured diagrams that Mermaid supports natively.
- **Raw SVG** (`.svg`) — better when you need precise layout control, custom styling, colour swatches, non-standard shapes, or anything Mermaid can't express well. Write the SVG directly — no rendering step needed.

Use your judgement. If a diagram would fight Mermaid's auto-layout or needs visual precision (e.g., colour palettes, component sheets, wireframes with exact dimensions), write raw SVG instead.

Current sources:

```
docs/diagrams/
├── OVR-1-system-context.mmd
├── ARCH-1-layered-architecture.mmd
├── ARCH-2-data-flow.mmd
├── APP-2-layout-wireframe.mmd
├── DAEMON-1-inventory-state-machine.mmd
├── WIRE-1-message-flow.mmd
├── BUILD-1-dev-workflow.mmd
└── CICD-1-pipeline.mmd
```

#### Rendering

**Mermaid sources** — use `@mermaid-js/mermaid-cli` (mmdc) to render locally to both SVG and PNG:

```bash
npx -p @mermaid-js/mermaid-cli mmdc -i docs/diagrams/ARCH-1-layered-architecture.mmd -o docs/diagrams/ARCH-1-layered-architecture.svg -b transparent
npx -p @mermaid-js/mermaid-cli mmdc -i docs/diagrams/ARCH-1-layered-architecture.mmd -o docs/diagrams/ARCH-1-layered-architecture.png -b transparent -s 2
```

The `-s 2` flag doubles PNG resolution for readability.

**Raw SVG sources** — the `.svg` file is the source and the rendered output. To produce a PNG for Confluence upload, convert with a tool like `rsvg-convert` or `cairosvg`:

```bash
rsvg-convert -o docs/diagrams/DS-1-colour-palette.png docs/diagrams/DS-1-colour-palette.svg
```

#### Confluence upload strategy

PNG files are uploaded as attachments to the **Diagrams** page (`5861736499`). This page has no body text — it is purely a container for image attachments. The user uploads the PNGs manually (Claude Code cannot upload attachments without an API token).

#### Cross-page image embedding

Content pages reference images from the Diagrams page using Confluence attachment download URLs:

```markdown
![ARCH-1: Layered architecture diagram](https://verventaudio.atlassian.net/wiki/download/attachments/5861736499/ARCH-1-layered-architecture.png?api=v2)
```

The URL pattern is:
```
https://verventaudio.atlassian.net/wiki/download/attachments/<DIAGRAMS_PAGE_ID>/<FILENAME>.png?api=v2
```

Confluence converts this markdown into an `<ac:image>` storage format element that renders inline.

#### Filenames

Attachment filenames follow the pattern: `<REF>-<slug>.png` where `<REF>` is the image reference ID in lowercase and `<slug>` is a kebab-case description. Examples:

- `OVR-1-system-context.png`
- `ARCH-1-layered-architecture.png`
- `DAEMON-1-inventory-state-machine.png`

#### Placeholder vs embedded

- **Placeholder** (not yet uploaded): `> **[ARCH-1]** Layered architecture diagram` — blockquote with bold reference and caption.
- **Embedded** (uploaded and linked): `![ARCH-1: Layered architecture diagram](https://verventaudio.atlassian.net/wiki/download/attachments/5861736499/ARCH-1-layered-architecture.png?api=v2)` — standard markdown image with download URL.

When a new diagram is generated and uploaded, replace the placeholder line with the embedded image line.

#### Non-Mermaid images

Some images cannot be generated as Mermaid diagrams:
- **APP-1** — requires real application screenshots
- **DS-1, DS-2** — require design system visuals (colour swatches, component sheets)
- **FW-1** — requires a board photo

These stay as placeholders until the assets are created manually and uploaded.

#### Full cycle

1. Update the description in the AI Appendix page if the diagram scope changed.
2. Edit or create the `.mmd` source file in `docs/diagrams/`.
3. Render locally to SVG and PNG.
4. Tell the user to upload the PNG to the Diagrams page on Confluence.
5. Update the content page — replace the placeholder with the embedded image URL.
6. Update the image status table in the AI Appendix (Pending → Generated, or Outdated → Generated).

## Audience

The Confluence pages serve two audiences:

- **Peer developers** joining or collaborating on the project — they need to understand the architecture, find the right source files, and follow established patterns
- **Technical management** tracking progress — they need the status tables, tech stack overview, and high-level data flow

Write for the developer first. Keep management-relevant information (status, decisions, roadmap) in clearly labelled tables so it's scannable without reading every section.

## Relationship to docs/ Folder

The `docs/` folder in the repository is the source of truth for implementation. The Confluence pages are a curated external view. They overlap but are not identical:

| | `docs/` folder | Confluence pages |
|-|----------------|-----------------|
| Audience | AI coding agents + developers in IDE | Developers + management in browser |
| Detail level | Implementation-specific (exact patterns, code examples) | Architectural (what exists, how it connects, current state) |
| Feature specs | Yes — individual feature files | No — features reflected in system-level pages |
| Update trigger | Every commit that changes behaviour | Periodic documentation cycles |
| Format | Markdown in git | Confluence wiki |

When updating Confluence, read the `docs/` specs to understand current truth, then summarise appropriately for the Confluence audience. Don't copy-paste from `docs/` — the framing and level of detail are different.
