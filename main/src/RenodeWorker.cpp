#include "RenodeWorker.h"

#include "renodeInterface.h"
#include "renodeMachine.h"

RenodeWorker::RenodeWorker(QObject *parent)
    : QObject(parent) {}

RenodeWorker::~RenodeWorker() {
    cleanupCallbacks();
}

void RenodeWorker::doConnect(QString renodePath, QString scriptPath,
                              QString host, int port, int monitorPort,
                              int timeoutMs, QString machineName) {
    try {
        renode::RenodeConfig config;
        config.renode_path = renodePath.toStdString();
        config.script_path = scriptPath.toStdString();
        config.host = host.toStdString();
        config.port = static_cast<uint16_t>(port);
        config.monitor_port = static_cast<uint16_t>(monitorPort);
        config.startup_timeout_ms = timeoutMs;

        m_client = renode::ExternalControlClient::launchAndConnect(config);

        if (!m_client->performHandshake()) {
            emit connectionFailed(QStringLiteral("Handshake failed"));
            m_client.reset();
            return;
        }

        if (monitorPort > 0) {
            m_client->connectMonitor(host.toStdString(),
                                     static_cast<uint16_t>(monitorPort));
        }

        renode::Error err;
        m_machine = m_client->getMachine(machineName.toStdString(), err);
        if (err) {
            emit connectionFailed(QString::fromStdString(err.message));
            m_client.reset();
            return;
        }

        emit connected(QString::fromStdString(m_machine->name()),
                        QString::fromStdString(m_machine->id()));
    } catch (const renode::RenodeException &e) {
        emit connectionFailed(QString::fromUtf8(e.what()));
        m_client.reset();
        m_machine.reset();
    }
}

void RenodeWorker::doDisconnect() {
    cleanupCallbacks();
    m_adcs.clear();
    m_gpios.clear();
    m_machine.reset();
    if (m_client) {
        m_client->disconnect();
        m_client.reset();
    }
    emit disconnected();
}

void RenodeWorker::doRunFor(quint64 duration, int timeUnitValue) {
    if (!m_machine) return;

    auto unit = static_cast<renode::TimeUnit>(timeUnitValue);
    renode::Error err = m_machine->runFor(duration, unit);
    if (err) {
        emit runForFailed(QString::fromStdString(err.message));
        return;
    }

    auto timeResult = m_machine->getTime(renode::TimeUnit::TU_MICROSECONDS);
    if (!timeResult.error) {
        emit simulationTimeUpdated(timeResult.value);
    }
    emit runForCompleted();
}

void RenodeWorker::doPause() {
    if (!m_machine) return;
    renode::Error err = m_machine->pause();
    if (err) {
        emit operationFailed(QStringLiteral("pause"),
                             QString::fromStdString(err.message));
        return;
    }
    emit paused();
}

void RenodeWorker::doResume() {
    if (!m_machine) return;
    renode::Error err = m_machine->resume();
    if (err) {
        emit operationFailed(QStringLiteral("resume"),
                             QString::fromStdString(err.message));
        return;
    }
    emit resumed();
}

void RenodeWorker::doReset() {
    if (!m_machine) return;
    renode::Error err = m_machine->reset();
    if (err) {
        emit operationFailed(QStringLiteral("reset"),
                             QString::fromStdString(err.message));
        return;
    }
    emit resetDone();
}

void RenodeWorker::doRefreshGpio(QString peripheralPath, int pinCount) {
    if (!m_machine) return;

    std::string path = peripheralPath.toStdString();
    renode::Error err;

    auto &gpio = m_gpios[path];
    if (!gpio) {
        gpio = m_machine->getGpio(path, err);
        if (err || !gpio) {
            m_gpios.erase(path);
            emit operationFailed(QStringLiteral("refreshGpio"),
                                 QString::fromStdString(err.message));
            return;
        }
        registerGpioCallbacks(path, gpio, pinCount);
    }

    QVector<GpioPinData> pins;
    pins.reserve(pinCount);
    for (int i = 0; i < pinCount; ++i) {
        renode::GpioState state;
        renode::Error pinErr = gpio->getState(i, state);
        if (!pinErr) {
            pins.append({i, static_cast<int>(state)});
        }
    }
    emit gpioStatesUpdated(peripheralPath, pins);
}

void RenodeWorker::doSetGpioPin(QString peripheralPath, int pin, int state) {
    if (!m_machine) return;

    std::string path = peripheralPath.toStdString();
    auto it = m_gpios.find(path);
    if (it == m_gpios.end() || !it->second) {
        emit operationFailed(QStringLiteral("setGpioPin"),
                             QStringLiteral("GPIO peripheral not initialized"));
        return;
    }

    auto gpioState = static_cast<renode::GpioState>(state);
    renode::Error err = it->second->setState(pin, gpioState);
    if (err) {
        emit operationFailed(QStringLiteral("setGpioPin"),
                             QString::fromStdString(err.message));
    }
}

void RenodeWorker::doRefreshAdc(QString peripheralPath) {
    if (!m_machine) return;

    std::string path = peripheralPath.toStdString();
    renode::Error err;

    auto &adc = m_adcs[path];
    if (!adc) {
        adc = m_machine->getAdc(path, err);
        if (err || !adc) {
            m_adcs.erase(path);
            emit operationFailed(QStringLiteral("refreshAdc"),
                                 QString::fromStdString(err.message));
            return;
        }
    }

    int channelCount = 0;
    err = adc->getChannelCount(channelCount);
    if (err) {
        emit operationFailed(QStringLiteral("refreshAdc"),
                             QString::fromStdString(err.message));
        return;
    }

    QVector<AdcChannelData> channels;
    channels.reserve(channelCount);
    for (int i = 0; i < channelCount; ++i) {
        renode::AdcValue val = 0.0;
        renode::Error chErr = adc->getChannelValue(i, val);
        if (!chErr) {
            channels.append({i, val});
        }
    }
    emit adcDataUpdated(peripheralPath, channelCount, channels);
}

void RenodeWorker::doSetAdcChannel(QString peripheralPath, int channel,
                                    double value) {
    if (!m_machine) return;

    std::string path = peripheralPath.toStdString();
    auto it = m_adcs.find(path);
    if (it == m_adcs.end() || !it->second) {
        emit operationFailed(QStringLiteral("setAdcChannel"),
                             QStringLiteral("ADC peripheral not initialized"));
        return;
    }

    renode::Error err = it->second->setChannelValue(channel, value);
    if (err) {
        emit operationFailed(QStringLiteral("setAdcChannel"),
                             QString::fromStdString(err.message));
    }
}

void RenodeWorker::doGetTime() {
    if (!m_machine) return;

    auto result = m_machine->getTime(renode::TimeUnit::TU_MICROSECONDS);
    if (!result.error) {
        emit simulationTimeUpdated(result.value);
    }
}

void RenodeWorker::registerGpioCallbacks(const std::string &path,
                                          std::shared_ptr<renode::Gpio> gpio,
                                          int pinCount) {
    QString qpath = QString::fromStdString(path);
    for (int pin = 0; pin < pinCount; ++pin) {
        int handle = -1;
        renode::Error err = gpio->registerStateChangeCallback(
            pin,
            [this, qpath](int p, renode::GpioState newState) {
                emit gpioPinChanged(qpath, p, static_cast<int>(newState));
            },
            handle);
        if (!err && handle >= 0) {
            m_gpioCallbackHandles.push_back({path, handle});
        }
    }
}

void RenodeWorker::cleanupCallbacks() {
    for (const auto &[path, handle] : m_gpioCallbackHandles) {
        auto it = m_gpios.find(path);
        if (it != m_gpios.end() && it->second) {
            it->second->unregisterStateChangeCallback(handle);
        }
    }
    m_gpioCallbackHandles.clear();
}
