// renodeInternal.h
// Internal header for exposing ExternalControlClient::Impl to renodeMachine.cpp
#pragma once

#include "renodeInterface.h"
#include "defs.h"
#include <map>
#include <mutex>
#include <functional>

namespace renode {

// Forward declare AMachine so we can reference it
class AMachine;

// Event callback registry for async GPIO callbacks during runFor()
// Matches C reference (renode_api.c:339-358)
class EventCallbackRegistry {
public:
  using RawCallback = std::function<void(const uint8_t*, size_t)>;

  static EventCallbackRegistry& instance() {
    static EventCallbackRegistry registry;
    return registry;
  }

  uint32_t registerCallback(RawCallback cb) {
    std::lock_guard<std::mutex> lock(mtx_);
    uint32_t ed = nextId_++;
    callbacks_[ed] = std::move(cb);
    return ed;
  }

  void unregisterCallback(uint32_t ed) {
    std::lock_guard<std::mutex> lock(mtx_);
    callbacks_.erase(ed);
  }

  bool invokeCallback(uint32_t ed, const uint8_t* data, size_t size) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = callbacks_.find(ed);
    if (it != callbacks_.end()) {
      it->second(data, size);
      return true;
    }
    return false;
  }

private:
  EventCallbackRegistry() = default;
  std::map<uint32_t, RawCallback> callbacks_;
  std::mutex mtx_;
  uint32_t nextId_ = 0;
};

// Forward declare Monitor
class Monitor;

// ExternalControlClient::Impl definition exposed for use in renodeMachine.cpp
struct ExternalControlClient::Impl {
  std::string host;
  uint16_t port;
  int sock_fd = -1;  // Socket file descriptor
  bool connected = false;
  std::mutex mtx;

  // Cache of machines
  std::map<std::string, std::weak_ptr<AMachine>> machines;

  // Pointer to Monitor (owned by ExternalControlClient, set after construction)
  Monitor* monitor = nullptr;

  Impl(const std::string &h, uint16_t p) : host(h), port(p) {}

  // Protocol methods for peripheral classes to use
  void send_bytes(const uint8_t *data, size_t len);
  std::vector<uint8_t> recv_response(ApiCommand expected_command);
  std::vector<uint8_t> send_command(ApiCommand commandId, const std::vector<uint8_t> &payload);
};

} // namespace renode
