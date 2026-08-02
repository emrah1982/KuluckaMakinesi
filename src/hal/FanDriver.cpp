#include "FanDriver.h"

// Fan kontrolu: Ayrı PWM pini uzerinden (FAN_PIN)

FanDriver::FanDriver()
    : _currentPWM(0)
{
}

void FanDriver::begin() {
    ledcAttach(FAN_PIN, PWM_FREQ_FAN, PWM_RESOLUTION);
    ledcWrite(FAN_PIN, FAN_MIN_PWM);
    _currentPWM = FAN_MIN_PWM;

    Serial.println("[FAN] Baslatildi");
}

void FanDriver::setPWM(uint8_t value) {
    if (value < FAN_MIN_PWM) value = FAN_MIN_PWM;
    if (value > FAN_MAX_PWM) value = FAN_MAX_PWM;
    _currentPWM = value;
    ledcWrite(FAN_PIN, _currentPWM);
}

void FanDriver::stop() {
    _currentPWM = 0;
    ledcWrite(FAN_PIN, 0);
}

uint8_t FanDriver::getCurrentPWM() const {
    return _currentPWM;
}
