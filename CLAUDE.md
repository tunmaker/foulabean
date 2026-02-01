# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**foulaBean** (فولة و تقسمت على إثنين) is an open-source electro-mechanical machine simulation platform - a cross-platform desktop application for creating digital twins to simulate embedded devices and protocols. The platform provides a user-friendly interface for developers to build, test, and integrate custom device models with minimal overhead.

**Key Goals**:
- User-friendly interface for custom device models
- Lightweight footprint with minimal overhead
- Soft real-time simulations with configurable tick rates
- Seamless CI/CD integration using headless CLI runners

## Core Architecture

The platform consists of five key components:

1. **Core Simulation Engine**: C++ soft real-time loop handling simulation logic using Renode as backend for embedded systems
2. **Qt GUI**: Qt-based QML desktop application with live pin dashboard and 3D model importer for interactive device visualization
3. **Protocol Adapters**: Transparently stream and read sensor data between backend and Renode
4. **Interfaces**: ETH, CAN, and other protocol exposure from backend to simulated components
5. **Scripting/API**: Embed Python (pybind11) or Lua for test scenarios and automation scripts

## Technology Stack

- **Language**: C++20
- **GUI Framework**: Qt6 (6.8+, Quick/QML module)
- **Build System**: CMake (3.16+) with Ninja generator
- **Dependencies**: libzmq + cppzmq (via vcpkg), Renode simulator (external)
- **Platforms**: Cross-platform desktop (Linux, macOS, Windows)

## Project Structure

```
src/
├── main/              # Qt6 Quick application (appdigitwin executable)
│   ├── src/main.cpp   # Application entry point
│   └── qml/           # QML UI components (pages, components)
└── renodeAPI/         # Renode external control client (static library)
    ├── include/       # Public API headers
    ├── src/           # Implementation files
    └── renodeTestScripts/  # Renode test configurations (.resc files)
```

## Build & Development Commands

### Build System

```bash
# Configure build from repository root
cmake -S src -B build -G Ninja

# Build project
cmake --build build

# Or use ninja directly
ninja -C build
```

### Dependencies

The project uses **vcpkg** for dependency management. Dependencies are automatically installed during CMake configuration.

Current dependencies:
- `cppzmq` - C++ bindings for ZeroMQ messaging

### Running with Renode

The renodeAPI requires a running Renode instance with external control server:

```bash
# Start Renode with test script
renode src/renodeAPI/renodeTestScripts/test-machine.resc

# Or manually in Renode monitor:
(monitor) logLevel 0
(monitor) emulation CreateExternalControlServer "srv" 5555
```

Test script creates an STM32H753 ARM Cortex-M7 platform on port 5555.

### VSCode Debugging

Two debug configurations are available:
- **"Debug Qt Application with cppdbg"**: Standard C++ debugging (gdb/lldb)
- **"Qt Debug with QML debugger"**: Full QML debugging on port 1234

## renodeAPI Module Architecture

The renodeAPI provides a C++ wrapper around Renode's external control protocol for programmatic device simulation.

### Design Pattern: Pimpl (Pointer to Implementation)

All public classes use the Pimpl idiom (`std::unique_ptr<Impl>`) to:
- Hide implementation details and reduce header dependencies
- Enable ABI stability across library versions
- Provide clean separation between interface and network protocol

### Key Classes

- **`ExternalControlClient`**: Connection manager to Renode server
  - Move-only (unique_ptr ownership)
  - Manages socket connection lifecycle
  - Caches machines as `std::weak_ptr` to prevent duplicates

- **`AMachine`**: Virtual device instance in Renode
  - Shared ownership (shared_ptr, reference-counted)
  - Provides simulation control: `runFor()`, `runUntil()`, `stepInstructions()`
  - Vends peripheral abstractions on-demand

- **Peripheral Classes**: `Adc`, `Gpio`, `SysBus`, `BusContext`
  - Abstractions over Renode peripherals
  - Created fresh on each request (caller responsible for caching if needed)

### Ownership Model

```
ExternalControlClient (unique_ptr)
  └── vends shared_ptr<AMachine>
      └── vends shared_ptr<Adc>, shared_ptr<Gpio>, shared_ptr<SysBus>
          └── SysBus vends shared_ptr<BusContext>
```

### Error Handling: Two-Tier Approach

1. **Non-throwing (preferred for latency-sensitive paths)**:
   - Return `Error` struct (code + message) or `Result<T>` wrapper
   - Methods marked `noexcept` with error out-parameter
   - Example: `getMachine(name, err)` returns nullable shared_ptr

2. **Throwing (for fatal errors)**:
   - `RenodeException` for unrecoverable scenarios
   - Example: `connect()` throws on socket failure

### Thread Safety

- `ExternalControlClient`: Uses `std::mutex` to protect connection state and machine cache
- **Peripheral objects**: Assume single-threaded usage or require external synchronization

### Current State (Branch: renodeExternalInterfaceZmq)

The API is **actively under development**:
- Migrating from raw POSIX sockets to ZeroMQ for improved robustness and async support
- Some async operations return `std::future<Error>` but have placeholder implementations
- Machine caching via `std::map<std::string, std::weak_ptr<AMachine>>`
- ZMQ dependency declared in CMake but socket integration still in progress

## MVP Development Priorities

Based on [MVP.md](MVP.md):

**High Priority**:
- Core simulation engine with soft real-time loop and configurable tick rates
- Qt GUI with live pin dashboard and 3D model interaction
- Thread safety and security (especially Qt cross-thread communication via `Qt::QueuedConnection`)

**Medium Priority**:
- JSON device import/export (Renode schema v0.1 compatibility)
- Headless CLI for CI/CD pipeline integration
- Plugin system for extensibility and third-party device models

## Key Signal Types

The platform supports multiple signal types for device simulation:
- Analog, Digital, PWM
- UART, CAN, ETH protocols

## Testing

**Current Approach**: Renode integration testing
- Test scripts located in `renodeAPI/renodeTestScripts/`
- Example: `test-machine.resc` configures STM32H753 platform with external control server

**No formal unit test framework** is currently configured. Consider adding Google Test or Catch2 for unit testing C++ components.

## Development Notes

- **C++ Standard**: C++20 required
- **Modern C++ patterns**: Heavy use of smart pointers, RAII, STL containers (`std::optional`, `std::vector`, `std::map`)
- **No linting/formatting configured yet**: Consider adding clang-format and clang-tidy for code consistency
- **VSCode Extensions**: Qt C++ Pack, C++ Tools Themes recommended (see [.vscode/extensions.json](.vscode/extensions.json))

## Git Workflow

- **Main branch**: `main`
- **Feature branches**: Used for component development (e.g., `renodeExternalInterfaceZmq`)
- **Commit style**: Descriptive messages focused on functionality (e.g., "getMachine OK", "Use Pimpl", "handshake ok")
