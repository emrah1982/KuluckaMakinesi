#include "HeaterDriver.h"
#include "RelayBoard.h"

// Time-proportional kontrol:
// PID cikisi (0-255) -> HEATER_WINDOW_MS icinde ON suresi
// Ornek: PWM=128, Window=10s -> 5s ON, 5s OFF

HeaterDriver::HeaterDriver()
    : _currentPWM(0)
    , _windowStart(0)
    , _relayState(false)
{
}

void HeaterDriver::begin() {
    _windowStart = millis();
    _relayState = false;
    RelayBoard::instance().setHeater(false);
    Serial.println("[HEATER] Baslatildi (PCF8574 role + time-proportional)");
}

void HeaterDriver::setPWM(uint8_t value) {
    _currentPWM = value;
}

void HeaterDriver::update() {
    unsigned long now = millis();
    unsigned long elapsed = now - _windowStart;

    // Pencere suresi doldu - yeni pencere baslat
    if (elapsed >= HEATER_WINDOW_MS) {
        _windowStart = now;
        elapsed = 0;
    }

    // ON suresi hesapla: (pwm / 255) * pencere_suresi
    unsigned long onTime = ((unsigned long)_currentPWM * HEATER_WINDOW_MS) / 255UL;

    bool shouldBeOn = (_currentPWM > 0) && (elapsed < onTime);

    if (shouldBeOn != _relayState) {
        _relayState = shouldBeOn;
        RelayBoard::instance().setHeater(_relayState);
    }
}

void HeaterDriver::stop() {
    _currentPWM = 0;
    _relayState = false;
    RelayBoard::instance().setHeater(false);
}

uint8_t HeaterDriver::getCurrentPWM() const {
    return _currentPWM;
}

bool HeaterDriver::isActive() const {
    return _relayState;
}
