#ifndef FAN_CONTROLLER_H
#define FAN_CONTROLLER_H

#include <Arduino.h>
#include "FanDriver.h"
#include "Config.h"

class FanController {
public:
    FanController(FanDriver &driver);

    // targetTemp: aktif fazin hedef sicakligi. Kesintili modda "hedefi asti mi"
    // karari profile gore verilmeli; sabit bir esik farkli hayvanlarda yanlis
    // olurdu (tavuk 37.8, devekusu 36.4 gibi).
    void update(float temperature, float targetTemp);
    uint8_t getComputedPWM() const;

private:
    FanDriver &_driver;
    uint8_t    _computedPWM;

#if FAN_DUTY_MODE
    // Kesintili mod durumu
    unsigned long _dutyPeriodStartMs;  // Icinde bulunulan periyodun baslangici
#endif
};

#endif // FAN_CONTROLLER_H
