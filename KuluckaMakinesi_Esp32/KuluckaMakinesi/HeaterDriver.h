#ifndef HEATER_DRIVER_H
#define HEATER_DRIVER_H

#include <Arduino.h>
#include "Config.h"

// Isitici: PCF8574 role uzerinden time-proportional kontrol
// PID cikisi (0-255) -> pencere suresi icinde ON/OFF orani
class HeaterDriver {
public:
    HeaterDriver();

    void begin();
    void setPWM(uint8_t value);  // PID cikisi (0-255) - API degismedi
    void stop();
    void update();               // Her loop'ta cagrilmali - role zamanlamasini yonetir
    uint8_t getCurrentPWM() const;
    bool isActive() const;
    uint32_t getCycleCount() const;

private:
    uint8_t _currentPWM;
    unsigned long _windowStart;
    bool _relayState;
    uint32_t _cycleCount;        // Toplam anahtarlama sayisi (bakim takibi)
};

#endif // HEATER_DRIVER_H
