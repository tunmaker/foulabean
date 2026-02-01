#include "renodeMachine.h"
#include "renodeInterface.h"
#include "renodeInternal.h"
#include <cstring>
#include <map>

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

  Impl(const std::string &p, AMachine::Impl *m) : path(p), machine(m) {}
};

struct Gpio::Impl {
  std::string path;
  AMachine::Impl *machine;
  int32_t instanceId = -1;  // Renode peripheral instance ID from registration
  int nextCbHandle = 1;
  std::map<int, GpioCallback> callbacks;

  Impl(const std::string &p, AMachine::Impl *m) : path(p), machine(m) {}
};

struct SysBus::Impl {
  std::string path;
  AMachine::Impl *machine;

  Impl(const std::string &p, AMachine::Impl *m) : path(p), machine(m) {}
};

struct BusContext::Impl {
  std::string nodePath;
  AMachine::Impl *machine;  // Reference to parent machine instead

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
  // TODO: Send LOAD_CONFIG command to server
  return {0, ""};
}

Error AMachine::reset() noexcept {
  if (!pimpl_) return {1, "Invalid machine"};
  // TODO: Send RESET command to server
  return {0, ""};
}

Error AMachine::pause() noexcept {
  if (!pimpl_) return {1, "Invalid machine"};
  // TODO: Send PAUSE command to server
  return {0, ""};
}

Error AMachine::resume() noexcept {
  if (!pimpl_) return {1, "Invalid machine"};
  // TODO: Send RESUME command to server
  return {0, ""};
}

Result<bool> AMachine::isRunning() const noexcept {
  if (!pimpl_) return {{}, {1, "Invalid machine"}};
  // TODO: Query server for running state
  return {true, {0, ""}};
}

Result<std::vector<PeripheralDescriptor>> AMachine::listPeripherals() noexcept {
  if (!pimpl_) return {{}, {1, "Invalid machine"}};
  // TODO: Query server for peripheral list
  return {std::vector<PeripheralDescriptor>{}, {0, ""}};
}

Error AMachine::runFor(uint64_t duration, TimeUnit unit) noexcept {
  if (!pimpl_) return {1, "Invalid machine"};
  // TODO: Send RUN_FOR command to server
  return {0, ""};
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
  // TODO: Query server for current time
  return {0, {0, ""}};
}

AMachine::operator bool() const noexcept {
  return pimpl_ != nullptr && pimpl_->descriptor >= 0;
}

std::shared_ptr<Adc> AMachine::getAdc(const std::string &path, Error &err) noexcept {
  if (!pimpl_) {
    err = {1, "Invalid machine"};
    return nullptr;
  }

  auto impl = std::make_unique<Adc::Impl>(path, pimpl_.get());
  err = {0, ""};
  return std::shared_ptr<Adc>(new Adc(std::move(impl)));
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

  auto impl = std::make_unique<SysBus::Impl>(path, pimpl_.get());
  err = {0, ""};
  return std::shared_ptr<SysBus>(new SysBus(std::move(impl)));
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

Error Adc::getChannelCount(int &outCount) noexcept {
  if (!pimpl_) return {1, "Invalid ADC"};
  // TODO: Query server for channel count
  outCount = 4; // placeholder
  return {0, ""};
}

Error Adc::getChannelValue(int channel, AdcValue &outValue) noexcept {
  if (!pimpl_) return {1, "Invalid ADC"};
  // TODO: Query server for channel value
  outValue = 0.0; // placeholder
  return {0, ""};
}

Error Adc::setChannelValue(int channel, AdcValue value) noexcept {
  if (!pimpl_) return {1, "Invalid ADC"};
  // TODO: Send SET_CHANNEL command to server
  return {0, ""};
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

Error Gpio::registerStateChangeCallback(GpioCallback cb, int &outHandle) noexcept {
  if (!pimpl_) return {1, "Invalid GPIO"};

  int handle = pimpl_->nextCbHandle++;
  pimpl_->callbacks.emplace(handle, std::move(cb));
  outHandle = handle;

  return {0, ""};
}

Error Gpio::unregisterStateChangeCallback(int handle) noexcept {
  if (!pimpl_) return {1, "Invalid GPIO"};

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

  auto impl = std::make_unique<BusContext::Impl>(nodePath, pimpl_->machine);
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

Error BusContext::read(uint64_t address, AccessWidth width, uint64_t &outValue) noexcept {
  if (!pimpl_) return {1, "Invalid BusContext"};
  // TODO: Send READ command to server
  outValue = 0; // placeholder
  return {0, ""};
}

Error BusContext::write(uint64_t address, AccessWidth width, uint64_t value) noexcept {
  if (!pimpl_) return {1, "Invalid BusContext"};
  // TODO: Send WRITE command to server
  return {0, ""};
}

BusContext::operator bool() const noexcept {
  return pimpl_ != nullptr;
}

}