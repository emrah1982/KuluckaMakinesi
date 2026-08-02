#include "StepperTurnerDriver.h"

// ============================================================
// Step adim sayisi:
//   stepsPerDeg = (STEPS_PER_REV * MICROSTEP * GEAR_RATIO) / 360
//   stepsForAngle(deg) = stepsPerDeg * deg
// Hareket suresi:
//   timePerStep = STEP_PERIOD_US (us)
//   toplam_ms = (stepsForAngle * STEP_PERIOD_US) / 1000
//
// I2C MUX altinda her step yaklasik 250-300 us I2C maliyeti
// getirir (set HIGH + delayUs + set LOW). Bu nedenle
// STEP_PERIOD_US >= 1000 us tutulmasi onerilir.
// ============================================================

uint32_t StepperTurnerDriver::stepsForAngle(uint8_t angleDeg) {
    if (angleDeg == 0) angleDeg = TURNER_DEFAULT_ANGLE_DEG;
    uint32_t fullRev = (uint32_t)STEPPER_STEPS_PER_REV
                     * (uint32_t)STEPPER_MICROSTEP
                     * (uint32_t)STEPPER_GEAR_RATIO;
    uint32_t steps = (fullRev * (uint32_t)angleDeg) / 360UL;
    if (steps == 0) steps = 1;
    return steps;
}

StepperTurnerDriver::StepperTurnerDriver()
    : _state(STEPPER_T_IDLE)
    , _lastTurnTime(0)
    , _stateStartTime(0)
    , _lastStepUs(0)
    , _stepsTarget(0)
    , _stepsDone(0)
    , _turnCount(0)
    , _wasEnabled(false)
    , _enabled(false)
    , _holdDurationMs(0)
{
}

void StepperTurnerDriver::enableDriver(bool en) {
    // A4988/DRV8825 EN aktif-LOW: LOW=enabled, HIGH=disabled
    RelayBoard::instance().setExtraPin(STEPPER_BIT_EN, !en);
    _enabled = en;
}

void StepperTurnerDriver::setDirection(bool forward) {
    RelayBoard::instance().setExtraPin(STEPPER_BIT_DIR, forward);
    delayMicroseconds(5);  // DIR setup time (A4988 datasheet ~200 ns; bizde I2C zaten yavasi)
}

void StepperTurnerDriver::begin() {
    enableDriver(false);
    setDirection(true);
    RelayBoard::instance().setExtraPin(STEPPER_BIT_STEP, false);

    _state = STEPPER_T_IDLE;
    _lastTurnTime = millis();
    _stateStartTime = millis();
    _lastStepUs = micros();
    _stepsTarget = 0;
    _stepsDone = 0;
    _turnCount = 0;
    _wasEnabled = false;

    Serial.println("[STEPPER] Baslatildi (PCF8574 P4=STEP/P5=DIR/P6=EN)");
}

void StepperTurnerDriver::doStepIfDue(unsigned long nowUs) {
    if (_stepsDone >= _stepsTarget) return;
    if ((unsigned long)(nowUs - _lastStepUs) < (unsigned long)STEPPER_STEP_PERIOD_US) return;
    RelayBoard::instance().pulseExtraPin(STEPPER_BIT_STEP, STEPPER_PULSE_WIDTH_US);
    _stepsDone++;
    _lastStepUs = nowUs;
}

void StepperTurnerDriver::update(bool turningEnabled,
                                 uint16_t intervalMin,
                                 uint8_t  durationSec,
                                 uint8_t  angleDeg) {
    (void)durationSec;  // step motor icin kullanilmiyor

    unsigned long now = millis();

    if (!turningEnabled || intervalMin == 0) {
        if (_state != STEPPER_T_DISABLED) {
            enableDriver(false);
            _state = STEPPER_T_DISABLED;
            _stateStartTime = now;
        }
        _wasEnabled = false;
        return;
    }

    unsigned long intervalMs = (unsigned long)intervalMin * 60000UL;

    if (_state == STEPPER_T_DISABLED) {
        _state = STEPPER_T_IDLE;
        _stateStartTime = now;
        _lastTurnTime = now;
    }

    if (!_wasEnabled) {
        _wasEnabled = true;
        _lastTurnTime = now;
    }

    switch (_state) {
        case STEPPER_T_IDLE:
            if (now - _lastTurnTime >= intervalMs) {
                _stepsTarget = stepsForAngle(angleDeg);
                _stepsDone = 0;
                enableDriver(true);
                setDirection(true);
                _state = STEPPER_T_STEPPING_FWD;
                _stateStartTime = now;
                _lastStepUs = micros();
                Serial.print("[STEPPER] FWD basliyor, hedef step="); Serial.println(_stepsTarget);
            }
            break;

        case STEPPER_T_STEPPING_FWD:
            doStepIfDue(micros());
            if (_stepsDone >= _stepsTarget) {
                _state = STEPPER_T_HOLD_FWD;
                _stateStartTime = now;
                if (STEPPER_DISABLE_AT_IDLE) enableDriver(false);
            }
            break;

        case STEPPER_T_HOLD_FWD:
            // Pause arasi yon donusu
            if (now - _stateStartTime >= TURNER_PAUSE_MS) {
                _stepsTarget = stepsForAngle(angleDeg);
                _stepsDone = 0;
                enableDriver(true);
                setDirection(false);
                _state = STEPPER_T_STEPPING_REV;
                _stateStartTime = now;
                _lastStepUs = micros();
            }
            break;

        case STEPPER_T_STEPPING_REV:
            doStepIfDue(micros());
            if (_stepsDone >= _stepsTarget) {
                _state = STEPPER_T_IDLE;
                _stateStartTime = now;
                _lastTurnTime = now;
                _turnCount++;
                if (STEPPER_DISABLE_AT_IDLE) enableDriver(false);
                Serial.print("[STEPPER] Cevirme tamam #"); Serial.println(_turnCount);
            }
            break;

        case STEPPER_T_DISABLED:
        default:
            break;
    }
}

void StepperTurnerDriver::stop() {
    enableDriver(false);
    _state = STEPPER_T_IDLE;
    _stateStartTime = millis();
    _stepsDone = _stepsTarget;
}

void StepperTurnerDriver::forceStop() {
    enableDriver(false);
    _state = STEPPER_T_DISABLED;
    _stateStartTime = millis();
    _stepsDone = _stepsTarget;
}

bool StepperTurnerDriver::isRunning() const {
    return (_state == STEPPER_T_STEPPING_FWD || _state == STEPPER_T_STEPPING_REV);
}

bool StepperTurnerDriver::isDisabled() const {
    return (_state == STEPPER_T_DISABLED);
}

StepperTurnerState StepperTurnerDriver::getState() const {
    return _state;
}

unsigned long StepperTurnerDriver::getLastTurnTime() const {
    return _lastTurnTime;
}

uint16_t StepperTurnerDriver::getTurnCount() const {
    return _turnCount;
}

void StepperTurnerDriver::setHoldDuration(unsigned long holdMs) {
    _holdDurationMs = holdMs;
}

unsigned long StepperTurnerDriver::getHoldDuration() const {
    return _holdDurationMs;
}
