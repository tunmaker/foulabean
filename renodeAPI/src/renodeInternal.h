// renodeInternal.h
// Internal header for exposing ExternalControlClient::Impl to renodeMachine.cpp
#pragma once

#include "renodeInterface.h"
#include <map>
#include <mutex>

namespace renode {

// Forward declare AMachine so we can reference it
class AMachine;

// ExternalControlClient::Impl definition exposed for use in renodeMachine.cpp
struct ExternalControlClient::Impl {
  std::string host;
  uint16_t port;
  int sock_fd = -1;  // Socket file descriptor
  bool connected = false;
  std::mutex mtx;

  // Cache of machines
  std::map<std::string, std::weak_ptr<AMachine>> machines;

  Impl(const std::string &h, uint16_t p) : host(h), port(p) {}

  // Protocol methods for peripheral classes to use
  void send_bytes(const uint8_t *data, size_t len);
  std::vector<uint8_t> recv_response(ApiCommand expected_command);
  std::vector<uint8_t> send_command(ApiCommand commandId, const std::vector<uint8_t> &payload);
};

} // namespace renode
