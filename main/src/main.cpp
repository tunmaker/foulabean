#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QVector>

#include "BridgeTypes.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);

    // Register metatypes for cross-thread signal/slot
    qRegisterMetaType<QVector<GpioPinData>>("QVector<GpioPinData>");
    qRegisterMetaType<QVector<AdcChannelData>>("QVector<AdcChannelData>");

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule("digitwin", "Main");

    return app.exec();
}
