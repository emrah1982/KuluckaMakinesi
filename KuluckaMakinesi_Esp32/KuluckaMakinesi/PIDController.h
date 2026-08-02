#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

#include <Arduino.h>
#include "Config.h"

// ============================================================
// PIDController — Gercek Dunya PID Kontrol Algoritmasi
// ------------------------------------------------------------
// Endustri standardi PID iyilestirmeleri:
//   * Time-based integral/derivative (gercek dt kullanir, sample
//     sayisi tabanli degil)
//   * Derivative on measurement (D-kick onleme: setpoint degisince
//     turev term spike yapmaz)
//   * Conditional anti-windup (cikis sature ise integral biriktirmeyi
//     durdurur, klasik clamp'e ek)
//   * Setpoint ramping (ani setpoint degisikliklerinde yumusak gecis)
//   * Output slew rate limit (PWM ani sicrama onleme — role omru)
//   * Derivative LPF (sensor noise filtresi)
//   * Bumpless transfer (manuel -> otomatik gecislerde output sicramaz)
//
// Kullanim:
//   pid.setSetpoint(37.5);
//   pid.startAutoTune();   // veya pid.setParameters(Kp,Ki,Kd)
//   loop() { uint8_t pwm = pid.compute(currentTemp); }
// ============================================================

enum PIDState {
    PID_STATE_IDLE,
    PID_STATE_AUTOTUNING,
    PID_STATE_RUNNING,
    PID_STATE_MANUAL    // manuel PWM modu (bumpless transfer destegi)
};

// Auto-tune sonrasi farkli kontrolcu profilleri
enum PIDTuningRule {
    PID_TUNING_ZN_CLASSIC,    // Ziegler-Nichols (hizli ama overshoot)
    PID_TUNING_ZN_PESSEN,     // Pessen Integral (daha az overshoot)
    PID_TUNING_TYREUS_LUYBEN, // Daha conservative (kararliilk)
    PID_TUNING_NO_OVERSHOOT   // Hicbir overshoot olmasin (yavaş ama güvenli)
};

class PIDController {
public:
    PIDController();

    void begin();

    // ------------------ Setpoint ------------------
    // Hedef setpoint ayarlar (ramping varsa kademeli yaklasim)
    void setSetpoint(double setpoint);
    double getSetpoint() const;            // Hedef setpoint
    double getActiveSetpoint() const;      // Anlik (ramped) setpoint
    void   setSetpointRampRate(double degPerSec);  // 0 = ramping kapali

    // ------------------ Hesap ------------------
    // Mevcut sicaklik girer, PWM (0-255) doner.
    // Ic zaman olcusu otomatik, dt = millis() - _lastComputeMs
    uint8_t compute(double input);

    // ------------------ Parametre ------------------
    double getKp() const;
    double getKi() const;
    double getKd() const;
    void setParameters(double kp, double ki, double kd);
    void setTuningRule(PIDTuningRule rule);  // Auto-tune sonrasi uygulanacak kural

    // ------------------ Auto-Tune ------------------
    void startAutoTune();
    bool isAutoTuning() const;
    void autoTuneStep(double input);

    // ------------------ Manuel mod ------------------
    // Manuel PWM uygula (bumpless transfer icin hatirlanir)
    void setManualOutput(uint8_t pwm);
    void setManualMode(bool enabled);
    bool isManualMode() const { return _state == PID_STATE_MANUAL; }

    // ------------------ Output limitleri ------------------
    void setOutputSlewRate(double pwmPerSec);  // 0 = slew sinirsiz

    // ------------------ State / Telemetri ------------------
    PIDState getState() const;
    double getLastOutput() const;
    double getLastError() const;
    double getIntegralTerm() const { return _integral; }
    double getDerivativeTerm() const { return _lastDerivative; }
    bool   isSaturated() const { return _saturated; }

    void reset();

private:
    // Setpoint
    double _setpointTarget;       // Kullanicinin istedigi hedef
    double _setpointActive;       // Anlik (ramped) setpoint
    double _setpointRampDegPerSec;

    // PID parametreleri
    double _kp, _ki, _kd;
    PIDTuningRule _tuningRule;

    // Internal state
    double _integral;
    double _lastError;
    double _lastInput;            // Derivative on measurement icin
    double _lastDerivative;       // LPF icin onceki D term
    double _lastOutput;
    bool   _saturated;

    // Time tracking
    unsigned long _lastComputeMs;

    // Output slew rate
    double _outputSlewPerSec;
    double _slewedOutput;         // Anlik (slew limited) output

    PIDState _state;

    // Auto-tune
    bool   _atHeating;
    double _atMaxTemp;
    double _atMinTemp;
    int    _atCycleCount;
    unsigned long _atLastSwitchTime;
    unsigned long _atStartTime;
    unsigned long _atPeriodSumMs; // Donus periyotlarinin toplami (ortalamak icin)
    unsigned long _atLastPeakMs;

    // Manuel mod / bumpless transfer
    uint8_t _manualPWM;

    void applyTuningRule(double Ku, double Tu);
};

#endif // PID_CONTROLLER_H
