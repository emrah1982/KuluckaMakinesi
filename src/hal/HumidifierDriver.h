#ifndef HUMIDIFIER_DRIVER_H
#define HUMIDIFIER_DRIVER_H

#include <Arduino.h>
#include "../config/Config.h"

class HumidifierDriver {
public:
    HumidifierDriver();

    void begin();
    void turnOn();
    void turnOff();
    void update();
    bool isActive() const;
    bool isPendingOff() const;

private:
    bool _state;
    bool _pendingOff;
    unsigned long _offRequestTime;
    unsigned long _onTime;
};

#endif // HUMIDIFIER_DRIVER_H
