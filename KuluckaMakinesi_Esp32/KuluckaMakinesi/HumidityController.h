#ifndef HUMIDITY_CONTROLLER_H
#define HUMIDITY_CONTROLLER_H

#include <Arduino.h>
#include "HumidifierDriver.h"

class HumidityController {
public:
    HumidityController(HumidifierDriver &driver);

    void setThresholds(float low, float high);
    void update(float currentHumidity);

    float getLowThreshold() const;
    float getHighThreshold() const;
    bool  isHumidifying() const;

    // Nemlendirici KESINTISIZ HUM_EFFECT_TIMEOUT_MS boyunca acik kalip nem
    // en az HUM_EFFECT_MIN_RISE kadar yukselmediyse true doner.
    // Tipik sebep: su deposunun bosalmasi. Bang-bang kontrol cikisa komut
    // verip ise yaradigini varsayiyordu; bu, o varsayimin dogrulanmasidir.
    bool isIneffective() const;

private:
    HumidifierDriver &_driver;
    float _lowThreshold;
    float _highThreshold;
    bool  _humidifying;

    // Etkinlik denetimi durumu
    unsigned long _humOnStartMs;   // Kesintisiz nemlendirmenin baslangici
    float         _humAtStart;     // O andaki nem
    bool          _ineffective;    // Ariza tespit edildi
};

#endif // HUMIDITY_CONTROLLER_H
