#pragma once

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <zmq.hpp>

class ZmqClient {
public:
    ZmqClient(const std::string& server_address);
    ~ZmqClient();
    // Get the device handle
    std::string get_device_handle();

    // Send an ADC command and get the reply
    std::string send_adc_command(int channel);

    // Start the ADC data collection loop
    void start_adc_loop();
private:
    zmq::context_t context_;
    zmq::socket_t socket_;
    std::string server_address_;
    bool is_loop_running_;
    std::thread loop_thread_;
};