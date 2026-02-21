#include "RenodeWorker.h"

#include "renodeInterface.h"
#include "renodeMachine.h"

#include <QDebug>
#include <QRegularExpression>

RenodeWorker::RenodeWorker(QObject *parent)
    : QObject(parent) {}

RenodeWorker::~RenodeWorker() {
    cleanupCallbacks();
}

void RenodeWorker::doConnect(QString renodePath, QString scriptPath,
                              QString host, int port, int monitorPort,
                              int timeoutMs, QString machineName) {
    qDebug("[Worker] doConnect: %s machine=%s", qPrintable(host), qPrintable(machineName));
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
            qWarning("[Worker] handshake failed");
            emit connectionFailed(QStringLiteral("Handshake failed"));
            m_client.reset();
            return;
        }
        qDebug("[Worker] handshake OK");

        if (monitorPort > 0) {
            m_client->connectMonitor(host.toStdString(),
                                     static_cast<uint16_t>(monitorPort));
            qDebug("[Worker] monitor connected on port %d", monitorPort);
        }

        renode::Error err;
        m_machine = m_client->getMachine(machineName.toStdString(), err);
        if (err) {
            qWarning("[Worker] getMachine failed: %s", err.message.c_str());
            emit connectionFailed(QString::fromStdString(err.message));
            m_client.reset();
            return;
        }
        qDebug("[Worker] getMachine OK: name=%s id=%s",
               m_machine->name().c_str(), m_machine->id().c_str());

        emit connected(QString::fromStdString(m_machine->name()),
                        QString::fromStdString(m_machine->id()));
    } catch (const renode::RenodeException &e) {
        qWarning("[Worker] doConnect exception: %s", e.what());
        emit connectionFailed(QString::fromUtf8(e.what()));
        m_client.reset();
        m_machine.reset();
    }
}

void RenodeWorker::doDisconnect() {
    qDebug("[Worker] doDisconnect");
    cleanupCallbacks();
    m_adcs.clear();
    m_gpios.clear();
    m_machine.reset();
    if (m_client) {
        m_client->disconnect();
        m_client.reset();
    }
    emit disconnected();
    qDebug("[Worker] disconnected");
}

void RenodeWorker::doRunFor(quint64 duration, int timeUnitValue) {
    qDebug("[Worker] doRunFor: %llu (unit=%d)", (unsigned long long)duration, timeUnitValue);
    if (!m_machine) return;

    auto unit = static_cast<renode::TimeUnit>(timeUnitValue);
    renode::Error err = m_machine->runFor(duration, unit);
    if (err) {
        qWarning("[Worker] runFor failed: %s", err.message.c_str());
        emit runForFailed(QString::fromStdString(err.message));
        return;
    }

    auto timeResult = m_machine->getTime(renode::TimeUnit::TU_MICROSECONDS);
    if (!timeResult.error) {
        qDebug("[Worker] simulationTime: %llu us", (unsigned long long)timeResult.value);
        emit simulationTimeUpdated(timeResult.value);
    }
    emit runForCompleted();
    qDebug("[Worker] runForCompleted");
}

void RenodeWorker::doPause() {
    qDebug("[Worker] doPause");
    if (!m_machine) return;
    renode::Error err = m_machine->pause();
    if (err) {
        qWarning("[Worker] pause failed: %s", err.message.c_str());
        emit operationFailed(QStringLiteral("pause"),
                             QString::fromStdString(err.message));
        return;
    }
    emit paused();
    qDebug("[Worker] paused");
}

void RenodeWorker::doResume() {
    qDebug("[Worker] doResume");
    if (!m_machine) return;
    renode::Error err = m_machine->resume();
    if (err) {
        qWarning("[Worker] resume failed: %s", err.message.c_str());
        emit operationFailed(QStringLiteral("resume"),
                             QString::fromStdString(err.message));
        return;
    }
    emit resumed();
    qDebug("[Worker] resumed");
}

void RenodeWorker::doReset() {
    qDebug("[Worker] doReset");
    if (!m_machine) return;
    renode::Error err = m_machine->reset();
    if (err) {
        qWarning("[Worker] reset failed: %s", err.message.c_str());
        emit operationFailed(QStringLiteral("reset"),
                             QString::fromStdString(err.message));
        return;
    }
    emit resetDone();
    qDebug("[Worker] resetDone");
}

void RenodeWorker::doRefreshGpio(QString peripheralPath, int pinCount) {
    qDebug("[Worker] doRefreshGpio: path=%s pins=%d", qPrintable(peripheralPath), pinCount);
    if (!m_machine) return;

    std::string path = peripheralPath.toStdString();
    renode::Error err;

    auto &gpio = m_gpios[path];
    if (!gpio) {
        gpio = m_machine->getGpio(path, err);
        if (err || !gpio) {
            qWarning("[Worker] getGpio failed: %s", err.message.c_str());
            m_gpios.erase(path);
            emit operationFailed(QStringLiteral("refreshGpio"),
                                 QString::fromStdString(err.message));
            return;
        }
        qDebug("[Worker] getGpio OK, registering callbacks");
        registerGpioCallbacks(path, gpio, pinCount);
    }

    QVector<GpioPinData> pins;
    pins.reserve(pinCount);
    for (int i = 0; i < pinCount; ++i) {
        renode::GpioState state;
        renode::Error pinErr = gpio->getState(i, state);
        if (!pinErr) {
            pins.append({i, static_cast<int>(state)});
        } else {
            qWarning("[Worker] getState failed for pin %d: %s", i, pinErr.message.c_str());
        }
    }
    qDebug("[Worker] gpioStatesUpdated: %lld pins read", (long long)pins.size());
    emit gpioStatesUpdated(peripheralPath, pins);
}

void RenodeWorker::doSetGpioPin(QString peripheralPath, int pin, int state) {
    qDebug("[Worker] doSetGpioPin: path=%s pin=%d state=%d", qPrintable(peripheralPath), pin, state);
    if (!m_machine) return;

    std::string path = peripheralPath.toStdString();
    auto it = m_gpios.find(path);
    if (it == m_gpios.end() || !it->second) {
        qWarning("[Worker] doSetGpioPin: GPIO not initialized");
        emit operationFailed(QStringLiteral("setGpioPin"),
                             QStringLiteral("GPIO peripheral not initialized"));
        return;
    }

    auto gpioState = static_cast<renode::GpioState>(state);
    renode::Error err = it->second->setState(pin, gpioState);
    if (err) {
        qWarning("[Worker] setState failed: %s", err.message.c_str());
        emit operationFailed(QStringLiteral("setGpioPin"),
                             QString::fromStdString(err.message));
    } else {
        qDebug("[Worker] setState OK");
    }
}

void RenodeWorker::doRefreshAdc(QString peripheralPath) {
    qDebug("[Worker] doRefreshAdc: path=%s", qPrintable(peripheralPath));
    if (!m_machine) return;

    std::string path = peripheralPath.toStdString();
    renode::Error err;

    auto &adc = m_adcs[path];
    if (!adc) {
        adc = m_machine->getAdc(path, err);
        if (err || !adc) {
            qWarning("[Worker] getAdc failed: %s", err.message.c_str());
            m_adcs.erase(path);
            emit operationFailed(QStringLiteral("refreshAdc"),
                                 QString::fromStdString(err.message));
            return;
        }
        qDebug("[Worker] getAdc OK");
    }

    int channelCount = 0;
    err = adc->getChannelCount(channelCount);
    if (err) {
        qWarning("[Worker] getChannelCount failed: %s", err.message.c_str());
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
        } else {
            qWarning("[Worker] getChannelValue failed for ch %d: %s", i, chErr.message.c_str());
        }
    }
    qDebug("[Worker] adcDataUpdated: %lld channels read", (long long)channels.size());
    emit adcDataUpdated(peripheralPath, channelCount, channels);
}

void RenodeWorker::doSetAdcChannel(QString peripheralPath, int channel,
                                    double value) {
    qDebug("[Worker] doSetAdcChannel: path=%s ch=%d val=%f", qPrintable(peripheralPath), channel, value);
    if (!m_machine) return;

    std::string path = peripheralPath.toStdString();
    auto it = m_adcs.find(path);
    if (it == m_adcs.end() || !it->second) {
        qWarning("[Worker] doSetAdcChannel: ADC not initialized");
        emit operationFailed(QStringLiteral("setAdcChannel"),
                             QStringLiteral("ADC peripheral not initialized"));
        return;
    }

    renode::Error err = it->second->setChannelValue(channel, value);
    if (err) {
        qWarning("[Worker] setChannelValue failed: %s", err.message.c_str());
        emit operationFailed(QStringLiteral("setAdcChannel"),
                             QString::fromStdString(err.message));
    } else {
        qDebug("[Worker] setChannelValue OK");
    }
}

void RenodeWorker::doGetTime() {
    qDebug("[Worker] doGetTime");
    if (!m_machine) return;

    auto result = m_machine->getTime(renode::TimeUnit::TU_MICROSECONDS);
    if (!result.error) {
        qDebug("[Worker] simulationTime: %llu us", (unsigned long long)result.value);
        emit simulationTimeUpdated(result.value);
    } else {
        qWarning("[Worker] getTime failed: %s", result.error.message.c_str());
    }
}

void RenodeWorker::doDiscoverPeripherals() {
    qDebug("[Worker] doDiscoverPeripherals");

    if (!m_client || !m_machine) {
        emit peripheralsDiscovered({});
        return;
    }

    auto *monitor = m_client->getMonitor();
    if (!monitor) {
        qWarning("[Discovery] monitor not available");
        emit peripheralsDiscovered({});
        return;
    }

    auto result = monitor->execute("peripherals");
    if (result.error) {
        qWarning("[Discovery] peripherals command failed: %s",
                 result.error.message.c_str());
        emit peripheralsDiscovered({});
        return;
    }

    QVector<GpioPortInfo> gpioPorts;
    QVector<AdcPortInfo>  adcPorts;

    // Strip ANSI escape sequences (e.g. \x1b[32m)
    static const QRegularExpression ansiRe(
        QStringLiteral(R"(\x1b\[[0-9;]*[A-Za-z])"));
    // Strip Unicode box-drawing chars (U+2500–U+257F) and ASCII tree chars
    static const QRegularExpression treeRe(
        QStringLiteral(R"([\x{2500}-\x{257F}|+\-])"));

    const QString output = QString::fromStdString(result.value);
    for (const QString &rawLine : output.split(QLatin1Char('\n'))) {
        QString line = rawLine;
        line.remove(ansiRe);
        line.remove(treeRe);
        line = line.simplified();
        if (line.isEmpty()) continue;

        // Format per line: "<shortName> <typeDescription>"
        const int sp = line.indexOf(QLatin1Char(' '));
        if (sp < 0) continue;

        const QString shortName = line.left(sp);
        const QString typeDesc  = line.mid(sp + 1);
        if (shortName.isEmpty() || typeDesc.isEmpty()) continue;

        const QString     path    = QStringLiteral("sysbus.") + shortName;
        const std::string stdPath = path.toStdString();

        if (typeDesc.contains(QLatin1String("GPIO"), Qt::CaseInsensitive)) {
            qDebug("[Discovery] GPIO candidate: %s", qPrintable(path));

            renode::Error err;
            auto gpio = m_machine->getGpio(stdPath, err);
            if (err || !gpio) {
                qWarning("[Discovery] getGpio failed for %s: %s",
                         qPrintable(path), err.message.c_str());
                continue;
            }
            m_gpios[stdPath] = gpio;

            // Register callback only once per path
            bool already = false;
            for (const auto &[hp, hnd] : m_gpioCallbackHandles)
                if (hp == stdPath) { already = true; break; }
            if (!already)
                registerGpioCallbacks(stdPath, gpio, 1);

            // Probe accessible pin count (up to 64)
            int pinCount = 0;
            for (int i = 0; i < 64; ++i) {
                renode::GpioState state;
                renode::Error pinErr = gpio->getState(i, state);
                if (pinErr) break;
                pinCount = i + 1;
            }
            if (pinCount == 0) {
                qWarning("[Discovery] GPIO %s: no pins accessible, skipping",
                         qPrintable(path));
                continue;
            }
            qDebug("[Discovery] GPIO %s: %d pins", qPrintable(path), pinCount);
            gpioPorts.append({path, shortName, pinCount});

        } else if (typeDesc.contains(QLatin1String("ADC"), Qt::CaseInsensitive)) {
            qDebug("[Discovery] ADC candidate: %s", qPrintable(path));

            renode::Error err;
            auto adc = m_machine->getAdc(stdPath, err);
            if (err || !adc) {
                qWarning("[Discovery] getAdc failed for %s: %s (skipping)",
                         qPrintable(path), err.message.c_str());
                continue;
            }
            m_adcs[stdPath] = adc;
            qDebug("[Discovery] ADC %s found", qPrintable(path));
            adcPorts.append({path, shortName});
        }
    }

    qDebug("[Discovery] done: %lld GPIO port(s), %lld ADC port(s)",
           (long long)gpioPorts.size(), (long long)adcPorts.size());
    emit peripheralsDiscovered(DiscoveredPeripherals{gpioPorts, adcPorts});
}

void RenodeWorker::registerGpioCallbacks(const std::string &path,
                                          std::shared_ptr<renode::Gpio> gpio,
                                          int pinCount) {
    // Register ONE callback for all pins. setState() fires every registered callback
    // regardless of pin, so per-pin registration causes N×N callback storms.
    // The `p` argument inside the lambda is the actual pin that changed.
    QString qpath = QString::fromStdString(path);
    int handle = -1;
    renode::Error err = gpio->registerStateChangeCallback(
        0,
        [this, qpath](int p, renode::GpioState newState) {
            qDebug("[GPIO callback] pin %d -> state %d", p, static_cast<int>(newState));
            emit gpioPinChanged(qpath, p, static_cast<int>(newState));
        },
        handle);
    if (!err && handle >= 0) {
        m_gpioCallbackHandles.push_back({path, handle});
    } else {
        qWarning("[GPIO] registerStateChangeCallback failed: %s", err.message.c_str());
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
