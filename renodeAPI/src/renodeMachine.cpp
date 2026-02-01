#include "renodeMachine.h"

namespace renode {


struct AMachine::Impl {
  std::string name;
  ExternalControlClient::Impl *renodeClient;
  Impl(const std::string &n, ExternalControlClient::Impl *c)
      : name(n), renodeClient(c) {}
};

AMachine::AMachine(std::unique_ptr<Impl> impl) noexcept
    : pimpl_(std::move(impl)) {}

AMachine::~AMachine() = default;

std::string AMachine::name() const noexcept {
  return pimpl_ ? pimpl_->name : std::string();
}

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
  instImpl->name = descriptor; // store received descriptor if your Impl has such a field
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


}