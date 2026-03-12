# Auracle — Release Plan

Artefacts: **Daemon** (`auracle-daemon`) · **CLI** (`auracle`) · **Desktop App** (`Auracle.app`)

Status: `—` todo · `▶` in progress · `✓` done

---

## Story 0 — Build and distribute

### M0 · macOS release pipeline

Developers can install the daemon and CLI on a Mac without a build environment.

| # | Work | Artefact | Status |
|---|------|----------|--------|
| 0.1 | CI job builds daemon and CLI on Mac Studio | CI | — |
| 0.2 | CI uploads universal macOS binaries as GitHub Release assets | CI | — |
| 0.3 | Create private Homebrew tap repo with formula for daemon + CLI | CI | — |
| 0.4 | `brew install auracle` installs daemon and CLI from release assets | CI | — |
| 0.5 | Create Homebrew cask for desktop app (`.app` bundle) | CI | — |
| 0.6 | Update docs and write user guide for installation | Docs | — |

---

## Story 1 — Observe and validate broadcasts

### M1 · Host adapter scan

Daemon discovers the Mac's built-in Bluetooth. CLI lists nearby BLE devices. No special hardware.

| # | Work | Artefact | Status |
|---|------|----------|--------|
| 1.1 | Daemon discovers host BT adapter on startup | Daemon | ✓ |
| 1.2 | Daemon promotes host adapter to scanning unit | Daemon | ✓ |
| 1.3 | CLI `inventory list` shows host adapter as a unit | CLI | ✓ |
| 1.4 | CLI `scan watch` streams live advertisements from host adapter | CLI | ✓ |
| 1.5 | CLI `scan list` shows retained scan results | CLI | ✓ |
| 1.6 | Update docs and write user guide for host adapter scanning | Docs | ✓ |

### M2 · Decoded advertisement data

Push the host adapter path to its natural limit. See a real Auracast DUT, decode its advertising.

| # | Work | Artefact | Status |
|---|------|----------|--------|
| 2.1 | Daemon decodes advertisement service data fields | Daemon | ✓ |
| 2.2 | CLI `scan decode` shows decoded fields for a device | CLI | ✓ |
| 2.3 | CLI `scan watch` includes decoded service data in output | CLI | — |
| 2.4 | CLI JSON output includes all decoded fields | CLI | — |
| 2.5 | Identify and document host adapter limitations for Auracast | Docs | — |
| 2.6 | Update docs and write user guide for decoded advertisement inspection | Docs | — |

### M3 · nRF5340 as scanning unit

Plug in dedicated BLE hardware. Daemon discovers it. CLI scans through it.

| # | Work | Artefact | Status |
|---|------|----------|--------|
| 3.1 | Daemon discovers nRF5340 DK via serial port | Daemon | ✓ |
| 3.2 | Daemon probes and promotes nRF5340 to scanning unit | Daemon | ✓ |
| 3.3 | CLI `inventory list` shows nRF5340 alongside host adapter | CLI | ✓ |
| 3.4 | CLI `scan watch --unit <nrf>` streams adverts from nRF5340 | CLI | — |
| 3.5 | Same decoded output regardless of which unit is scanning | CLI | — |
| 3.6 | Update docs and write user guide for nRF5340 setup and scanning | Docs | — |

### M4 · Compliance testing

Run checks against observed scan data. Pass/fail with spec references. CI-scriptable.

| # | Work | Artefact | Status |
|---|------|----------|--------|
| 4.1 | Define minimum Auracast advertising compliance rules | Daemon | ✓ |
| 4.2 | CLI `compliance run-rule` evaluates one rule against a device | CLI | ✓ |
| 4.3 | CLI `compliance run-suite` runs a full test suite | CLI | ✓ |
| 4.4 | CLI `compliance eval` runs locally without daemon | CLI | ✓ |
| 4.5 | JSON output suitable for CI pass/fail reporting | CLI | — |
| 4.6 | Non-zero exit code on compliance failure | CLI | — |
| 4.7 | Update docs and write user guide for compliance testing and CI integration | Docs | — |

---

## Story 2 — Sink an Auracast stream

### M5 · List available streams

Discover what's being broadcast. Periodic advertising sync via nRF5340.

| # | Work | Artefact | Status |
|---|------|----------|--------|
| 5.1 | Daemon tracks periodic advertising sync state | Daemon | — |
| 5.2 | Daemon reports available Auracast streams (broadcast ID, config) | Daemon | — |
| 5.3 | CLI lists available Auracast streams | CLI | — |
| 5.4 | CLI shows stream metadata (codec, channels, quality) | CLI | — |
| 5.5 | Update docs and write user guide for stream discovery | Docs | — |

### M6 · Connect and play audio

Lock onto a BIS and hear the broadcast.

| # | Work | Artefact | Status |
|---|------|----------|--------|
| 6.1 | Daemon manages BIS stream connection lifecycle | Daemon | — |
| 6.2 | Firmware receives and forwards audio frames | Firmware | — |
| 6.3 | CLI `sink connect` locks onto a stream | CLI | — |
| 6.4 | Audio playback on host machine | Daemon | — |
| 6.5 | CLI `sink disconnect` releases the stream | CLI | — |
| 6.6 | Update docs and write user guide for sinking audio streams | Docs | — |

---

## Story 3 — Source an Auracast broadcast

### M7 · Define and control a broadcast

Configure and run an Auracast source on the nRF5340.

| # | Work | Artefact | Status |
|---|------|----------|--------|
| 7.1 | Define broadcast configuration format (identity, codec, streams) | Docs | — |
| 7.2 | Daemon sends source configuration to firmware | Daemon | — |
| 7.3 | Firmware starts Auracast broadcast | Firmware | — |
| 7.4 | CLI `source start` begins broadcasting | CLI | — |
| 7.5 | CLI `source stop` stops broadcasting | CLI | — |
| 7.6 | CLI `source config` shows/sets broadcast parameters | CLI | — |
| 7.7 | Update docs and write user guide for broadcast generation | Docs | — |

---

## Story 4 — Desktop application

Interactive GUI for all of the above. Captured, not yet broken down.

---

## Notes

- Story 0 must be complete before any other story can be used by developers.
- Stories 1–3 target daemon + CLI only. The desktop app comes after.
- The nRF5340 firmware is a dependency for M3 onwards but is tracked separately.
- macOS first. Windows distribution (Scoop or similar) comes later.
- "Done" status is based on current codebase review — needs verification against real hardware.
