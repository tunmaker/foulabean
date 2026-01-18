#include <zmqInterface.hpp>

ZmqClient::ZmqClient(const std::string& server_address) : context_(1), socket_(context_, ZMQ_REQ), server_address_(server_address), is_loop_running_(false) {
    // Connect to the server
    try {
        socket_.connect(server_address_);
        std::cout << "Connection successful." << std::endl;
    } 
    catch (const std::exception& e) {
        std::cerr << "Failed to connect: " << e.what() << std::endl;
    }

}

ZmqClient::~ZmqClient() {
    if (loop_thread_.joinable()) {
        is_loop_running_ = false;
        loop_thread_.join();
    }
}
// Get the device handle
std::string ZmqClient::get_device_handle() {
    std::string command = "GET_DEVICE_HANDLE";
    zmq::message_t msg(command.c_str(), command.size());
    socket_.send(msg);
    zmq::message_t reply;
    socket_.recv(&reply);
    return std::string(static_cast<char*>(reply.data()), reply.size());
}
// Send an ADC command and get the reply
std::string ZmqClient::send_adc_command(int channel) {
    std::string command = "ADC " + std::to_string(channel);
    zmq::message_t msg(command.c_str(), command.size());
    socket_.send(msg);
    zmq::message_t reply;
    socket_.recv(&reply);
    return std::string(static_cast<char*>(reply.data()), reply.size());
}
// Start the ADC data collection loop
void ZmqClient::start_adc_loop() {
    is_loop_running_ = true;
    loop_thread_ = std::thread([this]() {
        while (is_loop_running_) {
            std::string reply = send_adc_command(0); // Assuming channel 0
            std::cout << "Received ADC data: " << reply << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
}