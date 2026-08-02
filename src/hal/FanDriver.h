#ifndef FAN_DRIVER_H
#define FAN_DRIVER_H

#include <Arduino.h>
#include "../config/Config.h"

class FanDriver {
public:
    FanDriver();

    void begin();
    void setPWM(uint8_t value);
    void stop();
    uint8_t getCurrentPWM() const;

private:
    uint8_t _currentPWM;
};

#endif // FAN_DRIVER_H
