#include <zmq.hpp>
#include <iostream>
#include <thread>
#include <chrono>

int zmqInterface() {
    // Initialize ZeroMQ context
    zmq::context_t context;
    zmq::socket_t socket(context, ZMQ_REQ);
    socket.connect("tcp://localhost:5555"); // Connect to the server

    // Step 1: Retrieve the device handle
    std::string cmd = "GetMachine my_target";
    zmq::message_t msg(cmd.c_str(), cmd.size());
    socket.send(msg);

    zmq::message_t reply;
    socket.recv(&reply);
    std::string handle(static_cast<char*>(reply.data()), reply.size());
    std::cout << "Received device handle: " << handle << std::endl;

    // Step 2: Real-time data retrieval loop
    while (true) {
        // Send ADC command (e.g., channel 0)
        std::string cmd = "ADC " + handle + " 0";
        zmq::message_t msg_adc(cmd.c_str(), cmd.size());
        socket.send(msg_adc);

        // Receive ADC value
        socket.recv(&reply);
        std::string adc_value(static_cast<char*>(reply.data()), reply.size());
        std::cout << "ADC Value: " << adc_value << std::endl;

        // Optional: Add delay for real-time monitoring
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    return 0;
}