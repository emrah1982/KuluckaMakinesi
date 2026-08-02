#include "HumidifierDriver.h"
#include "RelayBoard.h"

HumidifierDriver::HumidifierDriver()
    : _state(false)
    , _pendingOff(false)
    , _offRequestTime(0)
    , _onTime(0)
{
}

void HumidifierDriver::begin() {
    RelayBoard::instance().setHumidifier(false);
    Serial.println("[HUMIDIFIER] Baslatildi (PCF8574 role)");
}

void HumidifierDriver::turnOn() {
    if (!_state) {
        _state = true;
        _pendingOff = false;
        _onTime = millis();
        RelayBoard::instance().setHumidifier(true);
    }
}

void HumidifierDriver::turnOff() {
    if (_state && !_pendingOff) {
        if (millis() - _onTime < HUMIDIFIER_MIN_ON) {
            return;
        }
        _pendingOff = true;
        _offRequestTime = millis();
        _state = false;
    }
}

void HumidifierDriver::update() {
    if (_pendingOff && (millis() - _offRequestTime >= HUMIDIFIER_OFF_DELAY)) {
        RelayBoard::instance().setHumidifier(false);
        _pendingOff = false;
    }
}

bool HumidifierDriver::isActive() const {
    return _state || _pendingOff;
}

bool HumidifierDriver::isPendingOff() const {
    return _pendingOff;
}
