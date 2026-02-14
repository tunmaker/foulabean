// renodeInterface.cpp
#include "renodeInterface.h"
#include "renodeMachine.h"
#include "renodeInternal.h"
#include "defs.h"

#include <arpa/inet.h>
#include <charconv>
#include <cstdint>
#include <memory>
#include <netdb.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>

#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>
#include <map>

namespace renode {

// ============================================================================
// RenodeProcess Implementation
// ============================================================================

RenodeProcess::RenodeProcess(pid_t pid, uint16_t port) noexcept
    : pid_(pid), port_(port) {}

RenodeProcess::RenodeProcess(RenodeProcess &&other) noexcept
    : pid_(other.pid_), port_(other.port_) {
  other.pid_ = -1;
}

RenodeProcess &RenodeProcess::operator=(RenodeProcess &&other) noexcept {
  if (this != &other) {
    terminate();
    pid_ = other.pid_;
    port_ = other.port_;
    other.pid_ = -1;
  }
  return *this;
}

RenodeProcess::~RenodeProcess() {
  terminate();
}

bool RenodeProcess::isRunning() const noexcept {
  if (pid_ <= 0) return false;
  int status;
  pid_t result = waitpid(pid_, &status, WNOHANG);
  return result == 0;  // 0 means still running
}

void RenodeProcess::terminate() noexcept {
  if (pid_ <= 0) return;

  // Send SIGTERM first
  if (kill(pid_, SIGTERM) == 0) {
    // Wait up to 2 seconds for graceful shutdown
    for (int i = 0; i < 20; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      int status;
      if (waitpid(pid_, &status, WNOHANG) != 0) {
        pid_ = -1;
        return;
      }
    }
    // Force kill if still running
    kill(pid_, SIGKILL);
    waitpid(pid_, nullptr, 0);
  }
  pid_ = -1;
}

std::unique_ptr<RenodeProcess> RenodeProcess::launch(const RenodeConfig &config) {
  // Build command arguments
  std::vector<std::string> args_storage;
  args_storage.push_back(config.renode_path);

  if (config.console_mode) {
    args_storage.push_back("--console");
  }
  if (config.disable_gui) {
    args_storage.push_back("--disable-gui");
  }
  if (config.monitor_port > 0) {
    args_storage.push_back("--port");
    args_storage.push_back(std::to_string(config.monitor_port));
  }
  if (!config.script_path.empty()) {
    args_storage.push_back(config.script_path);
  }

  // Convert to char* array for execvp
  std::vector<char*> argv;
  for (auto &arg : args_storage) {
    argv.push_back(const_cast<char*>(arg.c_str()));
  }
  argv.push_back(nullptr);

  pid_t pid = fork();
  if (pid < 0) {
    std::cerr << "RenodeProcess: fork() failed: " << strerror(errno) << "\n";
    return nullptr;
  }

  if (pid == 0) {
    // Child process
    // Redirect stdout/stderr to /dev/null or keep for debugging
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull >= 0) {
      dup2(devnull, STDOUT_FILENO);
      dup2(devnull, STDERR_FILENO);
      close(devnull);
    }

    execvp(argv[0], argv.data());
    // If execvp returns, it failed
    std::cerr << "RenodeProcess: execvp failed: " << strerror(errno) << "\n";
    _exit(127);
  }

  // Parent process - wait for Renode to start listening
  auto process = std::unique_ptr<RenodeProcess>(new RenodeProcess(pid, config.port));

  // Poll until we can connect or timeout
  auto start = std::chrono::steady_clock::now();
  while (true) {
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    if (elapsed.count() >= config.startup_timeout_ms) {
      std::cerr << "RenodeProcess: timeout waiting for Renode to start\n";
      process->terminate();
      return nullptr;
    }

    // Check if process died
    if (!process->isRunning()) {
      std::cerr << "RenodeProcess: Renode process exited unexpectedly\n";
      return nullptr;
    }

    // Try to connect to the external control port
    int test_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (test_fd >= 0) {
      struct sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_port = htons(config.port);
      inet_pton(AF_INET, config.host.c_str(), &addr.sin_addr);

      if (::connect(test_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0) {
        close(test_fd);

        // Also check monitor port if configured
        bool monitor_ready = true;
        if (config.monitor_port > 0) {
          int mon_fd = socket(AF_INET, SOCK_STREAM, 0);
          if (mon_fd >= 0) {
            addr.sin_port = htons(config.monitor_port);
            monitor_ready = (::connect(mon_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0);
            close(mon_fd);
          }
        }

        if (monitor_ready) {
          std::cout << "RenodeProcess: Renode started successfully (pid=" << pid << ")\n";
          return process;
        }
      } else {
        close(test_fd);
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

// ============================================================================
// ExternalControlClient Implementation
// ============================================================================

std::unique_ptr<ExternalControlClient>
ExternalControlClient::launchAndConnect(const RenodeConfig &config) {
  // Launch Renode process
  auto process = RenodeProcess::launch(config);
  if (!process) {
    throw RenodeException("Failed to launch Renode process");
  }

  // Connect to it
  auto impl = std::make_unique<Impl>(config.host, config.port);
  struct addrinfo hints{};
  struct addrinfo *res = nullptr;

  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_UNSPEC;

  int rc = getaddrinfo(config.host.c_str(), std::to_string(config.port).c_str(), &hints, &res);
  if (rc != 0 || !res) {
    throw RenodeException(std::string("getaddrinfo: ") + gai_strerror(rc));
  }

  for (struct addrinfo *ai = res; ai != nullptr; ai = ai->ai_next) {
    impl->sock_fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (impl->sock_fd < 0)
      continue;
    if (::connect(impl->sock_fd, ai->ai_addr, ai->ai_addrlen) == 0) {
      impl->connected = true;
      freeaddrinfo(res);

      // Return client without monitor - connectMonitor() should be called after handshake
      return std::unique_ptr<ExternalControlClient>(
          new ExternalControlClient(std::move(impl), std::move(process), nullptr));
    }
    close(impl->sock_fd);
    impl->sock_fd = -1;
  }

  freeaddrinfo(res);
  throw RenodeException("launchAndConnect: unable to connect to Renode");
}

std::unique_ptr<ExternalControlClient>
ExternalControlClient::connect(const std::string &host, uint16_t port) {

  auto impl = std::make_unique<Impl>(host, port);
  struct addrinfo hints;
  struct addrinfo *res = nullptr;
  // int sock_fd_;
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_UNSPEC;

  int rc =
      getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res);
  if (rc != 0 || !res) {
    throw std::runtime_error(std::string("getaddrinfo: ") + gai_strerror(rc));
  }

  for (struct addrinfo *ai = res; ai != nullptr; ai = ai->ai_next) {
    impl->sock_fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (impl->sock_fd < 0)
      continue;
    if (::connect(impl->sock_fd, ai->ai_addr, ai->ai_addrlen) == 0) {
      impl->connected = true;
      freeaddrinfo(res);
      return std::unique_ptr<ExternalControlClient>(
          new ExternalControlClient(std::move(impl)));
    }
    close(impl->sock_fd);
    impl->sock_fd = -1;
  }

  freeaddrinfo(res);
  throw std::runtime_error("ExternalControlClient: unable to connect");

  // ToDo query server for machines, but we need handshake first ?
  return nullptr;
}

ExternalControlClient::ExternalControlClient(
    std::unique_ptr<Impl> impl) noexcept
    : pimpl_(std::move(impl)) {}

ExternalControlClient::ExternalControlClient(
    std::unique_ptr<Impl> impl,
    std::unique_ptr<RenodeProcess> process,
    std::unique_ptr<Monitor> monitor) noexcept
    : pimpl_(std::move(impl)), process_(std::move(process)), monitor_(std::move(monitor)) {
  // Set the monitor pointer in Impl for AMachine to access
  if (pimpl_ && monitor_) {
    pimpl_->monitor = monitor_.get();
  }
}

ExternalControlClient::~ExternalControlClient() {
  disconnect();
  // process_ destructor will terminate Renode if we own it
}

void ExternalControlClient::disconnect() noexcept {
  if (!pimpl_) return;
  if (pimpl_->sock_fd >= 0) {
    close(pimpl_->sock_fd);
    pimpl_->sock_fd = -1;
    std::cout << "disconnected cleanly." << '\n';
  }
  pimpl_->connected = false;
}

Monitor* ExternalControlClient::getMonitor() noexcept {
  return monitor_.get();
}

bool ExternalControlClient::connectMonitor(const std::string &host, uint16_t port) {
  if (monitor_) {
    return true;  // Already connected
  }

  try {
    monitor_ = Monitor::connect(host, port);
    if (pimpl_ && monitor_) {
      pimpl_->monitor = monitor_.get();
    }
    return true;
  } catch (const std::exception &e) {
    std::cerr << "Failed to connect to monitor: " << e.what() << "\n";
    return false;
  }
}

bool ExternalControlClient::performHandshake() {

  if (command_versions.size() > UINT16_MAX)
    return false;
  std::vector<uint8_t> buf;
  write_u16_le(buf, static_cast<uint16_t>(command_versions.size()));
  for (auto &p : command_versions) {
    buf.push_back(p.first);
    buf.push_back(p.second);
  }
  send_bytes(buf.data(), buf.size());

  // Read single-byte server response for handshake
  uint8_t response = 0;
  if (!read_byte(pimpl_->sock_fd, response)) {
    std::cerr << "handshake: failed to read handshake response\n";
    return false;
  }

  if (response != renode_return_code::OK_HANDSHAKE) {
    // If server sent an error return code, try to read error payload following
    // protocol: many error codes send an echoed command byte + 4-byte size +
    // payload; but handshake uses single-byte response on success. To
    // best-effort parse an error, read next byte if available (non-blocking
    // would be nicer).
    std::cerr << "handshake: unexpected handshake response 0x" << std::hex
              << int(response) << std::dec << "\n";
    return false;
  }

  return true;
}

// Build and send header+payload, then parse the response payload bytes and
// return them. expected_command is used to assert server echoed command (not
// enforced if 0xFF)
std::vector<uint8_t> ExternalControlClient::send_command(ApiCommand commandId, const std::vector<uint8_t> &payload) {
  return pimpl_->send_command(commandId, payload);
}

void ExternalControlClient::send_bytes(const uint8_t *data, size_t len) {
  pimpl_->send_bytes(data, len);
}

std::vector<uint8_t> ExternalControlClient::recv_response(ApiCommand expected_command) {
  return pimpl_->recv_response(expected_command);
}

// Impl method implementations
std::vector<uint8_t> ExternalControlClient::Impl::send_command(ApiCommand commandId, const std::vector<uint8_t> &payload) {
  // Build 7-byte header: 'R','E', command, data_size (4 bytes LE)
  uint8_t header[7];
  header[0] = static_cast<uint8_t>('R');
  header[1] = static_cast<uint8_t>('E');
  header[2] = (int8_t)commandId;
  uint32_t data_size = static_cast<uint32_t>(payload.size());
  header[3] = static_cast<uint8_t>(data_size & 0xFF);
  header[4] = static_cast<uint8_t>((data_size >> 8) & 0xFF);
  header[5] = static_cast<uint8_t>((data_size >> 16) & 0xFF);
  header[6] = static_cast<uint8_t>((data_size >> 24) & 0xFF);

  // Send header then payload
  send_bytes(header, sizeof(header));
  if (!payload.empty())
    send_bytes(payload.data(), payload.size());

  // Receive and return the response payload
  return recv_response(commandId);
}

void ExternalControlClient::Impl::send_bytes(const uint8_t *data, size_t len) {
  if (sock_fd < 0)
    throw std::runtime_error("socket closed");
  if (!write_all(sock_fd, data, len)) {
    throw std::runtime_error("send_bytes: write failed");
  }
}

std::vector<uint8_t> ExternalControlClient::Impl::recv_response(ApiCommand expected_command) {
  if (sock_fd < 0)
    throw std::runtime_error("socket closed");

  auto safe_read_size = [this](uint32_t &out_size) -> bool {
    uint8_t sizebuf[4];
    if (!read_all(sock_fd, sizebuf, 4))
      return false;
    out_size = uint32_t(sizebuf[0]) | (uint32_t(sizebuf[1]) << 8) |
               (uint32_t(sizebuf[2]) << 16) | (uint32_t(sizebuf[3]) << 24);
    return true;
  };

  // Loop to handle ASYNC_EVENT responses (e.g., GPIO callbacks during runFor)
  while (true) {
    uint8_t return_code = 0;
    if (!read_all(sock_fd, &return_code, 1)) {
      throw std::runtime_error("recv_response: failed to read return code");
    }

    // Handle ASYNC_EVENT: invoke callback and continue waiting for actual response
    if (return_code == ASYNC_EVENT) {
      // Parse event: command(1B) + ed(4B) + size(4B) + data(size bytes)
      uint8_t event_command = 0;
      if (!read_all(sock_fd, &event_command, 1)) {
        throw std::runtime_error("recv_response: failed to read event command");
      }

      uint32_t event_ed = 0;
      if (!safe_read_size(event_ed)) {
        throw std::runtime_error("recv_response: failed to read event descriptor");
      }

      uint32_t event_size = 0;
      if (!safe_read_size(event_size)) {
        throw std::runtime_error("recv_response: failed to read event size");
      }

      std::vector<uint8_t> event_data;
      if (event_size > 0) {
        event_data.resize(event_size);
        if (!read_all(sock_fd, event_data.data(), event_size)) {
          throw std::runtime_error("recv_response: failed to read event data");
        }
      }

      // Invoke the registered callback
      EventCallbackRegistry::instance().invokeCallback(event_ed, event_data.data(), event_data.size());

      // Continue loop to read the actual response
      continue;
    }

    uint8_t received_command = 0xFF;
    // For many codes we read the echoed command
    if (return_code == COMMAND_FAILED || return_code == INVALID_COMMAND ||
        return_code == SUCCESS_WITH_DATA || return_code == SUCCESS_WITHOUT_DATA) {
      if (!read_all(sock_fd, &received_command, 1)) {
        throw std::runtime_error("recv_response: failed to read echoed command");
      }
    }

    uint32_t data_size = 0;
    std::vector<uint8_t> payload;

    switch (return_code) {
    case COMMAND_FAILED:
    case FATAL_ERROR:
    case SUCCESS_WITH_DATA:
      if (!safe_read_size(data_size)) {
        std::cerr << "recv_response: truncated data_size (return_code=0x"
                  << std::hex << int(return_code) << std::dec << ")\n";
        return {};
      }
      if (data_size) {
        payload.resize(data_size);
        if (!read_all(sock_fd, payload.data(), data_size)) {
          std::cerr << "recv_response: truncated payload (expected " << data_size
                    << " bytes)\n";
          return {};
        }
      }
      break;
    case INVALID_COMMAND:
    case SUCCESS_WITHOUT_DATA:
      data_size = 0;
      break;
    default:
      std::cerr << "recv_response: unexpected return code " << int(return_code) << "\n";
    }

    // Validate echoed command if requested
    if (received_command != 0xFF && received_command != static_cast<uint8_t>(expected_command)) {
      throw std::runtime_error(
          "recv_response: command mismatch (server echoed different command)");
    }

    return payload;
  }
}

std::string ExternalControlClient::bytes_to_string(const std::vector<uint8_t> &v) {
  static const char *hex = "0123456789abcdef";
  std::string s;
  s.reserve(v.size() * 2);
  for (uint8_t b : v) {
    s.push_back(hex[(b >> 4) & 0xF]);
    s.push_back(hex[b & 0xF]);
  }
  return s;
}

// Move constructors implementation
ExternalControlClient::ExternalControlClient(ExternalControlClient&& other) noexcept = default;
ExternalControlClient& ExternalControlClient::operator=(ExternalControlClient&&) noexcept = default;

// ============================================================================
// Monitor Implementation
// ============================================================================

struct Monitor::Impl {
  int sock_fd = -1;
  std::string host;
  uint16_t port;

  Impl(const std::string &h, uint16_t p) : host(h), port(p) {}

  ~Impl() {
    if (sock_fd >= 0) {
      close(sock_fd);
    }
  }

  // Read until we get a monitor prompt like "(monitor) " or "(machine-name) "
  std::string readUntilPrompt() {
    std::string result;
    char buf[256];

    // std::cerr << "[Monitor] readUntilPrompt: waiting for data...\n";
    while (true) {
      ssize_t n = recv(sock_fd, buf, sizeof(buf) - 1, 0);
      if (n <= 0) {
        std::cerr << "[Monitor] recv returned " << n << " (errno=" << errno << ")\n";
        break;
      }
      buf[n] = '\0';
      result += buf;
      // std::cerr << "[Monitor] received " << n << " bytes, total=" << result.size()
      //           << ", last chars: \"" << result.substr(result.size() > 20 ? result.size() - 20 : 0) << "\"\n";

      // Check if we've received a prompt pattern: (something) followed by space
      // The prompt often has trailing space and may have ANSI codes
      // Look for ") " pattern near the end of the buffer
      size_t promptMarker = result.rfind(") ");
      if (promptMarker != std::string::npos) {
        // Found ") ", now find the matching "("
        size_t openPos = result.rfind('(', promptMarker);
        if (openPos != std::string::npos && promptMarker > openPos) {
          // std::cerr << "[Monitor] found prompt at pos " << openPos << " to " << promptMarker << "\n";
          // Remove the prompt (and any ANSI codes before it) from output
          // Find the start of the line containing the prompt
          size_t lineStart = result.rfind('\n', openPos);
          if (lineStart == std::string::npos) lineStart = 0;
          else lineStart++; // Skip the newline itself
          result = result.substr(0, lineStart);
          break;
        }
      }
    }
    // std::cerr << "[Monitor] readUntilPrompt done, result length=" << result.size() << "\n";
    return result;
  }

  // Send command and read response
  Result<std::string> sendCommand(const std::string &cmd) {
    std::string cmdLine = cmd + "\n";
    if (send(sock_fd, cmdLine.c_str(), cmdLine.size(), 0) < 0) {
      return {"", {1, "Failed to send command"}};
    }

    std::string response = readUntilPrompt();

    // Strip leading newline and the echoed command if present
    size_t start = 0;
    if (!response.empty() && response[0] == '\n') start = 1;

    // Skip echoed command line
    size_t cmdEnd = response.find('\n', start);
    if (cmdEnd != std::string::npos) {
      start = cmdEnd + 1;
    }

    // Trim trailing whitespace
    size_t end = response.size();
    while (end > start && (response[end-1] == '\n' || response[end-1] == '\r' ||
                           response[end-1] == ' ')) {
      --end;
    }

    return {response.substr(start, end - start), {0, ""}};
  }
};

Monitor::Monitor(std::unique_ptr<Impl> impl) noexcept : pimpl_(std::move(impl)) {}

Monitor::Monitor(Monitor &&other) noexcept = default;
Monitor &Monitor::operator=(Monitor &&other) noexcept = default;
Monitor::~Monitor() = default;

std::unique_ptr<Monitor> Monitor::connect(const std::string &host, uint16_t port) {
  auto impl = std::make_unique<Impl>(host, port);

  struct addrinfo hints{};
  struct addrinfo *res = nullptr;

  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_UNSPEC;

  int rc = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res);
  if (rc != 0 || !res) {
    throw RenodeException(std::string("Monitor getaddrinfo: ") + gai_strerror(rc));
  }

  for (struct addrinfo *ai = res; ai != nullptr; ai = ai->ai_next) {
    impl->sock_fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (impl->sock_fd < 0)
      continue;
    if (::connect(impl->sock_fd, ai->ai_addr, ai->ai_addrlen) == 0) {
      freeaddrinfo(res);

      // Read initial prompt
      impl->readUntilPrompt();

      return std::unique_ptr<Monitor>(new Monitor(std::move(impl)));
    }
    close(impl->sock_fd);
    impl->sock_fd = -1;
  }

  freeaddrinfo(res);
  throw RenodeException("Monitor: unable to connect to " + host + ":" + std::to_string(port));
}

Result<std::string> Monitor::execute(const std::string &command) noexcept {
  if (!pimpl_ || pimpl_->sock_fd < 0) {
    return {"", {1, "Not connected"}};
  }
  return pimpl_->sendCommand(command);
}

Error Monitor::loadPlatformDescription(const std::string &path) noexcept {
  auto result = execute("machine LoadPlatformDescription @" + path);
  return result.error;
}

Error Monitor::loadELF(const std::string &path) noexcept {
  auto result = execute("sysbus LoadELF @" + path);
  return result.error;
}

Error Monitor::pause() noexcept {
  auto result = execute("pause");
  return result.error;
}

Error Monitor::start() noexcept {
  auto result = execute("start");
  return result.error;
}

Error Monitor::reset() noexcept {
  auto result = execute("machine Reset");
  return result.error;
}

} // namespace renode