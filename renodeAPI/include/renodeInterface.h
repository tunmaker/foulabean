#pragma once

#include <string>
#include <utility>
#include <vector>
#include <cstdint>

constexpr uint8_t RUN_FOR     = 0x01;
constexpr uint8_t GET_TIME    = 0x02;
constexpr uint8_t GET_MACHINE = 0x03;
constexpr uint8_t ADC         = 0x04;
constexpr uint8_t GPIO        = 0x05;
constexpr uint8_t SYSTEM_BUS  = 0x06;

class ExternalControlClient {
public:
    ExternalControlClient(const std::string& server_address);
    ~ExternalControlClient();

    bool init();
    // Send a command and receive the payload bytes (empty vector if none or on error)
    std::vector<uint8_t> send_command(uint8_t commandId, const std::vector<uint8_t>& payload);

    // Hex printable representation (static helper)
    static std::string bytes_to_string(const std::vector<uint8_t>& v);

private:
    // Handshake: vector of (commandId, version)
    bool handshake_activate(const std::vector<std::pair<uint8_t,uint8_t>>& activations);
    void send_bytes(const uint8_t* data, size_t len);
    std::vector<uint8_t> recv_response(uint8_t expected_command = 0xFF);
    int sock_fd_;

    std::vector<std::pair<uint8_t,uint8_t>> command_versions = {
    //{0x00, 0x00} // reserved for size  
    {RUN_FOR, 0x0}      // 0x01, version 0  
    ,{GET_TIME, 0x0}     // 0x02, version 0  
    ,{GET_MACHINE, 0x0}  // 0x03, version 0  
    ,{ADC, 0x0}          // 0x04, version 0  
    ,{GPIO, 0x1}         // 0x05, version 1  
    ,{SYSTEM_BUS, 0x0}};   // 0x06, version 0  
    
};
