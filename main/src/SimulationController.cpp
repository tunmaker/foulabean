#include "SimulationController.h"
#include "GpioModel.h"
#include "AdcModel.h"
#include "RenodeWorker.h"

#include <QDir>

SimulationController::SimulationController(QObject *parent)
    : QObject(parent)
    , m_gpioModel(new GpioModel(this))
    , m_adcModel(new AdcModel(this))
    , m_worker(new RenodeWorker) // no parent — will be moved to thread
{
    QDir scriptsDir(QString::fromUtf8(RENODE_SCRIPTS_DIR));
    const auto entries = scriptsDir.entryInfoList({QStringLiteral("*.resc")},
                                                   QDir::Files, QDir::Name);
    for (const QFileInfo &entry : entries) {
        m_rescScriptNames << entry.fileName();
        m_rescScriptPaths << entry.absoluteFilePath();
    }

    m_worker->moveToThread(&m_workerThread);
    connect(&m_workerThread, &QThread::finished,
            m_worker, &QObject::deleteLater);
    setupWorkerConnections();
    m_workerThread.start();
}

SimulationController::~SimulationController() {
    m_workerThread.quit();
    m_workerThread.wait();
}

// --- Property getters ---

bool SimulationController::connected() const { return m_connected; }
bool SimulationController::connecting() const { return m_connecting; }
QString SimulationController::connectionError() const { return m_connectionError; }
QString SimulationController::machineName() const { return m_machineName; }
QString SimulationController::machineId() const { return m_machineId; }
bool SimulationController::running() const { return m_running; }
quint64 SimulationController::simulationTimeUs() const { return m_simulationTimeUs; }

QString SimulationController::simulationTimeFormatted() const {
    if (m_simulationTimeUs == 0)
        return QStringLiteral("0.000 ms");

    if (m_simulationTimeUs < 1000)
        return QString::number(m_simulationTimeUs) + QStringLiteral(" us");

    if (m_simulationTimeUs < 1000000) {
        double ms = static_cast<double>(m_simulationTimeUs) / 1000.0;
        return QString::number(ms, 'f', 3) + QStringLiteral(" ms");
    }

    double s = static_cast<double>(m_simulationTimeUs) / 1000000.0;
    return QString::number(s, 'f', 6) + QStringLiteral(" s");
}

GpioModel *SimulationController::gpioModel() const { return m_gpioModel; }
AdcModel *SimulationController::adcModel() const { return m_adcModel; }
QStringList SimulationController::rescScriptNames() const { return m_rescScriptNames; }
QString SimulationController::selectedScript() const { return m_selectedScript; }

void SimulationController::selectScript(int index) {
    if (index < 0 || index >= m_rescScriptPaths.size()) return;
    const QString path = m_rescScriptPaths.at(index);
    if (m_selectedScript != path) {
        m_selectedScript = path;
        emit selectedScriptChanged();
    }
}

// --- Q_INVOKABLE actions ---

void SimulationController::connectToRenode(const QString &renodePath,
                                            const QString &scriptPath,
                                            const QString &host,
                                            int port, int monitorPort,
                                            int timeoutMs,
                                            const QString &machineName) {
    if (m_connecting || m_connected) return;

    m_connecting = true;
    emit connectingChanged();
    m_connectionError.clear();
    emit connectionErrorChanged();

    emit requestConnect(renodePath, scriptPath, host, port,
                        monitorPort, timeoutMs, machineName);
}

void SimulationController::disconnect() {
    if (!m_connected && !m_connecting) return;
    emit requestDisconnect();
}

void SimulationController::runFor(quint64 durationMs) {
    if (!m_connected) return;
    // TimeUnit::TU_MILLISECONDS = 1000
    emit requestRunFor(durationMs, 1000);
}

void SimulationController::pause() {
    if (!m_connected || !m_running) return;
    emit requestPause();
}

void SimulationController::resume() {
    if (!m_connected || m_running) return;
    emit requestResume();
}

void SimulationController::reset() {
    if (!m_connected) return;
    emit requestReset();
}

void SimulationController::setGpioPin(int pin, int state) {
    if (!m_connected) return;
    emit requestSetGpioPin(m_gpioPath, pin, state);
}

void SimulationController::setAdcChannel(int channel, double value) {
    if (!m_connected) return;
    emit requestSetAdcChannel(m_adcPath, channel, value);
}

void SimulationController::refreshPeripherals() {
    if (!m_connected) return;
    emit requestDiscoverPeripherals();
}

// --- Worker result slots ---

void SimulationController::onConnected(QString machineName, QString machineId) {
    m_connecting = false;
    emit connectingChanged();

    m_connected = true;
    emit connectedChanged();

    if (m_machineName != machineName) {
        m_machineName = machineName;
        emit machineNameChanged();
    }
    if (m_machineId != machineId) {
        m_machineId = machineId;
        emit machineIdChanged();
    }

    m_running = true;
    emit runningChanged();

    // Discover peripherals (replaces hardcoded paths)
    emit requestDiscoverPeripherals();
}

void SimulationController::onConnectionFailed(QString errorMessage) {
    m_connecting = false;
    emit connectingChanged();

    if (m_connectionError != errorMessage) {
        m_connectionError = errorMessage;
        emit connectionErrorChanged();
    }
}

void SimulationController::onDisconnected() {
    m_connecting = false;
    emit connectingChanged();

    if (m_connected) {
        m_connected = false;
        emit connectedChanged();
    }
    if (m_running) {
        m_running = false;
        emit runningChanged();
    }

    m_machineName.clear();
    emit machineNameChanged();
    m_machineId.clear();
    emit machineIdChanged();
    m_simulationTimeUs = 0;
    emit simulationTimeUsChanged();

    m_gpioPath.clear();
    m_adcPath.clear();
    m_gpioPinCount = 0;
    m_gpioPorts.clear();

    m_gpioModel->resetPins({});
    m_adcModel->resetChannels(0, {});
}

void SimulationController::onSimulationTimeUpdated(quint64 timeMicroseconds) {
    if (m_simulationTimeUs == timeMicroseconds) return;
    m_simulationTimeUs = timeMicroseconds;
    emit simulationTimeUsChanged();
}

void SimulationController::onRunForCompleted() {
    // Time is already updated via onSimulationTimeUpdated
}

void SimulationController::onRunForFailed(QString errorMessage) {
    if (m_connectionError != errorMessage) {
        m_connectionError = errorMessage;
        emit connectionErrorChanged();
    }
}

void SimulationController::onPaused() {
    if (m_running) {
        m_running = false;
        emit runningChanged();
    }
}

void SimulationController::onResumed() {
    if (!m_running) {
        m_running = true;
        emit runningChanged();
    }
}

void SimulationController::onResetDone() {
    m_simulationTimeUs = 0;
    emit simulationTimeUsChanged();
    refreshPeripherals();
}

void SimulationController::onOperationFailed(QString operation,
                                              QString errorMessage) {
    Q_UNUSED(operation)
    if (m_connectionError != errorMessage) {
        m_connectionError = errorMessage;
        emit connectionErrorChanged();
    }
}

void SimulationController::onGpioStatesUpdated(QString peripheralPath,
                                                QVector<GpioPinData> pins) {
    for (int i = 0; i < m_gpioPorts.size(); ++i) {
        if (m_gpioPorts[i].path == peripheralPath) {
            m_gpioModel->setPortPins(i, m_gpioPorts[i].name, pins);
            return;
        }
    }
}

void SimulationController::onGpioPinChanged(QString peripheralPath,
                                             int pin, int newState) {
    for (int i = 0; i < m_gpioPorts.size(); ++i) {
        if (m_gpioPorts[i].path == peripheralPath) {
            m_gpioModel->updatePortPin(i, pin, newState);
            return;
        }
    }
}

void SimulationController::onAdcDataUpdated(QString peripheralPath,
                                             int channelCount,
                                             QVector<AdcChannelData> channels) {
    Q_UNUSED(peripheralPath)
    m_adcModel->resetChannels(channelCount, channels);
}

void SimulationController::onPeripheralsDiscovered(DiscoveredPeripherals discovered) {
    // Store all discovered GPIO ports (used for multi-port model updates)
    m_gpioPorts = discovered.gpioPorts;

    // Keep first-port shortcuts for setGpioPin / setAdcChannel
    if (!discovered.gpioPorts.isEmpty()) {
        m_gpioPath     = discovered.gpioPorts[0].path;
        m_gpioPinCount = discovered.gpioPorts[0].pinCount;
    }
    if (!discovered.adcPorts.isEmpty()) {
        m_adcPath = discovered.adcPorts[0].path;
    }
    // Trigger refresh for every discovered port using existing flow
    for (const auto &gp : discovered.gpioPorts)
        emit requestRefreshGpio(gp.path, gp.pinCount);
    for (const auto &ap : discovered.adcPorts)
        emit requestRefreshAdc(ap.path);
    emit requestGetTime();
}

// --- Internal setup ---

void SimulationController::setupWorkerConnections() {
    // Commands: controller -> worker (auto-queued across threads)
    connect(this, &SimulationController::requestDiscoverPeripherals,
            m_worker, &RenodeWorker::doDiscoverPeripherals);
    connect(this, &SimulationController::requestConnect,
            m_worker, &RenodeWorker::doConnect);
    connect(this, &SimulationController::requestDisconnect,
            m_worker, &RenodeWorker::doDisconnect);
    connect(this, &SimulationController::requestRunFor,
            m_worker, &RenodeWorker::doRunFor);
    connect(this, &SimulationController::requestPause,
            m_worker, &RenodeWorker::doPause);
    connect(this, &SimulationController::requestResume,
            m_worker, &RenodeWorker::doResume);
    connect(this, &SimulationController::requestReset,
            m_worker, &RenodeWorker::doReset);
    connect(this, &SimulationController::requestRefreshGpio,
            m_worker, &RenodeWorker::doRefreshGpio);
    connect(this, &SimulationController::requestSetGpioPin,
            m_worker, &RenodeWorker::doSetGpioPin);
    connect(this, &SimulationController::requestRefreshAdc,
            m_worker, &RenodeWorker::doRefreshAdc);
    connect(this, &SimulationController::requestSetAdcChannel,
            m_worker, &RenodeWorker::doSetAdcChannel);
    connect(this, &SimulationController::requestGetTime,
            m_worker, &RenodeWorker::doGetTime);

    // Results: worker -> controller (auto-queued across threads)
    connect(m_worker, &RenodeWorker::connected,
            this, &SimulationController::onConnected);
    connect(m_worker, &RenodeWorker::connectionFailed,
            this, &SimulationController::onConnectionFailed);
    connect(m_worker, &RenodeWorker::disconnected,
            this, &SimulationController::onDisconnected);
    connect(m_worker, &RenodeWorker::simulationTimeUpdated,
            this, &SimulationController::onSimulationTimeUpdated);
    connect(m_worker, &RenodeWorker::runForCompleted,
            this, &SimulationController::onRunForCompleted);
    connect(m_worker, &RenodeWorker::runForFailed,
            this, &SimulationController::onRunForFailed);
    connect(m_worker, &RenodeWorker::paused,
            this, &SimulationController::onPaused);
    connect(m_worker, &RenodeWorker::resumed,
            this, &SimulationController::onResumed);
    connect(m_worker, &RenodeWorker::resetDone,
            this, &SimulationController::onResetDone);
    connect(m_worker, &RenodeWorker::operationFailed,
            this, &SimulationController::onOperationFailed);
    connect(m_worker, &RenodeWorker::gpioStatesUpdated,
            this, &SimulationController::onGpioStatesUpdated);
    connect(m_worker, &RenodeWorker::gpioPinChanged,
            this, &SimulationController::onGpioPinChanged);
    connect(m_worker, &RenodeWorker::adcDataUpdated,
            this, &SimulationController::onAdcDataUpdated);
    connect(m_worker, &RenodeWorker::peripheralsDiscovered,
            this, &SimulationController::onPeripheralsDiscovered);
}
