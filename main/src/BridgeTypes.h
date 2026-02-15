#pragma once

#include <QMetaType>

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
