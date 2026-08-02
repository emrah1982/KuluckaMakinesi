#include "HeaterDriver.h"
#include "RelayBoard.h"

// Time-proportional kontrol:
// PID cikisi (0-255) -> HEATER_WINDOW_MS icinde ON suresi
// Ornek: PWM=128, Window=10s -> 5s ON, 5s OFF

HeaterDriver::HeaterDriver()
    : _currentPWM(0)
    , _windowStart(0)
    , _relayState(false)
    , _cycleCount(0)
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

    // --- Role asınma korumasi ---
    // ON suresi cok kisaysa -> bu pencereyi tamamen atla (role tiklamasi onlenir)
    // OFF suresi cok kisaysa -> bu pencereyi tamamen AC tut
    if (onTime > 0 && onTime < RELAY_MIN_ON_MS) {
        onTime = 0;  // Cok kisa ON -> atla, role anahtarlama yapma
    }
    unsigned long offTime = HEATER_WINDOW_MS - onTime;
    if (offTime > 0 && offTime < RELAY_MIN_OFF_MS) {
        onTime = HEATER_WINDOW_MS;  // Cok kisa OFF -> tam AC tut
    }

    bool shouldBeOn = (onTime > 0) && (elapsed < onTime);

    if (shouldBeOn != _relayState) {
        _relayState = shouldBeOn;
        _cycleCount++;
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

uint32_t HeaterDriver::getCycleCount() const {
    return _cycleCount;
}
