#include "renodeMachine.h"
#include "renodeInterface.h"
#include "renodeInternal.h"
#include <cstring>
#include <map>
#include <sstream>

namespace renode {

// AMachine::Impl definition
struct AMachine::Impl {
  std::string name;
  int32_t descriptor = -1;  // Server-side machine descriptor
  ExternalControlClient::Impl *renodeClient;

  Impl(const std::string &n, ExternalControlClient::Impl *c)
      : name(n), renodeClient(c) {}

  // Accessor for peripheral classes to get machine descriptor
  int32_t getDescriptor() const noexcept { return descriptor; }
};

// Peripheral Impl definitions
struct Adc::Impl {
  std::string path;
  AMachine::Impl *machine;
  int32_t instanceId = -1;  // Server-assigned instance ID from registration

  Impl(const std::string &p, AMachine::Impl *m) : path(p), machine(m) {}
};

struct Gpio::Impl {
  std::string path;
  AMachine::Impl *machine;
  int32_t instanceId = -1;  // Renode peripheral instance ID from registration
  int nextCbHandle = 1;
  std::map<int, GpioCallback> callbacks;
  std::map<int, uint32_t> handleToServerEd;  // Maps local handle to server event descriptor

  Impl(const std::string &p, AMachine::Impl *m) : path(p), machine(m) {}
};

struct SysBus::Impl {
  std::string path;
  AMachine::Impl *machine;
  int32_t instanceId = -1;  // Server-assigned instance ID

  Impl(const std::string &p, AMachine::Impl *m) : path(p), machine(m) {}
};

struct BusContext::Impl {
  std::string nodePath;
  AMachine::Impl *machine;
  int32_t instanceId = -1;  // Server-assigned bus context ID

  Impl(const std::string &n, AMachine::Impl *m) : nodePath(n), machine(m) {}
};

AMachine::AMachine(std::unique_ptr<Impl> impl) noexcept
    : pimpl_(std::move(impl)) {}

AMachine::~AMachine() = default;

std::string AMachine::id() const noexcept {
  return pimpl_ ? std::to_string(pimpl_->descriptor) : std::string();
}

std::string AMachine::path() const noexcept {
  return pimpl_ ? pimpl_->name : std::string();
}

std::string AMachine::name() const noexcept {
  return pimpl_ ? pimpl_->name : std::string();
}

std::optional<std::string> AMachine::metadata(const std::string &key) const noexcept {
  // TODO: Implement metadata storage and retrieval
  return std::nullopt;
}

Error AMachine::setMetadata(const std::string &key, const std::string &value) noexcept {
  if (!pimpl_) return {1, "Invalid machine"};
  // TODO: Implement metadata storage
  return {0, ""};
}

Error AMachine::loadConfiguration(const std::string &config) noexcept {
  if (!pimpl_) return {1, "Invalid machine"};
  if (!pimpl_->renodeClient) return {2, "No client connection"};

  // Use monitor to load platform description
  Monitor* monitor = pimpl_->renodeClient->monitor;
  if (monitor) {
    // Check if config looks like an ELF path or a .repl path
    if (config.find(".elf") != std::string::npos ||
        config.find(".ELF") != std::string::npos) {
      return monitor->loadELF(config);
    } else {
      return monitor->loadPlatformDescription(config);
    }
  }
  return {3, "No monitor connection for loadConfiguration"};
}

Error AMachine::reset() noexcept {
  if (!pimpl_) return {1, "Invalid machine"};
  if (!pimpl_->renodeClient) return {2, "No client connection"};

  // Use monitor if available
  Monitor* monitor = pimpl_->renodeClient->monitor;
  if (monitor) {
    return monitor->reset();
  }
  return {3, "No monitor connection for reset command"};
}

Error AMachine::pause() noexcept {
  if (!pimpl_) return {1, "Invalid machine"};
  if (!pimpl_->renodeClient) return {2, "No client connection"};

  // Use monitor if available
  Monitor* monitor = pimpl_->renodeClient->monitor;
  if (monitor) {
    return monitor->pause();
  }
  return {3, "No monitor connection for pause command"};
}

Error AMachine::resume() noexcept {
  if (!pimpl_) return {1, "Invalid machine"};
  if (!pimpl_->renodeClient) return {2, "No client connection"};

  // Use monitor if available
  Monitor* monitor = pimpl_->renodeClient->monitor;
  if (monitor) {
    return monitor->start();
  }
  return {3, "No monitor connection for resume command"};
}

Result<bool> AMachine::isRunning() const noexcept {
  if (!pimpl_) return {{}, {1, "Invalid machine"}};
  if (!pimpl_->renodeClient) return {{}, {2, "No client connection"}};

  // Use monitor to query state
  Monitor* monitor = pimpl_->renodeClient->monitor;
  if (monitor) {
    auto result = monitor->execute("emulation IsStarted");
    if (result.error) {
      return {{}, result.error};
    }
    // Parse "True" or "False" response
    bool running = (result.value.find("True") != std::string::npos);
    return {running, {0, ""}};
  }
  // Default to true if no monitor
  return {true, {0, ""}};
}

Result<std::vector<PeripheralDescriptor>> AMachine::listPeripherals() noexcept {
  if (!pimpl_) return {{}, {1, "Invalid machine"}};
  if (!pimpl_->renodeClient) return {{}, {2, "No client connection"}};

  // Use monitor to list peripherals
  Monitor* monitor = pimpl_->renodeClient->monitor;
  if (!monitor) {
    return {{}, {3, "No monitor connection for listPeripherals"}};
  }

  auto result = monitor->execute("peripherals");
  if (result.error) {
    return {{}, result.error};
  }

  // Parse output - format is typically:
  // sysbus:
  //   cpu (Cortex-M4)
  //   uart0 (STM32_UART)
  //   ...
  std::vector<PeripheralDescriptor> peripherals;
  std::istringstream stream(result.value);
  std::string line;
  std::string currentBus;

  while (std::getline(stream, line)) {
    // Skip empty lines
    if (line.empty()) continue;

    // Check for bus declaration (ends with ':')
    if (!line.empty() && line.back() == ':') {
      currentBus = line.substr(0, line.size() - 1);
      continue;
    }

    // Parse peripheral line: "  name (Type)"
    size_t start = line.find_first_not_of(" \t");
    if (start == std::string::npos) continue;

    size_t parenStart = line.find('(', start);
    size_t parenEnd = line.find(')', parenStart);

    if (parenStart != std::string::npos && parenEnd != std::string::npos) {
      PeripheralDescriptor desc;
      std::string name = line.substr(start, parenStart - start);
      // Trim trailing spaces from name
      while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) {
        name.pop_back();
      }
      desc.path = currentBus.empty() ? name : (currentBus + "." + name);
      desc.type = line.substr(parenStart + 1, parenEnd - parenStart - 1);
      peripherals.push_back(std::move(desc));
    }
  }

  return {peripherals, {0, ""}};
}

Error AMachine::runFor(uint64_t duration, TimeUnit unit) noexcept {
  if (!pimpl_) return {1, "Invalid machine"};
  if (!pimpl_->renodeClient) return {2, "No client connection"};

  // Convert duration to microseconds
  uint64_t microseconds = duration * static_cast<uint64_t>(unit);

  // Build payload: 8-byte little-endian microseconds
  std::vector<uint8_t> payload;
  write_u64_le(payload, microseconds);

  try {
    // Send RUN_FOR command
    // Note: In the future, this should handle ASYNC_EVENT responses in a loop
    // for GPIO callbacks during simulation run
    pimpl_->renodeClient->send_command(ApiCommand::RUN_FOR, payload);
    return {0, ""};
  } catch (const std::exception &ex) {
    return {3, std::string("runFor failed: ") + ex.what()};
  }
}

std::future<Error> AMachine::asyncRunFor(uint64_t duration, TimeUnit unit) {
  // TODO: Implement async version
  std::promise<Error> p;
  p.set_value(runFor(duration, unit));
  return p.get_future();
}

Error AMachine::runUntil(uint64_t timestampMicroseconds) noexcept {
  if (!pimpl_) return {1, "Invalid machine"};
  // TODO: Send RUN_UNTIL command to server
  return {0, ""};
}

Error AMachine::stepInstructions(uint64_t count) noexcept {
  if (!pimpl_) return {1, "Invalid machine"};
  // TODO: Send STEP command to server
  return {0, ""};
}

Result<uint64_t> AMachine::getTime(TimeUnit unit) const noexcept {
  if (!pimpl_) return {0, {1, "Invalid machine"}};
  if (!pimpl_->renodeClient) return {0, {2, "No client connection"}};

  try {
    // GET_TIME expects an 8-byte payload (placeholder, value ignored by server)
    std::vector<uint8_t> payload(8, 0);

    auto response = pimpl_->renodeClient->send_command(ApiCommand::GET_TIME, payload);

    if (response.size() != 8) {
      return {0, {3, "Unexpected response size from GET_TIME"}};
    }

    // Parse 8-byte little-endian microseconds
    uint64_t time_us = read_u64_le(response.data());

    // Convert to requested unit
    uint64_t divider = static_cast<uint64_t>(unit);
    return {time_us / divider, {0, ""}};

  } catch (const std::exception &ex) {
    return {0, {4, std::string("getTime failed: ") + ex.what()}};
  }
}

AMachine::operator bool() const noexcept {
  return pimpl_ != nullptr && pimpl_->descriptor >= 0;
}

std::shared_ptr<Adc> AMachine::getAdc(const std::string &path, Error &err) noexcept {
  if (!pimpl_) {
    err = {1, "Invalid machine"};
    return nullptr;
  }

  // Register the ADC peripheral with Renode to get an instance ID
  // Protocol (from renode_get_instance_descriptor):
  //   data[0] = -1 (registration marker)
  //   data[1] = machine->md (machine descriptor)
  //   data[2] = name_length
  //   data[3..] = name bytes
  try {
    std::vector<uint8_t> payload;

    // Instance ID = -1 (registration request)
    write_i32_le(payload, -1);

    // Machine descriptor (required for registration)
    write_i32_le(payload, pimpl_->descriptor);

    // Peripheral path string (4-byte length + UTF-8 bytes)
    write_string(payload, path);

    // Send ADC command for registration
    std::vector<uint8_t> response = pimpl_->renodeClient->send_command(ApiCommand::ADC, payload);

    // Response should be 4 bytes: the assigned instance ID
    if (response.size() != sizeof(int32_t)) {
      err = {2, "Unexpected response size from ADC registration"};
      return nullptr;
    }

    int32_t instanceId = static_cast<int32_t>(read_u32_le(response.data()));

    if (instanceId < 0) {
      err = {3, "ADC registration failed: invalid instance ID"};
      return nullptr;
    }

    auto impl = std::make_unique<Adc::Impl>(path, pimpl_.get());
    impl->instanceId = instanceId;
    err = {0, ""};
    return std::shared_ptr<Adc>(new Adc(std::move(impl)));

  } catch (const std::exception &ex) {
    err = {4, std::string("ADC registration failed: ") + ex.what()};
    return nullptr;
  }
}

std::shared_ptr<Gpio> AMachine::getGpio(const std::string &path, Error &err) noexcept {
  if (!pimpl_) {
    err = {1, "Invalid machine"};
    return nullptr;
  }

  // Register the GPIO peripheral with Renode to get an instance ID
  // Protocol (from renode_get_instance_descriptor):
  //   data[0] = -1 (registration marker)
  //   data[1] = machine->md (machine descriptor)
  //   data[2] = name_length
  //   data[3..] = name bytes
  try {
    std::vector<uint8_t> payload;

    // Instance ID = -1 (registration request)
    int32_t regId = -1;
    auto regId_bytes = reinterpret_cast<const uint8_t*>(&regId);
    payload.insert(payload.end(), regId_bytes, regId_bytes + sizeof(regId));

    // Machine descriptor (required for registration)
    int32_t machineDesc = pimpl_->descriptor;
    auto md_bytes = reinterpret_cast<const uint8_t*>(&machineDesc);
    payload.insert(payload.end(), md_bytes, md_bytes + sizeof(machineDesc));

    // Peripheral path string (4-byte length + UTF-8 bytes)
    write_string(payload, path);

    // Send GPIO command for registration
    std::vector<uint8_t> response = pimpl_->renodeClient->send_command(ApiCommand::GPIO, payload);

    // Response should be 4 bytes: the assigned instance ID
    if (response.size() != sizeof(int32_t)) {
      err = {2, "Unexpected response size from GPIO registration"};
      return nullptr;
    }

    int32_t instanceId = 0;
    std::memcpy(&instanceId, response.data(), sizeof(instanceId));

    if (instanceId < 0) {
      err = {3, "GPIO registration failed: invalid instance ID"};
      return nullptr;
    }

    auto impl = std::make_unique<Gpio::Impl>(path, pimpl_.get());
    impl->instanceId = instanceId;
    err = {0, ""};
    return std::shared_ptr<Gpio>(new Gpio(std::move(impl)));

  } catch (const std::exception &ex) {
    err = {4, std::string("GPIO registration failed: ") + ex.what()};
    return nullptr;
  }
}

std::shared_ptr<SysBus> AMachine::getSysBus(const std::string &path, Error &err) noexcept {
  if (!pimpl_) {
    err = {1, "Invalid machine"};
    return nullptr;
  }

  // Register the SysBus peripheral with Renode to get an instance ID
  // Protocol (same as ADC/GPIO registration):
  //   data[0] = -1 (registration marker)
  //   data[1] = machine->md (machine descriptor)
  //   data[2] = name_length
  //   data[3..] = name bytes
  try {
    std::vector<uint8_t> payload;

    // Instance ID = -1 (registration request)
    write_i32_le(payload, -1);

    // Machine descriptor
    write_i32_le(payload, pimpl_->descriptor);

    // Peripheral path string (4-byte length + UTF-8 bytes)
    write_string(payload, path);

    // Send SYSTEM_BUS command for registration
    std::vector<uint8_t> response = pimpl_->renodeClient->send_command(ApiCommand::SYSTEM_BUS, payload);

    // Response should be 4 bytes: the assigned instance ID
    if (response.size() != sizeof(int32_t)) {
      err = {2, "Unexpected response size from SysBus registration"};
      return nullptr;
    }

    int32_t instanceId = static_cast<int32_t>(read_u32_le(response.data()));

    if (instanceId < 0) {
      err = {3, "SysBus registration failed: invalid instance ID"};
      return nullptr;
    }

    auto impl = std::make_unique<SysBus::Impl>(path, pimpl_.get());
    impl->instanceId = instanceId;
    err = {0, ""};
    return std::shared_ptr<SysBus>(new SysBus(std::move(impl)));

  } catch (const std::exception &ex) {
    err = {4, std::string("SysBus registration failed: ") + ex.what()};
    return nullptr;
  }
}

// ============================================================================
// ExternalControlClient::getMachine implementation
// ============================================================================

std::shared_ptr<AMachine> ExternalControlClient::getMachine(const std::string &name, Error &err) noexcept {
  std::lock_guard<std::mutex> lk(pimpl_->mtx);
  if (!pimpl_->connected) {
    err = {1, "Not connected"};
    return nullptr;
  }

  std::vector<uint8_t> data;
  uint32_t name_length = static_cast<uint32_t>(name.size());
  data.reserve(sizeof(name_length) + name_length);

  // append length in little-endian
  uint32_t le_len = name_length;
  auto len_bytes = reinterpret_cast<const uint8_t*>(&le_len);
  data.insert(data.end(), len_bytes, len_bytes + sizeof(le_len));

  // append name bytes
  data.insert(data.end(), name.begin(), name.end());

  // send command and get reply
  std::vector<uint8_t> reply;
  try {
    reply = send_command(ApiCommand::GET_MACHINE, data);
  } catch (const std::exception &ex) {
    err = {2, std::string("send_command failed: ") + ex.what()};
    return nullptr;
  }

  // Expect exactly 4 bytes (int32 descriptor)
  if (reply.size() != sizeof(int32_t)) {
    err = {3, "Unexpected reply size from GET_MACHINE"};
    return nullptr;
  }

  int32_t descriptor = 0;
  std::memcpy(&descriptor, reply.data(), sizeof(descriptor)); // assumes little-endian
  // If you require network (big-endian) convert with ntohl here.

  if (descriptor < 0) {
    err = {4, "Machine not found"};
    return nullptr;
  }

  // If already have a weak_ptr cached, return it
  if (auto existing = pimpl_->machines[name].lock()) {
    err = {0, ""};
    return existing;
  }

  // Create new local wrapper and store weak_ptr
  auto instImpl = std::make_unique<AMachine::Impl>(name, pimpl_.get());
  instImpl->descriptor = descriptor; // store received descriptor
  auto inst = std::shared_ptr<AMachine>(new AMachine(std::move(instImpl)));
  pimpl_->machines[name] = inst;
  err = {0, ""};
  return inst;
}

std::shared_ptr<AMachine>
ExternalControlClient::getMachineOrThrow(const std::string &name) {
  Error e;
  auto m = getMachine(name, e);
  if (!m)
    throw RenodeException("Machine not found: " + name + " (" + e.message + ")");
  return m;
}

// ============================================================================
// Adc implementation
// ============================================================================

Adc::Adc(std::unique_ptr<Impl> impl) noexcept : pimpl_(std::move(impl)) {}

Adc::~Adc() = default;

// ADC subcommand enum (matches C reference adc_command_t)
enum AdcSubcommand : int8_t {
  ADC_GET_CHANNEL_COUNT = 0,
  ADC_GET_CHANNEL_VALUE = 1,
  ADC_SET_CHANNEL_VALUE = 2,
};

Error Adc::getChannelCount(int &outCount) noexcept {
  if (!pimpl_) return {1, "Invalid ADC"};
  if (pimpl_->instanceId < 0) return {2, "ADC not registered"};
  if (!pimpl_->machine) return {3, "Invalid machine reference"};

  try {
    std::vector<uint8_t> payload;

    // Instance ID (4 bytes LE)
    write_i32_le(payload, pimpl_->instanceId);

    // Subcommand: GET_CHANNEL_COUNT = 0
    payload.push_back(ADC_GET_CHANNEL_COUNT);

    auto response = pimpl_->machine->renodeClient->send_command(ApiCommand::ADC, payload);

    if (response.size() != 4) {
      return {4, "Unexpected response size from ADC getChannelCount"};
    }

    outCount = static_cast<int>(read_u32_le(response.data()));
    return {0, ""};

  } catch (const std::exception &ex) {
    return {5, std::string("ADC getChannelCount failed: ") + ex.what()};
  }
}

Error Adc::getChannelValue(int channel, AdcValue &outValue) noexcept {
  if (!pimpl_) return {1, "Invalid ADC"};
  if (pimpl_->instanceId < 0) return {2, "ADC not registered"};
  if (!pimpl_->machine) return {3, "Invalid machine reference"};

  try {
    std::vector<uint8_t> payload;

    // Instance ID (4 bytes LE)
    write_i32_le(payload, pimpl_->instanceId);

    // Subcommand: GET_CHANNEL_VALUE = 1
    payload.push_back(ADC_GET_CHANNEL_VALUE);

    // Channel index (4 bytes LE)
    write_i32_le(payload, static_cast<int32_t>(channel));

    auto response = pimpl_->machine->renodeClient->send_command(ApiCommand::ADC, payload);

    if (response.size() != 4) {
      return {4, "Unexpected response size from ADC getChannelValue"};
    }

    // Parse 4-byte unsigned value and convert to AdcValue (double)
    uint32_t rawValue = read_u32_le(response.data());
    outValue = static_cast<AdcValue>(rawValue);
    return {0, ""};

  } catch (const std::exception &ex) {
    return {5, std::string("ADC getChannelValue failed: ") + ex.what()};
  }
}

Error Adc::setChannelValue(int channel, AdcValue value) noexcept {
  if (!pimpl_) return {1, "Invalid ADC"};
  if (pimpl_->instanceId < 0) return {2, "ADC not registered"};
  if (!pimpl_->machine) return {3, "Invalid machine reference"};

  try {
    std::vector<uint8_t> payload;

    // Instance ID (4 bytes LE)
    write_i32_le(payload, pimpl_->instanceId);

    // Subcommand: SET_CHANNEL_VALUE = 2
    payload.push_back(ADC_SET_CHANNEL_VALUE);

    // Channel index (4 bytes LE)
    write_i32_le(payload, static_cast<int32_t>(channel));

    // Value (4 bytes LE) - convert double to uint32
    uint32_t rawValue = static_cast<uint32_t>(value);
    write_u32_le(payload, rawValue);

    // Expect SUCCESS_WITHOUT_DATA (empty response)
    pimpl_->machine->renodeClient->send_command(ApiCommand::ADC, payload);
    return {0, ""};

  } catch (const std::exception &ex) {
    return {5, std::string("ADC setChannelValue failed: ") + ex.what()};
  }
}

Adc::operator bool() const noexcept {
  return pimpl_ != nullptr;
}

// ============================================================================
// Gpio implementation
// ============================================================================

Gpio::Gpio(std::unique_ptr<Impl> impl) noexcept : pimpl_(std::move(impl)) {}

Gpio::~Gpio() = default;

Error Gpio::getState(int pin, GpioState &outState) noexcept {
  if (!pimpl_) return {1, "Invalid GPIO"};
  if (!pimpl_->machine) return {2, "Invalid machine reference"};
  if (pimpl_->instanceId < 0) return {2, "GPIO not registered"};

  try {
    // Build payload per Renode protocol:
    // id (int32_t) + command (int8_t) + number (int32_t)
    std::vector<uint8_t> payload;

    // Instance ID (4 bytes LE) - obtained from registration
    int32_t id = pimpl_->instanceId;
    auto id_bytes = reinterpret_cast<const uint8_t*>(&id);
    payload.insert(payload.end(), id_bytes, id_bytes + sizeof(id));

    // Subcommand: 0 = GET_STATE (1 byte)
    payload.push_back(0);

    // Pin number (4 bytes LE)
    int32_t pin_i32 = static_cast<int32_t>(pin);
    auto pin_bytes = reinterpret_cast<const uint8_t*>(&pin_i32);
    payload.insert(payload.end(), pin_bytes, pin_bytes + sizeof(pin_i32));

    // Send command
    std::vector<uint8_t> response = pimpl_->machine->renodeClient->send_command(ApiCommand::GPIO, payload);

    // Parse response: 1 byte state value
    if (response.size() != 1) {
      return {3, "Unexpected response size from GPIO GET_STATE"};
    }

    uint8_t state_byte = response[0];
    if (state_byte > 2) {
      return {4, "Invalid GPIO state value from server"};
    }

    outState = static_cast<GpioState>(state_byte);
    return {0, ""};

  } catch (const std::exception &ex) {
    return {5, std::string("GPIO getState failed: ") + ex.what()};
  }
}

Error Gpio::setState(int pin, GpioState state) noexcept {
  if (!pimpl_) return {1, "Invalid GPIO"};
  if (!pimpl_->machine) return {2, "Invalid machine reference"};
  if (pimpl_->instanceId < 0) return {2, "GPIO not registered"};

  try {
    // Build payload per Renode protocol:
    // id (int32_t) + command (int8_t) + number (int32_t) + state (uint8_t)
    std::vector<uint8_t> payload;

    // Instance ID (4 bytes LE) - obtained from registration
    int32_t id = pimpl_->instanceId;
    auto id_bytes = reinterpret_cast<const uint8_t*>(&id);
    payload.insert(payload.end(), id_bytes, id_bytes + sizeof(id));

    // Subcommand: 1 = SET_STATE (1 byte)
    payload.push_back(1);

    // Pin number (4 bytes LE)
    int32_t pin_i32 = static_cast<int32_t>(pin);
    auto pin_bytes = reinterpret_cast<const uint8_t*>(&pin_i32);
    payload.insert(payload.end(), pin_bytes, pin_bytes + sizeof(pin_i32));

    // State value (1 byte)
    payload.push_back(static_cast<uint8_t>(state));

    // Send command (expect SUCCESS_WITHOUT_DATA, empty response)
    pimpl_->machine->renodeClient->send_command(ApiCommand::GPIO, payload);

    // Trigger callbacks for state change (only after successful server update)
    for (auto &kv : pimpl_->callbacks) {
      kv.second(pin, state);
    }

    return {0, ""};

  } catch (const std::exception &ex) {
    // Don't trigger callbacks if command failed
    return {5, std::string("GPIO setState failed: ") + ex.what()};
  }
}

// GPIO subcommand enum (matches C reference gpio_command_t)
enum GpioSubcommand : int8_t {
  GPIO_GET_STATE = 0,
  GPIO_SET_STATE = 1,
  GPIO_REGISTER_EVENT = 2,
};

Error Gpio::registerStateChangeCallback(int pin, GpioCallback cb, int &outHandle) noexcept {
  if (!pimpl_) return {1, "Invalid GPIO"};
  if (pimpl_->instanceId < 0) return {2, "GPIO not registered"};
  if (!pimpl_->machine) return {3, "Invalid machine reference"};

  try {
    // Allocate local handle first
    int handle = pimpl_->nextCbHandle++;

    // Create wrapper callback that converts server event data to GpioCallback format
    // Server sends: timestamp_us (8B) + state (1B)
    auto wrapperCb = [cb, pin](const uint8_t* data, size_t size) {
      if (size >= 9) {  // 8 bytes timestamp + 1 byte state
        // uint64_t timestamp_us = read_u64_le(data);  // Available if needed
        uint8_t state_byte = data[8];
        GpioState state = (state_byte != 0) ? GpioState::High : GpioState::Low;
        cb(pin, state);
      }
    };

    // Register with global event callback registry to get server event descriptor
    uint32_t serverEd = EventCallbackRegistry::instance().registerCallback(wrapperCb);

    // Build payload for REGISTER_EVENT command (from C reference event_gpio_frame)
    // id (4B) + command (1B) + number (4B) + ed (4B)
    std::vector<uint8_t> payload;

    // Instance ID (4 bytes LE)
    write_i32_le(payload, pimpl_->instanceId);

    // Subcommand: REGISTER_EVENT = 2
    payload.push_back(GPIO_REGISTER_EVENT);

    // Pin number (4 bytes LE)
    write_i32_le(payload, static_cast<int32_t>(pin));

    // Server event descriptor (4 bytes LE)
    write_u32_le(payload, serverEd);

    // Send command to register with Renode server
    pimpl_->machine->renodeClient->send_command(ApiCommand::GPIO, payload);

    // Store mappings
    pimpl_->callbacks.emplace(handle, std::move(cb));
    pimpl_->handleToServerEd[handle] = serverEd;
    outHandle = handle;

    return {0, ""};

  } catch (const std::exception &ex) {
    return {4, std::string("GPIO registerStateChangeCallback failed: ") + ex.what()};
  }
}

// Legacy overload: local-only callback, not registered with server
Error Gpio::registerStateChangeCallback(GpioCallback cb, int &outHandle) noexcept {
  if (!pimpl_) return {1, "Invalid GPIO"};

  int handle = pimpl_->nextCbHandle++;
  pimpl_->callbacks.emplace(handle, std::move(cb));
  outHandle = handle;

  return {0, ""};
}

Error Gpio::unregisterStateChangeCallback(int handle) noexcept {
  if (!pimpl_) return {1, "Invalid GPIO"};

  // If this callback was registered with server, unregister from EventCallbackRegistry
  auto edIt = pimpl_->handleToServerEd.find(handle);
  if (edIt != pimpl_->handleToServerEd.end()) {
    EventCallbackRegistry::instance().unregisterCallback(edIt->second);
    pimpl_->handleToServerEd.erase(edIt);
  }

  pimpl_->callbacks.erase(handle);

  return {0, ""};
}

Gpio::operator bool() const noexcept {
  return pimpl_ != nullptr;
}

// ============================================================================
// SysBus implementation
// ============================================================================

SysBus::SysBus(std::unique_ptr<Impl> impl) noexcept : pimpl_(std::move(impl)) {}

SysBus::~SysBus() = default;

std::shared_ptr<BusContext> SysBus::getBusContext(const std::string &nodePath, Error &err) noexcept {
  if (!pimpl_) {
    err = {1, "Invalid SysBus"};
    return nullptr;
  }
  if (pimpl_->instanceId < 0) {
    err = {2, "SysBus not registered"};
    return nullptr;
  }

  auto impl = std::make_unique<BusContext::Impl>(nodePath, pimpl_->machine);
  impl->instanceId = pimpl_->instanceId;  // Pass SysBus instance ID to BusContext
  err = {0, ""};
  return std::shared_ptr<BusContext>(new BusContext(std::move(impl)));
}

SysBus::operator bool() const noexcept {
  return pimpl_ != nullptr;
}

// ============================================================================
// BusContext implementation
// ============================================================================

BusContext::BusContext(std::unique_ptr<Impl> impl) noexcept : pimpl_(std::move(impl)) {}

BusContext::~BusContext() = default;

// SysBus operation enum (matches C reference sysbus_operation_t)
enum SysBusOperation : uint8_t {
  SYSBUS_READ = 0,
  SYSBUS_WRITE = 1,
};

Error BusContext::read(uint64_t address, AccessWidth width, uint64_t &outValue) noexcept {
  if (!pimpl_) return {1, "Invalid BusContext"};
  if (pimpl_->instanceId < 0) return {2, "BusContext not initialized"};
  if (!pimpl_->machine) return {3, "Invalid machine reference"};

  try {
    // Build payload per C reference (sysbus_command_t):
    // id (4B) + operation (1B) + access_width (1B) + address (8B) + data_count (4B)
    std::vector<uint8_t> payload;

    // Instance ID (4 bytes LE)
    write_i32_le(payload, pimpl_->instanceId);

    // Operation: READ = 0
    payload.push_back(SYSBUS_READ);

    // Access width (1 byte)
    payload.push_back(static_cast<uint8_t>(width));

    // Address (8 bytes LE)
    write_u64_le(payload, address);

    // Count = 1 (reading single value)
    write_u32_le(payload, 1);

    auto response = pimpl_->machine->renodeClient->send_command(ApiCommand::SYSTEM_BUS, payload);

    // Calculate expected response size based on access width
    size_t expected_bytes;
    switch (width) {
      case AccessWidth::AW_BYTE:       expected_bytes = 1; break;
      case AccessWidth::AW_WORD:       expected_bytes = 2; break;
      case AccessWidth::AW_DWord:      expected_bytes = 4; break;
      case AccessWidth::AW_QWord:      expected_bytes = 8; break;
      case AccessWidth::AW_MULTI_BYTE: expected_bytes = 1; break; // Default for multi-byte
      default:                         expected_bytes = 4; break;
    }

    if (response.size() < expected_bytes) {
      return {4, "Unexpected response size from SysBus read"};
    }

    // Parse response as little-endian value
    outValue = 0;
    for (size_t i = 0; i < expected_bytes && i < response.size(); ++i) {
      outValue |= static_cast<uint64_t>(response[i]) << (i * 8);
    }

    return {0, ""};

  } catch (const std::exception &ex) {
    return {5, std::string("BusContext read failed: ") + ex.what()};
  }
}

Error BusContext::write(uint64_t address, AccessWidth width, uint64_t value) noexcept {
  if (!pimpl_) return {1, "Invalid BusContext"};
  if (pimpl_->instanceId < 0) return {2, "BusContext not initialized"};
  if (!pimpl_->machine) return {3, "Invalid machine reference"};

  try {
    // Build payload per C reference (sysbus_command_t):
    // id (4B) + operation (1B) + access_width (1B) + address (8B) + data_count (4B) + data[]
    std::vector<uint8_t> payload;

    // Instance ID (4 bytes LE)
    write_i32_le(payload, pimpl_->instanceId);

    // Operation: WRITE = 1
    payload.push_back(SYSBUS_WRITE);

    // Access width (1 byte)
    payload.push_back(static_cast<uint8_t>(width));

    // Address (8 bytes LE)
    write_u64_le(payload, address);

    // Count = 1 (writing single value)
    write_u32_le(payload, 1);

    // Data bytes based on access width
    size_t data_bytes;
    switch (width) {
      case AccessWidth::AW_BYTE:       data_bytes = 1; break;
      case AccessWidth::AW_WORD:       data_bytes = 2; break;
      case AccessWidth::AW_DWord:      data_bytes = 4; break;
      case AccessWidth::AW_QWord:      data_bytes = 8; break;
      case AccessWidth::AW_MULTI_BYTE: data_bytes = 1; break;
      default:                         data_bytes = 4; break;
    }

    // Append data bytes in little-endian order
    for (size_t i = 0; i < data_bytes; ++i) {
      payload.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
    }

    // Expect SUCCESS_WITHOUT_DATA (empty response)
    pimpl_->machine->renodeClient->send_command(ApiCommand::SYSTEM_BUS, payload);
    return {0, ""};

  } catch (const std::exception &ex) {
    return {5, std::string("BusContext write failed: ") + ex.what()};
  }
}

BusContext::operator bool() const noexcept {
  return pimpl_ != nullptr;
}

}