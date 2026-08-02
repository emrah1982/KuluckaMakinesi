#ifndef STEPPER_TURNER_DRIVER_H
#define STEPPER_TURNER_DRIVER_H

#include <Arduino.h>
#include "Config.h"
#include "RelayBoard.h"

// ============================================================
// Step Motor (A4988 / DRV8825) ile yumurta cevirme surucusu
// PCF8574 P4=STEP, P5=DIR, P6=EN ile dogrudan kontrol.
// Grove 8 kanal I2C MUX altinda olduundan her STEP tetiklemesi
// yaklasik 250-300 us I2C transaction maliyetlidir; pratikte
// makul ust hiz ~250 step/s civari (egg turning icin yeterli).
//
// API TurnerDriver ile esleser uyumludur (intervalMin, angleDeg).
// durationSec parametresi step motor icin anlamsizdir (verilirse
// loglanir, kullanilmaz). Donus suresi acidan + STEPPER_STEP_PERIOD_US
// degerinden hesaplanir.
// ============================================================

enum StepperTurnerState {
    STEPPER_T_IDLE,        // Motor disabled, sira sonraki cevirmede
    STEPPER_T_STEPPING_FWD,// Ileri yon adimliyor
    STEPPER_T_HOLD_FWD,    // Ileri konumda bekleme
    STEPPER_T_STEPPING_REV,// Geri yon adimliyor
    STEPPER_T_DISABLED     // Lockdown / kapali
};

class StepperTurnerDriver {
public:
    StepperTurnerDriver();

    void begin();

    // TurnerDriver ile ayni signature.
    // durationSec parametresi step motor icin yok sayilir.
    // angleDeg=0 ise TURNER_DEFAULT_ANGLE_DEG (Config.h) kullanilir.
    void update(bool turningEnabled,
                uint16_t intervalMin = 0,
                uint8_t  durationSec = 0,
                uint8_t  angleDeg    = 0);

    void stop();
    void forceStop();

    bool isRunning() const;
    bool isDisabled() const;
    StepperTurnerState getState() const;
    unsigned long getLastTurnTime() const;
    uint16_t getTurnCount() const;

    void setHoldDuration(unsigned long holdMs);
    unsigned long getHoldDuration() const;

private:
    StepperTurnerState _state;
    unsigned long _lastTurnTime;
    unsigned long _stateStartTime;
    unsigned long _lastStepUs;
    uint32_t _stepsTarget;
    uint32_t _stepsDone;
    uint16_t _turnCount;
    bool _wasEnabled;
    bool _enabled;

    unsigned long _holdDurationMs;

    // Donanim islemleri (PCF8574 ekstra pin uzerinden)
    void enableDriver(bool en);
    void setDirection(bool forward);
    void doStepIfDue(unsigned long nowUs);

    static uint32_t stepsForAngle(uint8_t angleDeg);
};

#endif // STEPPER_TURNER_DRIVER_H
