// renodeInterface.h
#pragma once

#include <cstdint>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "defs.h"

namespace renode {

// Forward declarations
class AMachine;

// Fatal exception for unrecoverable errors
class RenodeException : public std::runtime_error {
public:
  explicit RenodeException(const std::string &msg) : std::runtime_error(msg) {}
};

class ExternalControlClient {
public:
  // Non-copyable
  ExternalControlClient(const ExternalControlClient &) = delete;
  ExternalControlClient &operator=(const ExternalControlClient &) = delete;

  // Movable
  ExternalControlClient(ExternalControlClient &&) noexcept;
  ExternalControlClient &operator=(ExternalControlClient &&) noexcept;

  // Destructor will close connection and free resources.
  ~ExternalControlClient();
  struct Impl;

  // *Connect to renode server on host:port. Throws RenodeException on failure.
  static std::unique_ptr<ExternalControlClient>
  connect(const std::string &host = "127.0.0.1", uint16_t port = 5555);

  // *Disconnect explicitly, Destructor will disconnect.
  void disconnect() noexcept;

  // *Handshake: vector of (commandId, version)
  bool performHandshake();

  // Get machine by name. Returns nullptr if not found (no exception); error
  // populated in err.
  std::shared_ptr<AMachine> getMachine(const std::string &name,
                                       Error &err) noexcept;

  // Get machine or throw if not found.
  std::shared_ptr<AMachine> getMachineOrThrow(const std::string &name);

  // Run emulation for `duration` in given unit. Returns Error on failure.
  Error runFor(uint64_t duration, TimeUnit unit) noexcept;

  // Async runFor — returns future with Error.
  std::future<Error> asyncRunFor(uint64_t duration, TimeUnit unit);

  // Get current emulation time in microseconds. Returns std::nullopt on error.
  std::optional<uint64_t> getCurrentTimeMicroseconds(Error &err) noexcept;

  // Get current emulation time with unit conversion helper
  Result<uint64_t> getCurrentTime(uint64_t &outValue, TimeUnit unit) noexcept;
  
private:
  void send_bytes(const uint8_t *data, size_t len);
  std::vector<uint8_t> recv_response(ApiCommand expected_command);
  std::vector<uint8_t> send_command(ApiCommand commandId, const std::vector<uint8_t> &payload);
  static std::string bytes_to_string(const std::vector<uint8_t> &v);

private:
  std::unique_ptr<Impl> pimpl_;
  explicit ExternalControlClient(std::unique_ptr<Impl> impl) noexcept;
};


} // namespace renode