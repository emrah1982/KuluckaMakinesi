#ifndef FAN_CONTROLLER_H
#define FAN_CONTROLLER_H

#include <Arduino.h>
#include "FanDriver.h"
#include "Config.h"

class FanController {
public:
    FanController(FanDriver &driver);

    void update(float temperature);
    uint8_t getComputedPWM() const;

private:
    FanDriver &_driver;
    uint8_t    _computedPWM;
};

#endif // FAN_CONTROLLER_H
