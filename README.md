# Auracle

Professional desktop test harness for Auracast LE Audio broadcast streams.

Built with a C++ backend (CMake) and a Tauri 2 / React 18 frontend.

## Repository layout

```
proto/          Protocol Buffer definitions
backend/        C++ daemon, inventory library, gRPC service, CLI
frontend/       Tauri 2 + React 18 desktop UI
src-tauri/      Tauri Rust shell (bridges frontend ↔ backend)
docs/           Specifications and architecture docs
```

## Building

See [docs/build.md](docs/build.md) for full build instructions.
