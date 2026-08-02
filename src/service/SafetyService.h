#ifndef SAFETY_SERVICE_H
#define SAFETY_SERVICE_H

#include <Arduino.h>
#include "../config/Config.h"
#include "../hal/HeaterDriver.h"
#include "../hal/HumidifierDriver.h"
#include "../hal/FanDriver.h"

enum SafetyState {
    SAFETY_OK,
    SAFETY_WARNING,
    SAFETY_SHUTDOWN
};

class SafetyService {
public:
    SafetyService(HeaterDriver &heater, HumidifierDriver &humidifier, FanDriver &fan);

    void begin();
    SafetyState check(float temperature, bool sensorOK, uint8_t sensorFailCount);
    void emergencyShutdown(const String &reason);
    void reset();

    SafetyState getState() const;
    String getShutdownReason() const;
    bool isShutdown() const;

private:
    HeaterDriver     &_heater;
    HumidifierDriver &_humidifier;
    FanDriver        &_fan;

    SafetyState _state;
    String      _shutdownReason;
    bool        _shutdown;
};

#endif // SAFETY_SERVICE_H
