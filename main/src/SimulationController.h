#pragma once

#include <QObject>
#include <QThread>
#include <QString>
#include <QVector>
#include <QtQml/qqmlregistration.h>

#include "BridgeTypes.h"

class GpioModel;
class AdcModel;
class RenodeWorker;

class SimulationController : public QObject {
    Q_OBJECT
    QML_ELEMENT

    // Connection state
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged FINAL)
    Q_PROPERTY(bool connecting READ connecting NOTIFY connectingChanged FINAL)
    Q_PROPERTY(QString connectionError READ connectionError
               NOTIFY connectionErrorChanged FINAL)

    // Machine metadata
    Q_PROPERTY(QString machineName READ machineName NOTIFY machineNameChanged FINAL)
    Q_PROPERTY(QString machineId READ machineId NOTIFY machineIdChanged FINAL)

    // Simulation state
    Q_PROPERTY(bool running READ running NOTIFY runningChanged FINAL)
    Q_PROPERTY(quint64 simulationTimeUs READ simulationTimeUs
               NOTIFY simulationTimeUsChanged FINAL)
    Q_PROPERTY(QString simulationTimeFormatted READ simulationTimeFormatted
               NOTIFY simulationTimeUsChanged FINAL)

    // Peripheral models
    Q_PROPERTY(GpioModel *gpioModel READ gpioModel CONSTANT FINAL)
    Q_PROPERTY(AdcModel *adcModel READ adcModel CONSTANT FINAL)

public:
    explicit SimulationController(QObject *parent = nullptr);
    ~SimulationController() override;

    // Property getters
    bool connected() const;
    bool connecting() const;
    QString connectionError() const;
    QString machineName() const;
    QString machineId() const;
    bool running() const;
    quint64 simulationTimeUs() const;
    QString simulationTimeFormatted() const;
    GpioModel *gpioModel() const;
    AdcModel *adcModel() const;

    // User-triggered actions
    Q_INVOKABLE void connectToRenode(const QString &renodePath,
                                     const QString &scriptPath,
                                     const QString &host = QStringLiteral("127.0.0.1"),
                                     int port = 5555,
                                     int monitorPort = 5556,
                                     int timeoutMs = 15000,
                                     const QString &machineName = QStringLiteral("stm32-machine"));
    Q_INVOKABLE void disconnect();
    Q_INVOKABLE void runFor(quint64 durationMs);
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void reset();
    Q_INVOKABLE void setGpioPin(int pin, int state);
    Q_INVOKABLE void setAdcChannel(int channel, double value);
    Q_INVOKABLE void refreshPeripherals();

signals:
    // Property change notifications
    void connectedChanged();
    void connectingChanged();
    void connectionErrorChanged();
    void machineNameChanged();
    void machineIdChanged();
    void runningChanged();
    void simulationTimeUsChanged();

    // Internal signals forwarded to worker
    void requestConnect(QString renodePath, QString scriptPath,
                        QString host, int port, int monitorPort,
                        int timeoutMs, QString machineName);
    void requestDisconnect();
    void requestRunFor(quint64 duration, int timeUnitValue);
    void requestPause();
    void requestResume();
    void requestReset();
    void requestDiscoverPeripherals();
    void requestRefreshGpio(QString peripheralPath, int pinCount);
    void requestSetGpioPin(QString peripheralPath, int pin, int state);
    void requestRefreshAdc(QString peripheralPath);
    void requestSetAdcChannel(QString peripheralPath, int channel, double value);
    void requestGetTime();

private slots:
    void onConnected(QString machineName, QString machineId);
    void onConnectionFailed(QString errorMessage);
    void onDisconnected();
    void onSimulationTimeUpdated(quint64 timeMicroseconds);
    void onRunForCompleted();
    void onRunForFailed(QString errorMessage);
    void onPaused();
    void onResumed();
    void onResetDone();
    void onOperationFailed(QString operation, QString errorMessage);
    void onGpioStatesUpdated(QString peripheralPath,
                             QVector<GpioPinData> pins);
    void onGpioPinChanged(QString peripheralPath, int pin, int newState);
    void onAdcDataUpdated(QString peripheralPath,
                          int channelCount,
                          QVector<AdcChannelData> channels);
    void onPeripheralsDiscovered(DiscoveredPeripherals discovered);

private:
    void setupWorkerConnections();

    // State
    bool m_connected = false;
    bool m_connecting = false;
    QString m_connectionError;
    QString m_machineName;
    QString m_machineId;
    bool m_running = false;
    quint64 m_simulationTimeUs = 0;

    // Peripheral parameters — populated by doDiscoverPeripherals
    int m_gpioPinCount = 0;
    QString m_gpioPath;
    QString m_adcPath;

    // Owned objects
    GpioModel *m_gpioModel = nullptr;
    AdcModel *m_adcModel = nullptr;
    QThread m_workerThread;
    RenodeWorker *m_worker = nullptr;
};
