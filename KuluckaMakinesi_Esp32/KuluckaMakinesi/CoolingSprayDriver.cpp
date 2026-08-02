#include "CoolingSprayDriver.h"

// Slot penceresi genisligi (dakika) — slot baslangic saati geldikten sonra
// bu sure icinde aktive etmezsek o slotu kacirmis saydirir.
static const uint8_t SLOT_WINDOW_MIN = 5;

CoolingSprayDriver::CoolingSprayDriver()
    : _mode(CS_IDLE)
    , _stateStartMs(0)
    , _lastSlotDay(0)
    , _lastSlotDone(255)
    , _activeCoolingMin(0)
    , _activeSprayEnabled(0)
    , _activeSpraySec(0)
{
}

void CoolingSprayDriver::begin() {
    _mode = CS_IDLE;
    _stateStartMs = millis();
    _lastSlotDay = 0;
    _lastSlotDone = 255;
    _activeCoolingMin = 0;
    _activeSprayEnabled = 0;
    _activeSpraySec = 0;

    Serial.println("[COOLSPRAY] Baslatildi");
}

bool CoolingSprayDriver::inSlotWindow(uint8_t rtcHour, uint8_t rtcMin, uint8_t perDay, uint8_t &slotOut) const {
    // Slot saatleri:
    //   perDay=1 -> slot0: 12:00
    //   perDay=2 -> slot0: 08:00, slot1: 20:00
    // Pencere: [slotHour:00, slotHour:SLOT_WINDOW_MIN)
    uint8_t slotHours[2] = {0, 0};
    uint8_t slotCount = 0;

    if (perDay == 1) {
        slotHours[0] = 12;
        slotCount = 1;
    } else if (perDay >= 2) {
        slotHours[0] = 8;
        slotHours[1] = 20;
        slotCount = 2;
    } else {
        return false;
    }

    for (uint8_t i = 0; i < slotCount; i++) {
        if (rtcHour == slotHours[i] && rtcMin < SLOT_WINDOW_MIN) {
            slotOut = i;
            return true;
        }
    }
    return false;
}

void CoolingSprayDriver::update(uint8_t rtcHour, uint8_t rtcMin, uint8_t rtcDay,
                                 bool coolingEnabled, uint8_t coolingDurationMin, uint8_t coolingPerDay,
                                 bool sprayEnabled, uint8_t sprayDurationSec) {
    unsigned long now = millis();

    // Gun degisti mi? Slot sayaclarini sifirla.
    if (rtcDay != _lastSlotDay) {
        _lastSlotDay = rtcDay;
        _lastSlotDone = 255;
    }

    // Cooling tamamen kapaliysa IDLE'a don ve cik
    if (!coolingEnabled || coolingDurationMin == 0 || coolingPerDay == 0) {
        if (_mode != CS_IDLE) {
            _mode = CS_IDLE;
            _stateStartMs = now;
        }
        return;
    }

    switch (_mode) {
        case CS_IDLE: {
            // Slot penceresinde miyiz ve bu slot bugun tamamlanmadi mi?
            uint8_t slot = 255;
            if (inSlotWindow(rtcHour, rtcMin, coolingPerDay, slot)) {
                bool alreadyDone = (_lastSlotDone != 255) && (slot <= _lastSlotDone);
                if (!alreadyDone) {
                    // Seansi baslat — parametreleri cache'le (pencerede degisirse
                    // seans ortasi atlamayalim)
                    _activeCoolingMin = coolingDurationMin;
                    _activeSprayEnabled = sprayEnabled ? 1 : 0;
                    _activeSpraySec = sprayDurationSec;

                    _mode = CS_COOLING;
                    _stateStartMs = now;

                    Serial.print("[COOLSPRAY] COOLING basladi slot=");
                    Serial.print(slot);
                    Serial.print(" sure=");
                    Serial.print(_activeCoolingMin);
                    Serial.println("dk");
                }
            }
            break;
        }

        case CS_COOLING: {
            unsigned long elapsed = now - _stateStartMs;
            unsigned long target = (unsigned long)_activeCoolingMin * 60000UL;
            if (elapsed >= target) {
                if (_activeSprayEnabled && _activeSpraySec > 0) {
                    _mode = CS_SPRAYING;
                    _stateStartMs = now;
                    Serial.print("[COOLSPRAY] SPRAYING basladi sure=");
                    Serial.print(_activeSpraySec);
                    Serial.println("sn");
                } else {
                    // Sprey yok — dogrudan slot tamamla
                    uint8_t slot = 0;
                    uint8_t dummy = 255;
                    (void)inSlotWindow(rtcHour, rtcMin, coolingPerDay, dummy);
                    // Tamamlanan slot'u isaretle: pencere kapandi, ama biz seansi
                    // baslatan slot bilgisi yok; basit yaklasim: _lastSlotDone++
                    slot = (_lastSlotDone == 255) ? 0 : (_lastSlotDone + 1);
                    _lastSlotDone = slot;
                    _mode = CS_IDLE;
                    _stateStartMs = now;
                    Serial.println("[COOLSPRAY] Seans tamamlandi (sprey yok)");
                }
            }
            break;
        }

        case CS_SPRAYING: {
            unsigned long elapsed = now - _stateStartMs;
            unsigned long target = (unsigned long)_activeSpraySec * 1000UL;
            if (elapsed >= target) {
                uint8_t slot = (_lastSlotDone == 255) ? 0 : (_lastSlotDone + 1);
                _lastSlotDone = slot;
                _mode = CS_IDLE;
                _stateStartMs = now;
                Serial.println("[COOLSPRAY] Seans tamamlandi");
            }
            break;
        }
    }
}

CoolingSprayMode CoolingSprayDriver::getMode() const {
    return _mode;
}

bool CoolingSprayDriver::shouldOverrideHeater() const {
    return _mode == CS_COOLING;
}

bool CoolingSprayDriver::shouldOverrideFan() const {
    return _mode == CS_COOLING;
}

bool CoolingSprayDriver::shouldOverrideHumidifier() const {
    return _mode == CS_SPRAYING;
}

uint16_t CoolingSprayDriver::getSecondsInState() const {
    return (uint16_t)((millis() - _stateStartMs) / 1000UL);
}

uint16_t CoolingSprayDriver::getCoolingRemainSec() const {
    if (_mode != CS_COOLING) return 0;
    unsigned long elapsed = millis() - _stateStartMs;
    unsigned long target  = (unsigned long)_activeCoolingMin * 60000UL;
    if (elapsed >= target) return 0;
    return (uint16_t)((target - elapsed) / 1000UL);
}

uint8_t CoolingSprayDriver::getLastSlotDone() const {
    return _lastSlotDone;
}

void CoolingSprayDriver::cancel() {
    if (_mode != CS_IDLE) {
        Serial.println("[COOLSPRAY] Iptal edildi");
    }
    _mode = CS_IDLE;
    _stateStartMs = millis();
}
