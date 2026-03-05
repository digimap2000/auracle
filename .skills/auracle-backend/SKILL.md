---
name: auracle-backend
description: "C++23 backend development for Auracle. Use this skill whenever writing, modifying, or reviewing C++ code in the backend. Covers modern C++23 idioms, module structure, error handling, ownership semantics, gRPC service definitions, CMake build integration, and cross-platform patterns. Trigger on any backend work: new services, domain logic, device communication, protocol implementations, CMake changes, or gRPC schema updates."
---

# Auracle C++ Backend

This skill encodes the patterns and constraints for all C++ backend work in Auracle. The backend is the primary domain logic layer. Rust/Tauri remains the desktop shell and frontend bridge — it calls into the C++ backend over gRPC. The frontend specs in `docs/` remain the source of truth for the IPC contract visible to the UI.

## Role Boundaries

```
React (UI) → Tauri invoke → Rust (thin gRPC client) → gRPC → C++ backend (domain logic)
```

Rust is a **transport adapter**. It translates Tauri invocations into gRPC calls and relays results. All business logic, device communication, protocol parsing, and state management live in the C++ backend. If you're tempted to put logic in Rust, stop — it belongs here.

The C++ backend runs as a **standalone process** managed by the Tauri app. It starts when the app launches, communicates over a local gRPC channel, and shuts down when the app exits.

## Language Standard and Toolchain

- **Standard**: C++23. Use the full feature set — `std::expected`, `std::format`, `std::ranges`, `std::print`, structured bindings, `auto` return types, concepts, coroutines. Write code that reads like it was written in 2025, not 2015.
- **Compiler targets**: Clang 17+ (macOS, Linux), MSVC 19.38+ (Windows). GCC is not a primary target but should compile cleanly.
- **Build system**: CMake 3.28+ with presets. The backend builds as a standalone executable that the Tauri app spawns.

## DTS Library (first-choice utility library)

DTS is our internal C++ utility library, installed at `/opt/dts`. It is the **first choice** for any commonly needed functionality not natively available in the C++ standard library. Before reaching for a third-party dependency, writing your own utility, or working around a gap in `std`, check whether DTS already provides it.

```
/opt/dts/
├── docs/       # Library documentation
├── include/    # Public headers
└── lib/        # Compiled libraries
```

**Before writing any backend code, read the DTS headers in `/opt/dts/include/`** to understand what's available. Browse the docs in `/opt/dts/docs/` for usage guidance and design rationale.

### Integration

DTS is linked via CMake using `find_package` or direct path:

```cmake
# In CMakeLists.txt
set(DTS_ROOT "/opt/dts")
target_include_directories(auracle_backend PRIVATE ${DTS_ROOT}/include)
target_link_directories(auracle_backend PRIVATE ${DTS_ROOT}/lib)
target_link_libraries(auracle_backend PRIVATE dts)  # adjust library name per DTS docs
```

### Decision Order for Utilities

When you need functionality beyond what you're writing from scratch as domain logic, follow this order:

1. **C++23 standard library** — if `std` provides it, use `std`
2. **DTS** — if `std` doesn't cover it, check DTS next. Read `/opt/dts/include/` headers
3. **Third-party library** — only if neither `std` nor DTS provides it, and only via `FetchContent` or `find_package` with a pinned version

Never duplicate functionality that DTS already provides. If you find yourself writing a general-purpose utility (string helpers, platform abstractions, container adapters, serialisation helpers, etc.), stop and check DTS first.

## Project Layout

```
backend/
├── CMakeLists.txt              # Root build — targets, options, dependencies
├── CMakePresets.json           # Configure/build/test presets per platform
├── proto/
│   └── auracle.proto           # gRPC service + message definitions
├── src/
│   ├── main.cpp                # Entry point: start gRPC server, signal handling
│   ├── server/
│   │   ├── auracle_service.h   # gRPC service implementation header
│   │   └── auracle_service.cpp
│   ├── ble/
│   │   ├── adapter.h           # Bluetooth adapter abstraction
│   │   ├── adapter.cpp
│   │   ├── scanner.h           # BLE scan orchestration
│   │   └── scanner.cpp
│   ├── devices/
│   │   ├── device.h            # Device concept/interface
│   │   ├── registry.h          # Device lifecycle management
│   │   ├── registry.cpp
│   │   └── nrf5340/
│   │       ├── nrf5340.h
│   │       └── nrf5340.cpp
│   ├── serial/
│   │   ├── port.h              # Serial port abstraction
│   │   └── port.cpp
│   ├── audio/
│   │   ├── stream_config.h     # Auracast stream configuration
│   │   └── stream_config.cpp
│   └── core/
│       ├── error.h             # Error types (std::expected based)
│       ├── types.h             # Shared value types
│       ├── log.h               # Structured logging
│       └── platform.h          # Platform detection and abstractions
├── include/
│   └── auracle/                # Public headers (if building as library)
├── tests/
│   ├── CMakeLists.txt
│   ├── ble_test.cpp
│   ├── registry_test.cpp
│   └── ...
└── third_party/
    └── CMakeLists.txt          # FetchContent / find_package for dependencies
```

When a source file grows beyond ~300 lines, split it. One type or concept per header. Implementation files mirror their headers exactly.

## Design Philosophy

### Ownership Is Explicit

Every resource has exactly one owner. Express this in the type system:

```cpp
// Sole ownership — the registry owns devices
std::unordered_map<std::string, std::unique_ptr<Device>> devices_;

// Shared observation — scoped, never outlives the owner
Device& device = registry.get(id);  // reference, not pointer

// Transfer — moves are explicit and intentional
registry.add(id, std::move(device));
```

Never use raw `new`/`delete`. Never use `std::shared_ptr` unless you can articulate why shared ownership is the correct semantic — not just convenient. If you reach for `shared_ptr` out of habit, redesign the ownership graph.

### Boundaries Are Types

Domain boundaries are enforced by the type system, not by convention or documentation.

```cpp
// Strong types for identifiers — no stringly-typed interfaces
struct DeviceId { std::string value; };
struct AdapterId { std::string value; };
struct SessionId { std::uint64_t value; };

// A function that accepts DeviceId cannot accidentally receive an AdapterId
auto get_device(DeviceId id) -> std::expected<Device&, Error>;
```

Prefer `enum class` over bool parameters. `start_scan(true, false)` is unreadable. `start_scan(ScanMode::active, DuplicateFilter::off)` is self-documenting.

### Seams Are Concepts

Use C++20/23 concepts to define behavioural boundaries (seams) between subsystems. Concepts replace inheritance hierarchies for polymorphism that doesn't need runtime dispatch.

```cpp
template<typename T>
concept Scannable = requires(T t, ScanConfig config) {
    { t.start(config) } -> std::same_as<std::expected<void, Error>>;
    { t.stop() } -> std::same_as<void>;
    { t.results() } -> std::ranges::range;
};

// Any type satisfying Scannable works — no vtable, no base class
template<Scannable S>
class ScanOrchestrator {
    S scanner_;
    // ...
};
```

Reserve virtual dispatch (`abstract class` with pure virtuals) for cases where you genuinely need runtime polymorphism — e.g., a device registry holding heterogeneous device types. Even then, prefer a `std::variant` of concrete types if the set is closed and known at compile time.

### Guardrails Are Compile-Time

Catch errors at compile time, not at runtime. Prefer:

- `static_assert` over runtime assertions for invariants knowable at compile time
- `constexpr` / `consteval` for values computable at compile time
- Concepts over SFINAE or unconstrained templates
- `std::expected<T, E>` over exceptions for recoverable errors
- `[[nodiscard]]` on every function that returns a value the caller must inspect

```cpp
[[nodiscard]] auto connect(DeviceId id) -> std::expected<Connection, Error>;

// Caller MUST handle the result — compiler warns if they ignore it
auto conn = connect(device_id);
if (!conn) {
    log::error("connection failed: {}", conn.error().message());
    return std::unexpected(conn.error());
}
```

## Error Handling

Use `std::expected<T, Error>` for all fallible operations. Exceptions are reserved for truly exceptional, unrecoverable situations (out of memory, invariant violation). Normal failure paths — device not found, connection refused, timeout — use `expected`.

```cpp
// Error type hierarchy
enum class ErrorKind {
    not_found,
    connection_failed,
    timeout,
    protocol_error,
    permission_denied,
    invalid_argument,
    internal,
};

struct Error {
    ErrorKind kind;
    std::string message;     // Human-readable, specific, actionable
    std::source_location loc; // Where it originated

    static auto make(ErrorKind k, std::string msg,
                     std::source_location loc = std::source_location::current())
        -> Error {
        return {k, std::move(msg), loc};
    }
};

// Convenience
auto not_found(std::string msg) -> std::unexpected<Error> {
    return std::unexpected(Error::make(ErrorKind::not_found, std::move(msg)));
}
```

Error messages must be **specific and actionable**. "Failed" is useless. "Bluetooth adapter not found — verify Bluetooth is enabled in system settings" tells the operator what happened and what to do. These messages propagate through gRPC to the frontend and are displayed to users verbatim.

### Propagation

Use a macro or helper for early-return on error — keep the happy path clean:

```cpp
#define TRY(expr)                          \
    ({                                      \
        auto _result = (expr);              \
        if (!_result) return std::unexpected(_result.error()); \
        std::move(*_result);                \
    })

// Usage
auto do_thing() -> std::expected<Result, Error> {
    auto device = TRY(registry.get(id));
    auto conn = TRY(device.connect());
    return conn.query_config();
}
```

If your compiler supports P2561 (monadic operations on `expected`), prefer `and_then`/`transform` chaining over the macro.

## gRPC Service Layer

### Proto Definitions

The `.proto` files in `proto/` define the contract between the Rust/Tauri shell and the C++ backend. They are the **source of truth** for the IPC boundary.

```protobuf
syntax = "proto3";
package auracle;

service Auracle {
    // Device discovery
    rpc StartBleScan(ScanRequest) returns (stream BleDevice);
    rpc StopBleScan(Empty) returns (Empty);
    rpc GetBluetoothAdapter(Empty) returns (AdapterInfo);

    // Device management
    rpc ConnectDevice(DeviceId) returns (ConnectionInfo);
    rpc DisconnectDevice(DeviceId) returns (Empty);
    rpc ListDevices(Empty) returns (DeviceList);

    // Streaming
    rpc GetStreamConfig(DeviceId) returns (StreamConfig);
    rpc SetStreamConfig(StreamConfigRequest) returns (StreamConfig);
}
```

Design principles for the proto schema:

- **Streaming RPCs** for real-time data (BLE scan results, log output, device status changes). The Rust side maps these to Tauri events.
- **Unary RPCs** for request/response patterns (connect, configure, query).
- **Message types mirror domain types**, not UI types. The frontend may transform them; the proto represents the backend's view of the world.
- **Never use `string` for enums** in proto — use proto `enum` types and map them to C++ `enum class`.

### Service Implementation

```cpp
class AuracleServiceImpl final : public auracle::Auracle::Service {
public:
    explicit AuracleServiceImpl(DeviceRegistry& registry, BleScanner& scanner);

    grpc::Status StartBleScan(
        grpc::ServerContext* ctx,
        const auracle::ScanRequest* request,
        grpc::ServerWriter<auracle::BleDevice>* writer) override;

    // ... other RPCs
private:
    DeviceRegistry& registry_;
    BleScanner& scanner_;
};
```

The service implementation is a **thin translation layer** (same principle as Tauri commands). It converts proto messages to domain types, delegates to domain modules, and converts results back to proto messages. No business logic in RPC handlers.

## Concurrency

### Threading Model

The gRPC server runs on its own thread pool. Domain operations that touch hardware (BLE, serial) run on dedicated threads or use the platform's async mechanisms. Shared state is protected explicitly.

```cpp
// Prefer std::jthread for managed threads
std::jthread scan_thread_([this](std::stop_token token) {
    while (!token.stop_requested()) {
        auto results = scanner_.poll();
        // process results
    }
});

// Cooperative cancellation via stop_token — no force-killing threads
```

### Shared State

Use the narrowest synchronisation primitive that works:

```cpp
// Read-heavy, write-rare: std::shared_mutex
mutable std::shared_mutex mutex_;
std::unordered_map<std::string, DeviceState> devices_;

auto get(const DeviceId& id) const -> std::expected<DeviceState, Error> {
    std::shared_lock lock(mutex_);
    auto it = devices_.find(id.value);
    if (it == devices_.end()) return not_found("device not registered");
    return it->second;
}

auto update(const DeviceId& id, DeviceState state) -> void {
    std::unique_lock lock(mutex_);
    devices_[id.value] = std::move(state);
}
```

Minimise lock scope. Acquire, do the work, release. Never hold a lock while doing I/O or calling into another subsystem — that's a deadlock waiting to happen.

### Async Patterns

For operations that are naturally asynchronous (network, BLE events), use coroutines where gRPC's async API supports them, or callbacks composed through typed channels:

```cpp
// Typed channel for cross-thread communication
template<typename T>
class Channel {
public:
    auto send(T value) -> void;
    auto receive() -> std::optional<T>;          // non-blocking
    auto receive_blocking() -> T;                // blocking
    auto receive_for(std::chrono::milliseconds timeout) -> std::optional<T>;
};
```

## Platform Abstraction

Cross-platform code uses a compile-time platform layer. Platform-specific implementations are isolated behind a common interface, selected at build time — not at runtime.

```cpp
// core/platform.h
namespace platform {
    constexpr bool is_macos =
    #ifdef __APPLE__
        true;
    #else
        false;
    #endif

    constexpr bool is_windows =
    #ifdef _WIN32
        true;
    #else
        false;
    #endif
}

// ble/adapter.h — platform-dispatched at build time via CMake
// src/ble/adapter_macos.cpp  — CoreBluetooth implementation
// src/ble/adapter_windows.cpp — WinRT implementation
// src/ble/adapter_linux.cpp   — BlueZ/D-Bus implementation
```

CMake selects the correct platform source:

```cmake
if(APPLE)
    target_sources(auracle_backend PRIVATE src/ble/adapter_macos.cpp)
    target_link_libraries(auracle_backend PRIVATE "-framework CoreBluetooth")
elseif(WIN32)
    target_sources(auracle_backend PRIVATE src/ble/adapter_windows.cpp)
elseif(UNIX)
    target_sources(auracle_backend PRIVATE src/ble/adapter_linux.cpp)
endif()
```

Never use `#ifdef` scattered through business logic. Platform divergence is contained in platform-specific source files behind a shared header.

## CMake Patterns

### Root CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.28)
project(auracle_backend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Warnings are errors in CI
option(AURACLE_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

add_executable(auracle_backend)
target_sources(auracle_backend PRIVATE
    src/main.cpp
    src/server/auracle_service.cpp
    # ... per-module sources
)
target_include_directories(auracle_backend PRIVATE src)

# Aggressive warnings
target_compile_options(auracle_backend PRIVATE
    $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion
        -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual
    >
    $<$<CXX_COMPILER_ID:MSVC>:
        /W4 /permissive-
    >
)

if(AURACLE_WARNINGS_AS_ERRORS)
    target_compile_options(auracle_backend PRIVATE
        $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Werror>
        $<$<CXX_COMPILER_ID:MSVC>:/WX>
    )
endif()

# Dependencies
include(third_party/CMakeLists.txt)
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

# Tests
enable_testing()
add_subdirectory(tests)
```

### Dependency Management

The dependency resolution order is: **std → DTS → third-party**. DTS (`/opt/dts`) covers common utility gaps and must be checked before adding any new third-party dependency. See the DTS Library section above.

For third-party dependencies not covered by `std` or DTS, use `FetchContent` with pinned versions — never track `main`.

```cmake
include(FetchContent)

FetchContent_Declare(
    grpc
    GIT_REPOSITORY https://github.com/grpc/grpc
    GIT_TAG        v1.62.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(grpc)
```

Prefer `find_package` when a system-installed version is available (e.g., Protobuf, gRPC via vcpkg or Homebrew). Fall back to `FetchContent` for reproducibility in CI.

### Presets

Use `CMakePresets.json` for consistent builds across developers and CI:

```json
{
    "version": 6,
    "configurePresets": [
        {
            "name": "dev",
            "binaryDir": "build/dev",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Debug",
                "AURACLE_WARNINGS_AS_ERRORS": "OFF"
            }
        },
        {
            "name": "ci",
            "binaryDir": "build/ci",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Release",
                "AURACLE_WARNINGS_AS_ERRORS": "ON"
            }
        }
    ]
}
```

## Testing

Use Google Test for unit tests. Tests live in `tests/` and mirror the source structure.

```cpp
#include <gtest/gtest.h>
#include "devices/registry.h"

TEST(DeviceRegistryTest, AddAndRetrieve) {
    DeviceRegistry registry;
    auto device = std::make_unique<MockDevice>("test-001");

    registry.add(DeviceId{"test-001"}, std::move(device));

    auto result = registry.get(DeviceId{"test-001"});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->id().value, "test-001");
}

TEST(DeviceRegistryTest, GetUnknownDeviceReturnsNotFound) {
    DeviceRegistry registry;
    auto result = registry.get(DeviceId{"nonexistent"});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, ErrorKind::not_found);
}
```

Test the contract, not the implementation. If refactoring internals breaks your tests, the tests were testing the wrong thing.

### Testing Seams

Design for testability by injecting dependencies through constructor parameters or template arguments:

```cpp
// Production: real BLE scanner
ScanOrchestrator<BleScanner> orchestrator(real_scanner);

// Test: mock scanner satisfying the same concept
ScanOrchestrator<MockScanner> orchestrator(mock_scanner);
```

Concepts make this natural — any type satisfying the concept works. No factory patterns, no dependency injection frameworks.

## Logging

Use structured logging with severity levels. Log entries propagate through gRPC streaming to the frontend log view.

```cpp
namespace log {
    enum class Level { trace, debug, info, warn, error };

    void write(Level level, std::string_view message,
               std::source_location loc = std::source_location::current());

    // Convenience
    void info(std::string_view msg);
    void error(std::string_view msg);
    void debug(std::string_view msg);
}

// Usage
log::info(std::format("scan started on adapter {}", adapter_id.value));
log::error(std::format("connection to {} failed: {}", device_id.value, err.message));
```

Log messages must include enough context to diagnose problems from the log alone — device IDs, adapter IDs, error details. Never log "error occurred" without saying which error, where, and involving what.

## Code Quality (non-negotiable)

- **Zero warnings** under the project's warning flags (`-Wall -Wextra -Wpedantic` or `/W4`)
- **No raw owning pointers** — use `unique_ptr`, `shared_ptr` (sparingly), or value semantics
- **No C-style casts** — use `static_cast`, `dynamic_cast`, `std::bit_cast` as appropriate
- **No `using namespace std;`** in headers, ever. In `.cpp` files, scope it to the narrowest block
- **`[[nodiscard]]`** on every function returning a value
- **`const` by default** — parameters, member functions, local variables. Mutability is the exception
- **Pass by `std::string_view`** for read-only string parameters, never `const std::string&`
- **Initialise everything** — no uninitialised variables, use designated initialisers where possible
- **Prefer algorithms and ranges** over hand-written loops. `std::ranges::find_if` over `for` with `break`
- **Include what you use** — no transitive include dependencies. Each file includes exactly what it needs

## Naming Conventions

| Element | Style | Example |
|---------|-------|---------|
| Types (classes, structs, enums) | PascalCase | `DeviceRegistry`, `ScanConfig` |
| Functions and methods | snake_case | `start_scan()`, `get_device()` |
| Variables and parameters | snake_case | `device_id`, `scan_results` |
| Constants and enum values | snake_case | `max_retry_count`, `ErrorKind::not_found` |
| Macros (avoid if possible) | SCREAMING_SNAKE | `AURACLE_TRY` |
| Namespaces | snake_case | `auracle::ble`, `auracle::devices` |
| Private members | trailing underscore | `devices_`, `mutex_` |
| Template parameters | PascalCase | `template<Scannable S>` |
| File names | snake_case | `device_registry.h`, `ble_scanner.cpp` |

## Header Conventions

```cpp
#pragma once  // All headers use pragma once

#include <expected>       // Standard library — angle brackets, sorted
#include <string>
#include <vector>

#include "core/error.h"   // Project headers — quotes, sorted
#include "core/types.h"

namespace auracle::devices {

// One primary type per header
class DeviceRegistry {
public:
    // Public interface first
    [[nodiscard]] auto get(DeviceId id) const -> std::expected<const Device&, Error>;
    auto add(DeviceId id, std::unique_ptr<Device> device) -> void;
    auto remove(DeviceId id) -> std::expected<void, Error>;

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<Device>> devices_;
};

} // namespace auracle::devices
```

## DRY Principle

If the same non-trivial logic appears in two places, extract it immediately. This applies at every level:

- **Functions**: shared conversion logic, repeated validation, common formatting
- **Types**: if two modules define similar data carriers, unify them in `core/types.h`
- **Concepts**: if two templates constrain their parameters the same way, name the concept
- **Proto messages**: shared field groups become nested messages, not copy-pasted fields

The cost of an abstraction is one indirection. The cost of duplication is every future change applied twice, incorrectly.

## Test Tool Principle

Auracle is a test harness for firmware developers. **Never hide, deduplicate, or sanitise data reported by devices.** If a device advertises the same service UUID five times, report it five times over gRPC. If manufacturer data contains unexpected values, pass them through unchanged. Bugs in device firmware are what this tool exists to find.

## Reference

For DTS utility library headers and docs, see `/opt/dts/include/` and `/opt/dts/docs/`. **Read these before writing utility code.**
For the gRPC service contract, see `backend/proto/auracle.proto`.
For frontend integration patterns, see `docs/frontend.md` and the `auracle-frontend` skill.
For the Rust/Tauri bridge layer, see `docs/backend.md` and the `auracle-rust` skill.
For device-specific protocols and hardware details, see the `auracle-devices` skill.
