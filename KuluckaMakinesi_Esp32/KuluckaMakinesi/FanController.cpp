#include "FanController.h"

FanController::FanController(FanDriver &driver)
    : _driver(driver)
    , _computedPWM(FAN_MIN_PWM)
#if FAN_DUTY_MODE
    , _dutyPeriodStartMs(0)
#endif
{
}

void FanController::update(float temperature, float targetTemp) {
    // Sıcaklığa bağlı fan hızı haritalaması
    int pwm = map((long)(temperature * 10),
                   (long)(FAN_TEMP_LOW * 10),
                   (long)(FAN_TEMP_HIGH * 10),
                   FAN_MIN_PWM,
                   FAN_MAX_PWM);

    pwm = constrain(pwm, FAN_MIN_PWM, FAN_MAX_PWM);

#if FAN_DUTY_MODE
    // ---- KESINTILI CALISMA ----
    // Fan yalnizca sogutma degil HOMOJENLIK aracidir. Uzun sure durursa
    // kabin katmanlasir; sensor kendi bolgesini olcer ve tepsinin geri
    // kalani hedeften sapabilir. Bu yuzden kesinti "tamamen kapali" degil,
    // "her periyotta garanti sirkulasyon" seklinde tasarlandi.
    unsigned long nowMs = millis();
    if (_dutyPeriodStartMs == 0) _dutyPeriodStartMs = nowMs;

    unsigned long elapsed = nowMs - _dutyPeriodStartMs;
    const unsigned long periodMs = (unsigned long)FAN_DUTY_PERIOD_SEC * 1000UL;
    const unsigned long onMs     = (unsigned long)FAN_DUTY_ON_SEC * 1000UL;
    if (elapsed >= periodMs) {
        _dutyPeriodStartMs = nowMs;
        elapsed = 0;
    }

    // Sicaklik hedefi asmissa sogutma onceliklidir: kesinti uygulanmaz.
    bool needCooling = (temperature >= targetTemp + FAN_DUTY_FORCE_DELTA);
    bool inOnWindow  = (elapsed < onMs);

    if (!needCooling && !inOnWindow) {
        pwm = 0;   // sirkulasyon penceresi disinda dinlen
    }
#else
    (void)targetTemp;   // surekli modda hedef kullanilmaz
#endif

    _computedPWM = (uint8_t)pwm;
    _driver.setPWM(_computedPWM);
}

uint8_t FanController::getComputedPWM() const {
    return _computedPWM;
}
