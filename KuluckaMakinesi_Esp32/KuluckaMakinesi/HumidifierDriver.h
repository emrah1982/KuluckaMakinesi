#ifndef HUMIDIFIER_DRIVER_H
#define HUMIDIFIER_DRIVER_H

#include <Arduino.h>
#include "Config.h"

class HumidifierDriver {
public:
    HumidifierDriver();

    void begin();
    void turnOn();
    void turnOff();
    void update();
    bool isActive() const;
    bool isPendingOff() const;
    uint32_t getCycleCount() const;

private:
    bool _state;
    bool _pendingOff;
    unsigned long _offRequestTime;
    unsigned long _onTime;
    unsigned long _lastSwitchTime;  // Son anahtarlama zamani (cooldown)
    uint32_t _cycleCount;
};

#endif // HUMIDIFIER_DRIVER_H
