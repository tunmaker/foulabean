
# foulaBean MVP — Features & Progress

> Last updated: 2026-02-15

---

## Progress Summary

| Section | Progress | Status |
|---------|----------|--------|
| 1. Core Simulation Engine | 7/12 | In Progress |
| 2. Backend-GUI Bridge | 0/5 | Not Started |
| 3. Qt GUI & Live Dashboard | 1/8 | Not Started |
| 4. Data Handling & Import/Export | 0/3 | Not Started |
| 5. Headless CLI & CI | 0/3 | Not Started |
| 6. Security & Performance | 2/4 | In Progress |
| 7. Extensibility | 1/3 | In Progress |

---

### **1. Core Simulation Engine (High Priority)**

**Objective**: Enable real-time simulation with configurable tick rates and support for signal types.

#### Renode Backend Integration
- [x] **Integrate Renode as the backend** — ExternalControlClient with raw TCP protocol, handshake, machine management
- [x] **Renode process management** — RAII RenodeProcess with launch/terminate lifecycle
- [x] **Monitor command interface** — Telnet-based monitor for loadELF, loadPlatform, pause/start/reset
- [x] **Machine abstraction** — AMachine with runFor, getTime, peripheral vending, metadata accessors

#### Peripheral Support
- [x] **GPIO peripheral** — getState, setState, async event callbacks via EventCallbackRegistry
- [x] **ADC peripheral** — getChannelCount, getChannelValue, setChannelValue
- [x] **SysBus memory access** — read/write with access widths (Byte/Word/DWord/QWord)
- [ ] **UART protocol** — read/write, baud rate configuration
- [ ] **CAN protocol** — message send/receive, bus configuration
- [ ] **ETH protocol** — network interface simulation

#### Simulation Control
- [ ] **Implement a soft real-time simulation loop** with configurable tick rates (1ms to 100ms)
- [ ] **Complete async operations** — asyncRunFor (currently stub), runUntil, stepInstructions

#### Scripting (Low Priority — deferred)
- [ ] **Python API** (pybind11) for scripting test scenarios — *deferred to post-MVP*

---

### **2. Backend-GUI Bridge (High Priority — do next)**

**Objective**: Connect the working renodeAPI backend to the Qt QML frontend so the UI can control and display simulation state.

- [ ] **Restructure main.cpp** — move Renode initialization off the main thread, re-enable `app.exec()` (currently commented out)
- [ ] **Create SimulationController QObject** — wrap ExternalControlClient + AMachine with Q_PROPERTYs and Q_INVOKABLEs for QML binding
- [ ] **Create peripheral QML models** — GpioModel, AdcModel QObjects exposing pin/channel state to QML
- [ ] **Register C++ types with QML engine** — make backend accessible from QML components
- [ ] **Set up worker thread** — QThread for Renode communication to keep UI responsive, signals for async event delivery

---

### **3. Qt GUI & Live Dashboard (High Priority)**

**Objective**: Provide a user-friendly interface for interacting with simulated devices and visualizing data.

#### Application Shell
- [x] **Qt6 QML application skeleton** — appdigitwin target builds with Qt6::Quick
- [ ] **Navigation shell** — sidebar or tab bar in Main.qml for page routing

#### Dashboard & Controls
- [ ] **Live pin dashboard page** — real-time GPIO state grid (High/Low/HighZ indicators) and ADC channel value displays
- [ ] **Simulation control page** — connect/disconnect, run/pause/reset, runFor with duration input, simulation time display
- [ ] **Settings page** — configurable tick rate, connection parameters, signal value adjustments

#### Reusable Components
- [ ] **PinIndicator component** — color-coded GPIO state display
- [ ] **ValueGauge component** — ADC/analog value visualization

#### Advanced (Lower Priority)
- [ ] **3D model importer** — Qt Quick 3D integration for .obj/.fbx device visualization

---

### **4. Data Handling & Device Import/Export (Medium Priority)**

**Objective**: Enable flexible device model management and compatibility with Renode.

- [ ] **Design a JSON schema** for device models, ensuring compatibility with Renode's schema (e.g., for UART/CAN configurations)
- [ ] **Implement import/export functionality** for device models in JSON format, including metadata for signal types and protocol settings
- [ ] **Validate input data** during import to prevent errors in simulation setup

---

### **5. Headless CLI & CI Integration (Medium Priority)**

**Objective**: Support automation and CI/CD pipelines for testing and simulation.

- [ ] **Develop a command-line interface (CLI)** for running simulations, loading device models, and executing test scripts
- [ ] **Expose CLI commands** to control simulation parameters (e.g., start/stop, set tick rate)
- [ ] **Integrate with CI tools** (e.g., GitHub Actions, Jenkins) to automate test scenarios and validate simulation outputs

---

### **6. Security & Performance (High Priority)**

**Objective**: Ensure robustness, thread safety, and efficient resource management.

- [x] **Thread safety for backend** — std::mutex on ExternalControlClient, thread-safe EventCallbackRegistry singleton
- [ ] **Thread safety for GUI interactions** — Qt::QueuedConnection for cross-thread communication between worker and UI threads
- [ ] **Validate user inputs** (e.g., JSON files, CLI commands) to prevent invalid configurations or crashes
- [x] **ABI stability** — Pimpl pattern on all public classes, clean interface/implementation separation

---

### **7. Extensibility & Future-Proofing (Medium Priority)**

**Objective**: Design the architecture to support future features and integrations.

- [x] **Modularize protocol handling** — separate peripheral classes (GPIO, ADC, SysBus) with consistent registration pattern
- [ ] **Design a plugin system** for third-party device models or custom protocols
- [ ] **Document APIs** for CLI commands and C++ API to enable community contributions

---

### **Key Dependencies & Risks**

- ~~**Renode integration**: Ensure compatibility with Renode's API and data formats~~ — RESOLVED (raw TCP protocol working)
- **3D model importer**: Verify support for common file formats (.obj, .fbx) and integration with Qt Quick 3D
- **Performance bottlenecks**: Optimize the simulation loop to avoid latency issues in real-time scenarios
- **ZMQ**: Evaluated and removed — Renode protocol incompatible with ZMQ framing; using POSIX sockets

---

### **Recommended Development Order**

1. **Section 2** — Backend-GUI Bridge (unblocks all UI work)
2. **Section 3** — Qt GUI Dashboard & Controls
3. **Section 1** — Simulation loop + UART/CAN protocols
4. **Section 4** — JSON device schema
5. **Section 5** — Headless CLI
6. **Section 6/7** — Testing, performance, extensibility
