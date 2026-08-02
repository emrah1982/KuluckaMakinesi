#include "PIDController.h"

// ============================================================
// PIDController — Gercek Dunya PID Algoritmasi
// ------------------------------------------------------------
// Bkz. PIDController.h dokumani.
// ============================================================

PIDController::PIDController()
    : _setpointTarget(37.5)
    , _setpointActive(37.5)
    , _setpointRampDegPerSec(PID_SETPOINT_RAMP_DEG_PER_SEC)
    , _kp(PID_DEFAULT_KP)
    , _ki(PID_DEFAULT_KI)
    , _kd(PID_DEFAULT_KD)
    , _tuningRule(PID_TUNING_ZN_PESSEN)   // varsayilan: az overshoot
    , _integral(0.0)
    , _lastError(0.0)
    , _lastInput(0.0)
    , _lastDerivative(0.0)
    , _lastOutput(0.0)
    , _saturated(false)
    , _lastComputeMs(0)
    , _outputSlewPerSec(PID_OUTPUT_SLEW_PWM_PER_SEC)
    , _slewedOutput(0.0)
    , _state(PID_STATE_IDLE)
    , _atHeating(true)
    , _atMaxTemp(0.0)
    , _atMinTemp(100.0)
    , _atCycleCount(0)
    , _atLastSwitchTime(0)
    , _atStartTime(0)
    , _atPeriodSumMs(0)
    , _atLastPeakMs(0)
    , _manualPWM(0)
{
}

void PIDController::begin() {
    _state = PID_STATE_AUTOTUNING;
    _atHeating = true;
    _atMaxTemp = 0.0;
    _atMinTemp = 100.0;
    _atCycleCount = 0;
    _atLastSwitchTime = millis();
    _atStartTime = millis();
    _atPeriodSumMs = 0;
    _atLastPeakMs = 0;
    Serial.println("[PID] Auto-tuning baslatildi");
}

// ==================== Setpoint ====================
void PIDController::setSetpoint(double setpoint) {
    _setpointTarget = setpoint;
    // Ilk ayarda setpointActive direkt atlasin (ramping cihaz acildigindan basliyor)
    if (_state == PID_STATE_IDLE || _setpointActive == 0.0) {
        _setpointActive = setpoint;
    }
    // Aksi halde compute() icinde kademeli yaklasilacak
}

double PIDController::getSetpoint() const {
    return _setpointTarget;
}

double PIDController::getActiveSetpoint() const {
    return _setpointActive;
}

void PIDController::setSetpointRampRate(double degPerSec) {
    _setpointRampDegPerSec = (degPerSec < 0.0) ? 0.0 : degPerSec;
}

// ==================== Compute (Time-Based PID) ====================
uint8_t PIDController::compute(double input) {
    // RUNNING degilse manuel mod veya idle: bumpless transfer icin output korunur
    if (_state == PID_STATE_MANUAL) {
        _lastOutput = (double)_manualPWM;
        _slewedOutput = _lastOutput;
        return _manualPWM;
    }
    if (_state != PID_STATE_RUNNING) {
        return 0;
    }

    // ---------- dt hesapla (saniye cinsinden) ----------
    unsigned long now = millis();
    double dt = 0.0;
    if (_lastComputeMs == 0) {
        // Ilk cagri: dt yok, derivative term sifir
        _lastComputeMs = now;
        _lastInput = input;
        // Bumpless transfer baslangici icin slewedOutput = lastOutput
        _slewedOutput = _lastOutput;
        return (uint8_t)constrain((int)_slewedOutput, PID_OUTPUT_MIN, PID_OUTPUT_MAX);
    }
    dt = (double)(now - _lastComputeMs) / 1000.0;
    if (dt <= 0.0 || dt > 60.0) {
        // dt olmadi veya cok uzun (saat atlamasi): integral/derivative bozulmasin
        _lastComputeMs = now;
        return (uint8_t)constrain((int)_lastOutput, PID_OUTPUT_MIN, PID_OUTPUT_MAX);
    }
    _lastComputeMs = now;

    // ---------- Setpoint ramping ----------
    if (_setpointRampDegPerSec > 0.0 && _setpointActive != _setpointTarget) {
        double maxStep = _setpointRampDegPerSec * dt;
        double diff = _setpointTarget - _setpointActive;
        if (fabs(diff) <= maxStep) {
            _setpointActive = _setpointTarget;
        } else if (diff > 0.0) {
            _setpointActive += maxStep;
        } else {
            _setpointActive -= maxStep;
        }
    } else if (_setpointRampDegPerSec == 0.0) {
        _setpointActive = _setpointTarget;
    }

    double error = _setpointActive - input;

    // ---------- Proportional ----------
    double pTerm = _kp * error;

    // ---------- Derivative on Measurement (D-kick onleme) ----------
    // Klasik: D = Kd * d(error)/dt
    // Daha iyi: D = -Kd * d(input)/dt  (setpoint degisimi spike yapmaz)
    double dInput = input - _lastInput;
    double rawDeriv = -_kd * (dInput / dt);
    // LPF: turev gurultusunu yumusat
    double alpha = PID_DERIVATIVE_LPF_ALPHA;
    if (alpha < 0.0) alpha = 0.0;
    if (alpha > 1.0) alpha = 1.0;
    double dTerm = (alpha * _lastDerivative) + ((1.0 - alpha) * rawDeriv);
    _lastDerivative = dTerm;

    // ---------- Integral (conditional anti-windup) ----------
    double newIntegral = _integral + (_ki * error * dt);
    // Integral clamp (klasik koruma)
    if (newIntegral > PID_INTEGRAL_MAX) newIntegral = PID_INTEGRAL_MAX;
    if (newIntegral < PID_INTEGRAL_MIN) newIntegral = PID_INTEGRAL_MIN;

    // Tentatif output ile saturasyon kontrolu
    double tentativeOutput = pTerm + newIntegral + dTerm;
    bool willSaturate = (tentativeOutput > PID_OUTPUT_MAX) ||
                        (tentativeOutput < PID_OUTPUT_MIN);

    // Conditional integration: sature olacak ve hata yonu integrali daha kotuye
    // goturuyorsa, integrali biriktirme (anti-windup proper)
    if (willSaturate) {
        bool errorPushesUp   = (error > 0.0);
        bool outputAtUpper   = (tentativeOutput > PID_OUTPUT_MAX);
        bool outputAtLower   = (tentativeOutput < PID_OUTPUT_MIN);
        // Eger error yukari iterken cikis ust limitte ise integral arttirma
        if ((errorPushesUp && outputAtUpper) ||
            (!errorPushesUp && outputAtLower)) {
            // integrali oldugu yerde tut
        } else {
            _integral = newIntegral;
        }
    } else {
        _integral = newIntegral;
    }

    // ---------- Final output ----------
    double rawOutput = pTerm + _integral + dTerm;
    _lastOutput = rawOutput;

    // ---------- Output slew rate limiting ----------
    if (_outputSlewPerSec > 0.0) {
        double maxDelta = _outputSlewPerSec * dt;
        double diff = rawOutput - _slewedOutput;
        if (diff > maxDelta) {
            _slewedOutput += maxDelta;
        } else if (diff < -maxDelta) {
            _slewedOutput -= maxDelta;
        } else {
            _slewedOutput = rawOutput;
        }
    } else {
        _slewedOutput = rawOutput;
    }

    // ---------- Sinirla ----------
    int pwm = constrain((int)_slewedOutput, PID_OUTPUT_MIN, PID_OUTPUT_MAX);
    _saturated = (pwm == PID_OUTPUT_MIN) || (pwm == PID_OUTPUT_MAX);

    // State guncelle
    _lastError = error;
    _lastInput = input;

    return (uint8_t)pwm;
}

// ==================== Auto-Tune (Iyilestirilmis) ====================
void PIDController::startAutoTune() {
    _state = PID_STATE_AUTOTUNING;
    _atHeating = true;
    _atMaxTemp = 0.0;
    _atMinTemp = 100.0;
    _atCycleCount = 0;
    _atLastSwitchTime = millis();
    _atStartTime = millis();
    _atPeriodSumMs = 0;
    _atLastPeakMs = 0;
    _integral = 0.0;
    _lastError = 0.0;
    _lastInput = 0.0;
    _lastDerivative = 0.0;
    _lastComputeMs = 0;
    Serial.println("[PID] Auto-tuning yeniden baslatildi");
}

bool PIDController::isAutoTuning() const {
    return (_state == PID_STATE_AUTOTUNING);
}

void PIDController::autoTuneStep(double input) {
    if (_state != PID_STATE_AUTOTUNING) return;

    // Min/max takibi
    if (input > _atMaxTemp) _atMaxTemp = input;
    if (input < _atMinTemp) _atMinTemp = input;

    // Histerezis ile yon degistirme + periyot olcumu
    if (_atHeating && input > _setpointTarget + AUTOTUNE_HYSTERESIS) {
        _atHeating = false;
        _atCycleCount++;
        unsigned long now = millis();
        // Periyot (peak-to-peak) olc: her zoom-in'de bir periyot
        if (_atLastPeakMs != 0) {
            _atPeriodSumMs += (now - _atLastPeakMs);
        }
        _atLastPeakMs = now;
        Serial.print("[PID-AT] Dongu #");
        Serial.print(_atCycleCount);
        Serial.print(" - Temp: ");
        Serial.println(input);
        _atLastSwitchTime = now;
    }

    if (!_atHeating && input < _setpointTarget - AUTOTUNE_HYSTERESIS) {
        _atHeating = true;
        _atLastSwitchTime = millis();
    }

    // Yeterli salinim tamamlandi
    if (_atCycleCount >= AUTOTUNE_CYCLES) {
        double amplitude = (_atMaxTemp - _atMinTemp) / 2.0;
        if (amplitude < 0.01) amplitude = 0.01;

        // Periyot: kayitli peak-to-peak ortalamasi (daha hassas)
        // _atPeriodSumMs ilk donguden sonra biriktiyor (count-1 periyot)
        double Tu = 0.0;
        if (_atCycleCount > 1 && _atPeriodSumMs > 0) {
            Tu = (double)_atPeriodSumMs / (double)(_atCycleCount - 1) / 1000.0;
        } else {
            // Fallback: total time / cycles
            unsigned long totalTime = millis() - _atStartTime;
            Tu = (double)totalTime / (double)_atCycleCount / 1000.0;
        }
        if (Tu < 0.1) Tu = 0.1; // korku payi (sifira bolunmesin)

        // Ku hesaplama (ultimate gain)
        double Ku = (4.0 * (double)AUTOTUNE_OUTPUT) / (3.14159 * amplitude);

        // Tuning rule uygula
        applyTuningRule(Ku, Tu);

        Serial.println("[PID] ===== AUTO-TUNE TAMAMLANDI =====");
        Serial.print("[PID] Ku="); Serial.print(Ku);
        Serial.print(" Tu="); Serial.println(Tu);
        Serial.print("[PID] Kp="); Serial.print(_kp);
        Serial.print(" Ki="); Serial.print(_ki);
        Serial.print(" Kd="); Serial.println(_kd);

        _state = PID_STATE_RUNNING;
        _integral = 0.0;
        _lastError = 0.0;
        _lastInput = input;            // bumpless start
        _lastDerivative = 0.0;
        _lastComputeMs = 0;
        return;
    }
}

// Tuning kurali uygulama (Z-N Classic, Pessen, Tyreus-Luyben, No-Overshoot)
void PIDController::applyTuningRule(double Ku, double Tu) {
    switch (_tuningRule) {
        case PID_TUNING_ZN_CLASSIC:
            _kp = 0.6  * Ku;
            _ki = 1.2  * Ku / Tu;
            _kd = 0.075 * Ku * Tu;
            break;
        case PID_TUNING_ZN_PESSEN:    // varsayilan, Pessen Integral
            _kp = 0.7  * Ku;
            _ki = 1.75 * Ku / Tu;
            _kd = 0.105 * Ku * Tu;
            break;
        case PID_TUNING_TYREUS_LUYBEN:
            _kp = 0.45 * Ku;
            _ki = _kp / (2.2 * Tu);
            _kd = _kp * (Tu / 6.3);
            break;
        case PID_TUNING_NO_OVERSHOOT:
            _kp = 0.2  * Ku;
            _ki = 0.4  * Ku / Tu;
            _kd = 0.067 * Ku * Tu;
            break;
    }
}

void PIDController::setTuningRule(PIDTuningRule rule) {
    _tuningRule = rule;
}

// ==================== Parametre ====================
double PIDController::getKp() const { return _kp; }
double PIDController::getKi() const { return _ki; }
double PIDController::getKd() const { return _kd; }

void PIDController::setParameters(double kp, double ki, double kd) {
    _kp = kp;
    _ki = ki;
    _kd = kd;
    _state = PID_STATE_RUNNING;
    _integral = 0.0;
    _lastError = 0.0;
    _lastDerivative = 0.0;
    _lastComputeMs = 0;       // ilk cagriya kadar dt sifir
    Serial.print("[PID] Parametreler ayarlandi Kp=");
    Serial.print(kp);
    Serial.print(" Ki=");
    Serial.print(ki);
    Serial.print(" Kd=");
    Serial.println(kd);
}

// ==================== Manuel mod ====================
void PIDController::setManualOutput(uint8_t pwm) {
    _manualPWM = pwm;
    _lastOutput = (double)pwm;
    _slewedOutput = (double)pwm;
}

void PIDController::setManualMode(bool enabled) {
    if (enabled) {
        // Bumpless transfer: manuel girince mevcut output korunur
        _state = PID_STATE_MANUAL;
        _manualPWM = (uint8_t)constrain((int)_lastOutput, PID_OUTPUT_MIN, PID_OUTPUT_MAX);
    } else {
        // Otomatige donus: integral'i mevcut output'tan back-calculate et
        // (bumpless): integral = lastOutput - pTerm - dTerm
        _state = PID_STATE_RUNNING;
        // Basitleştirilmiş bumpless: integrali mevcut PWM degerine seed et
        _integral = (double)_manualPWM;
        if (_integral > PID_INTEGRAL_MAX) _integral = PID_INTEGRAL_MAX;
        if (_integral < PID_INTEGRAL_MIN) _integral = PID_INTEGRAL_MIN;
        _lastDerivative = 0.0;
        _lastComputeMs = 0;
        _slewedOutput = _lastOutput;
    }
}

// ==================== Output slew ====================
void PIDController::setOutputSlewRate(double pwmPerSec) {
    _outputSlewPerSec = (pwmPerSec < 0.0) ? 0.0 : pwmPerSec;
}

// ==================== State / Telemetri ====================
PIDState PIDController::getState() const {
    return _state;
}

double PIDController::getLastOutput() const {
    return _lastOutput;
}

double PIDController::getLastError() const {
    return _lastError;
}

void PIDController::reset() {
    _integral = 0.0;
    _lastError = 0.0;
    _lastInput = 0.0;
    _lastDerivative = 0.0;
    _lastOutput = 0.0;
    _slewedOutput = 0.0;
    _saturated = false;
    _lastComputeMs = 0;
    _state = PID_STATE_IDLE;
    _setpointActive = _setpointTarget;
}
