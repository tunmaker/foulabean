// renodeInterface.h
#pragma once

#include <cstdint>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <sys/types.h>

#include "defs.h"

namespace renode {

// Forward declarations
class AMachine;
class Monitor;

// Configuration for launching Renode subprocess
struct RenodeConfig {
  std::string renode_path;         // Path to renode executable
  std::string script_path;         // .resc script to load (optional)
  std::string host = "127.0.0.1";  // Host to connect to
  uint16_t port = 5555;            // External control port
  uint16_t monitor_port = 5556;    // Monitor telnet port (0 to disable)
  bool console_mode = false;        // --console flag
  bool disable_gui = false;         // --disable-gui flag
  int startup_timeout_ms = 10000;  // Max time to wait for Renode to start
};

// RAII wrapper for Renode subprocess
class RenodeProcess {
public:
  // Non-copyable
  RenodeProcess(const RenodeProcess &) = delete;
  RenodeProcess &operator=(const RenodeProcess &) = delete;

  // Movable
  RenodeProcess(RenodeProcess &&other) noexcept;
  RenodeProcess &operator=(RenodeProcess &&other) noexcept;

  // Destructor terminates process
  ~RenodeProcess();

  // Launch Renode with given config. Returns nullptr on failure.
  static std::unique_ptr<RenodeProcess> launch(const RenodeConfig &config);

  // Check if process is still running
  bool isRunning() const noexcept;

  // Terminate the process (SIGTERM, then SIGKILL after timeout)
  void terminate() noexcept;

  // Get process ID
  pid_t pid() const noexcept { return pid_; }

  // Get the port Renode is listening on
  uint16_t port() const noexcept { return port_; }

private:
  explicit RenodeProcess(pid_t pid, uint16_t port) noexcept;
  pid_t pid_ = -1;
  uint16_t port_ = 5555;
};

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

  // Launch Renode and connect. Owns the Renode process (kills on destruction).
  static std::unique_ptr<ExternalControlClient>
  launchAndConnect(const RenodeConfig &config);

  // *Disconnect explicitly, Destructor will disconnect.
  void disconnect() noexcept;

  // Get the Monitor connection (if available). Returns nullptr if not connected.
  Monitor* getMonitor() noexcept;

  // Connect to monitor socket (call after handshake succeeds)
  bool connectMonitor(const std::string &host = "127.0.0.1", uint16_t port = 5556);

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

  // Returns the raw socket file descriptor for monitoring purposes.
  // The caller must NOT close or take ownership of this fd.
  int getSocketFd() const noexcept;

private:
  void send_bytes(const uint8_t *data, size_t len);
  std::vector<uint8_t> recv_response(ApiCommand expected_command);
  std::vector<uint8_t> send_command(ApiCommand commandId, const std::vector<uint8_t> &payload);
  static std::string bytes_to_string(const std::vector<uint8_t> &v);

private:
  std::unique_ptr<Impl> pimpl_;
  std::unique_ptr<RenodeProcess> process_;  // Optional: owned Renode subprocess
  std::unique_ptr<Monitor> monitor_;        // Optional: monitor connection
  explicit ExternalControlClient(std::unique_ptr<Impl> impl) noexcept;
  ExternalControlClient(std::unique_ptr<Impl> impl,
                        std::unique_ptr<RenodeProcess> process,
                        std::unique_ptr<Monitor> monitor) noexcept;
};


// Monitor: execute Renode monitor commands via telnet socket
class Monitor {
public:
  // Non-copyable
  Monitor(const Monitor &) = delete;
  Monitor &operator=(const Monitor &) = delete;

  // Movable
  Monitor(Monitor &&) noexcept;
  Monitor &operator=(Monitor &&) noexcept;

  ~Monitor();

  // Connect to Renode monitor socket
  static std::unique_ptr<Monitor> connect(const std::string &host = "127.0.0.1",
                                          uint16_t port = 5556);

  // Execute a monitor command and return the output
  Result<std::string> execute(const std::string &command) noexcept;

  // Convenience methods
  Error loadPlatformDescription(const std::string &path) noexcept;
  Error loadELF(const std::string &path) noexcept;
  Error pause() noexcept;
  Error start() noexcept;
  Error reset() noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> pimpl_;
  explicit Monitor(std::unique_ptr<Impl> impl) noexcept;
};

// Dispatch an async event by its event descriptor to registered callbacks.
// Called by the event pump (RenodeEventDispatcher) when an ASYNC_EVENT frame
// is received on the idle socket.
void dispatchEvent(uint32_t eventDescriptor, const uint8_t *data, size_t size) noexcept;

} // namespace renode