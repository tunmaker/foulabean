#pragma once

#include <QObject>
#include <QString>
#include <QVector>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "BridgeTypes.h"

namespace renode {
class ExternalControlClient;
class AMachine;
class Gpio;
class Adc;
enum class GpioState : uint8_t;
}

class RenodeEventDispatcher;

class RenodeWorker : public QObject {
    Q_OBJECT

public:
    explicit RenodeWorker(QObject *parent = nullptr);
    ~RenodeWorker();

public slots:
    void doConnect(QString renodePath, QString scriptPath,
                   QString host, int port, int monitorPort,
                   int timeoutMs, QString machineName);
    void doDisconnect();
    void doRunFor(quint64 duration, int timeUnitValue);
    void doPause();
    void doResume();
    void doReset();
    void doRefreshGpio(QString peripheralPath, int pinCount);
    void doSetGpioPin(QString peripheralPath, int pin, int state);
    void doRefreshAdc(QString peripheralPath);
    void doSetAdcChannel(QString peripheralPath, int channel, double value);
    void doGetTime();
    void doDiscoverPeripherals();

signals:
    void connected(QString machineName, QString machineId);
    void connectionFailed(QString errorMessage);
    void disconnected();

    void simulationTimeUpdated(quint64 timeMicroseconds);
    void runForCompleted();
    void runForFailed(QString errorMessage);

    void paused();
    void resumed();
    void resetDone();
    void operationFailed(QString operation, QString errorMessage);

    void gpioStatesUpdated(QString peripheralPath,
                           QVector<GpioPinData> pins);
    void gpioPinChanged(QString peripheralPath, int pin, int newState);
    void adcDataUpdated(QString peripheralPath,
                        int channelCount,
                        QVector<AdcChannelData> channels);
    void peripheralsDiscovered(DiscoveredPeripherals discovered);

private:
    std::unique_ptr<renode::ExternalControlClient> m_client;
    std::shared_ptr<renode::AMachine> m_machine;

    std::map<std::string, std::shared_ptr<renode::Gpio>> m_gpios;
    std::map<std::string, std::shared_ptr<renode::Adc>> m_adcs;

    std::vector<std::pair<std::string, int>> m_gpioCallbackHandles;

    void registerGpioCallbacks(const std::string &path,
                               std::shared_ptr<renode::Gpio> gpio,
                               int pinCount);
    void cleanupCallbacks();

    RenodeEventDispatcher *m_eventDispatcher = nullptr;
};
