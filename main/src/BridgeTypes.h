#pragma once

#include <QMetaType>
#include <QString>
#include <QVector>

struct GpioPinData {
    int pin = 0;
    int state = 0; // 0=Low, 1=High, 2=HighZ
};
Q_DECLARE_METATYPE(GpioPinData)

struct AdcChannelData {
    int channel = 0;
    double value = 0.0;
};
Q_DECLARE_METATYPE(AdcChannelData)

struct GpioPortInfo {
    QString path;     // e.g. "sysbus.gpioPortA"
    QString name;     // e.g. "gpioPortA"
    int pinCount = 0;
};

struct AdcPortInfo {
    QString path;     // e.g. "sysbus.adc1"
    QString name;     // e.g. "adc1"
};

struct DiscoveredPeripherals {
    QVector<GpioPortInfo> gpioPorts;
    QVector<AdcPortInfo>  adcPorts;
};
Q_DECLARE_METATYPE(DiscoveredPeripherals)
