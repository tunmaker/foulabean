#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include "zmqInterface.hpp"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("digitwin", "Main");

    ZmqClient client("tcp://localhost:5555");

    std::string device_handle = client.get_device_handle(); //blocking
    std::cout << "Device Handle: " << device_handle << std::endl;

    client.start_adc_loop();

    // Keep the program alive for a while to see the ADC loop output
    //std::this_thread::sleep_for(std::chrono::seconds(10));

    return app.exec();
}
