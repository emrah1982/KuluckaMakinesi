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
    pinMode(TURNER_IN1_PIN, OUTPUT);
    pinMode(TURNER_IN2_PIN, OUTPUT);

    ledcAttach(TURNER_ENA_PIN, PWM_FREQ_TURNER, PWM_RESOLUTION);
    ledcWrite(TURNER_ENA_PIN, 0);

    stopMotor();

    _state = TURNER_IDLE;
    _lastTurnTime = millis();
    _stateStartTime = millis();
    _turnCount = 0;
    _wasEnabled = false;

    Serial.println("[TURNER] Baslatildi (L298N)");
}

void TurnerDriver::setMotor(bool forward) {
    if (forward) {
        digitalWrite(TURNER_IN1_PIN, HIGH);
        digitalWrite(TURNER_IN2_PIN, LOW);
    } else {
        digitalWrite(TURNER_IN1_PIN, LOW);
        digitalWrite(TURNER_IN2_PIN, HIGH);
    }

    ledcWrite(TURNER_ENA_PIN, TURNER_PWM_SPEED);
}

void TurnerDriver::stopMotor() {
    ledcWrite(TURNER_ENA_PIN, 0);
    digitalWrite(TURNER_IN1_PIN, LOW);
    digitalWrite(TURNER_IN2_PIN, LOW);
}

void TurnerDriver::update(bool turningEnabled) {
    unsigned long now = millis();

    if (!turningEnabled) {
        if (_state != TURNER_DISABLED) {
            stopMotor();
            _state = TURNER_DISABLED;
            _stateStartTime = now;
        }
        _wasEnabled = false;
        return;
    }

    if (_state == TURNER_DISABLED) {
        _state = TURNER_IDLE;
        _stateStartTime = now;
        _lastTurnTime = now;
    }

    // turning enabled
    if (!_wasEnabled) {
        _wasEnabled = true;
        _lastTurnTime = now;
    }

    switch (_state) {
        case TURNER_IDLE:
            if (now - _lastTurnTime >= TURNER_INTERVAL_MS) {
                _state = TURNER_FORWARD;
                _stateStartTime = now;
                setMotor(true);
            }
            break;

        case TURNER_FORWARD:
            if (now - _stateStartTime >= TURNER_DURATION_MS) {
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
            if (now - _stateStartTime >= TURNER_DURATION_MS) {
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
