#include "TurnerDriver.h"

TurnerDriver::TurnerDriver()
    : _state(TURNER_IDLE)
    , _lastTurnTime(0)
    , _stateStartTime(0)
    , _turnCount(0)
    , _wasEnabled(false)
{
}

void TurnerDriver::begin() {
    stopMotor();

    _state = TURNER_IDLE;
    _lastTurnTime = millis();
    _stateStartTime = millis();
    _turnCount = 0;
    _wasEnabled = false;

    Serial.println("[TURNER] Baslatildi (PCF8574 Role)");
}

void TurnerDriver::setMotor(bool forward) {
    // Once yonu ayarla, sonra gucu ac (guvenli siralama)
    RelayBoard::instance().setTurnerDirection(!forward);  // false=Ileri, true=Geri
    RelayBoard::instance().setTurnerPower(true);
}

void TurnerDriver::stopMotor() {
    // Once gucu kes, sonra yonu sifirla
    RelayBoard::instance().setTurnerPower(false);
    RelayBoard::instance().setTurnerDirection(false);
}

void TurnerDriver::update(bool turningEnabled, uint16_t intervalMin, uint8_t durationSec, uint8_t angleDeg) {
    unsigned long now = millis();

    // intervalMin=0 -> profil turning istemiyor (EMPTY/ipek bocegi/ari vb.)
    if (!turningEnabled || intervalMin == 0) {
        if (_state != TURNER_DISABLED) {
            stopMotor();
            _state = TURNER_DISABLED;
            _stateStartTime = now;
        }
        _wasEnabled = false;
        return;
    }

    // Profile'dan gelen degerler.
    // Surecin oncelik sirasi:
    //   1) durationSec > 0  -> dogrudan kullan
    //   2) angleDeg   > 0   -> aci/TURNER_DEG_PER_SEC ile hesapla (yazilim kalibrasyonu)
    //   3) ikisi de 0       -> Config.h TURNER_DURATION_MS fallback
    unsigned long intervalMs = (unsigned long)intervalMin * 60000UL;
    unsigned long durationMs;
    if (durationSec > 0) {
        durationMs = (unsigned long)durationSec * 1000UL;
    } else if (angleDeg > 0 && TURNER_DEG_PER_SEC > 0.01f) {
        float secs = (float)angleDeg / TURNER_DEG_PER_SEC;
        durationMs = (unsigned long)(secs * 1000.0f);
        if (durationMs < 200UL) durationMs = 200UL;        // minik koruma
    } else {
        durationMs = (unsigned long)TURNER_DURATION_MS;
    }

    if (_state == TURNER_DISABLED) {
        _state = TURNER_IDLE;
        _stateStartTime = now;
        _lastTurnTime = now;
    }

    if (!_wasEnabled) {
        _wasEnabled = true;
        _lastTurnTime = now;
    }

    switch (_state) {
        case TURNER_IDLE:
            if (now - _lastTurnTime >= intervalMs) {
                _state = TURNER_FORWARD;
                _stateStartTime = now;
                setMotor(true);
            }
            break;

        case TURNER_FORWARD:
            if (now - _stateStartTime >= durationMs) {
                stopMotor();
                _state = TURNER_PAUSE;
                _stateStartTime = now;
            }
            break;

        case TURNER_PAUSE:
            if (now - _stateStartTime >= TURNER_PAUSE_MS) {
                _state = TURNER_REVERSE;
                _stateStartTime = now;
                setMotor(false);
            }
            break;

        case TURNER_REVERSE:
            if (now - _stateStartTime >= durationMs) {
                stopMotor();
                _state = TURNER_IDLE;
                _stateStartTime = now;
                _lastTurnTime = now;
                _turnCount++;
            }
            break;

        case TURNER_DISABLED:
        default:
            break;
    }
}

void TurnerDriver::stop() {
    stopMotor();
    _state = TURNER_IDLE;
    _stateStartTime = millis();
}

void TurnerDriver::forceStop() {
    stopMotor();
    _state = TURNER_DISABLED;
    _stateStartTime = millis();
}

bool TurnerDriver::isRunning() const {
    return (_state == TURNER_FORWARD || _state == TURNER_REVERSE);
}

bool TurnerDriver::isDisabled() const {
    return (_state == TURNER_DISABLED);
}

TurnerState TurnerDriver::getState() const {
    return _state;
}

unsigned long TurnerDriver::getLastTurnTime() const {
    return _lastTurnTime;
}

uint16_t TurnerDriver::getTurnCount() const {
    return _turnCount;
}
