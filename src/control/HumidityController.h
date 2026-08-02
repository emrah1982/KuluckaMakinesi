#ifndef HUMIDITY_CONTROLLER_H
#define HUMIDITY_CONTROLLER_H

#include <Arduino.h>
#include "../hal/HumidifierDriver.h"

class HumidityController {
public:
    HumidityController(HumidifierDriver &driver);

    void setThresholds(float low, float high);
    void update(float currentHumidity);

    float getLowThreshold() const;
    float getHighThreshold() const;
    bool  isHumidifying() const;

private:
    HumidifierDriver &_driver;
    float _lowThreshold;
    float _highThreshold;
    bool  _humidifying;
};

#endif // HUMIDITY_CONTROLLER_H
