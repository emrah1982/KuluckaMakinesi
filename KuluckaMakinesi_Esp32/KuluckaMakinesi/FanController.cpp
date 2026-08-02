#include "FanController.h"

FanController::FanController(FanDriver &driver)
    : _driver(driver)
    , _computedPWM(FAN_MIN_PWM)
{
}

void FanController::update(float temperature) {
    // Sıcaklığa bağlı fan hızı haritalaması
    int pwm = map((long)(temperature * 10),
                   (long)(FAN_TEMP_LOW * 10),
                   (long)(FAN_TEMP_HIGH * 10),
                   FAN_MIN_PWM,
                   FAN_MAX_PWM);

    pwm = constrain(pwm, FAN_MIN_PWM, FAN_MAX_PWM);
    _computedPWM = (uint8_t)pwm;
    _driver.setPWM(_computedPWM);
}

uint8_t FanController::getComputedPWM() const {
    return _computedPWM;
}
