# Build Guide

## Prerequisites

- CMake 3.25+
- C++20 compiler (Clang 16+ or GCC 13+)
- Protobuf & gRPC
- Node.js LTS
- Rust stable

## Backend

```bash
cmake -B build -S backend
cmake --build build
```

## Frontend

```bash
npm install --prefix frontend
npm run --prefix frontend build
```

## Full Tauri app

```bash
cargo tauri build
```
