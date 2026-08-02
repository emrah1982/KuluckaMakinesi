#include "PIDController.h"

PIDController::PIDController()
    : _setpoint(37.5)
    , _kp(PID_DEFAULT_KP)
    , _ki(PID_DEFAULT_KI)
    , _kd(PID_DEFAULT_KD)
    , _integral(0.0)
    , _lastError(0.0)
    , _lastOutput(0.0)
    , _state(PID_STATE_IDLE)
    , _atHeating(true)
    , _atMaxTemp(0.0)
    , _atMinTemp(100.0)
    , _atCycleCount(0)
    , _atLastSwitchTime(0)
    , _atStartTime(0)
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
    Serial.println("[PID] Auto-tuning baslatildi");
}

void PIDController::setSetpoint(double setpoint) {
    _setpoint = setpoint;
}

double PIDController::getSetpoint() const {
    return _setpoint;
}

uint8_t PIDController::compute(double input) {
    if (_state != PID_STATE_RUNNING) return 0;

    double error = _setpoint - input;

    // Integral hesaplama + windup koruma
    _integral += error;
    if (_integral > PID_INTEGRAL_MAX) _integral = PID_INTEGRAL_MAX;
    if (_integral < PID_INTEGRAL_MIN) _integral = PID_INTEGRAL_MIN;

    // Derivative
    double derivative = error - _lastError;
    _lastError = error;

    // PID çıkışı
    _lastOutput = _kp * error + _ki * _integral + _kd * derivative;

    // Çıkışı sınırla
    int pwm = constrain((int)_lastOutput, PID_OUTPUT_MIN, PID_OUTPUT_MAX);
    return (uint8_t)pwm;
}

void PIDController::startAutoTune() {
    _state = PID_STATE_AUTOTUNING;
    _atHeating = true;
    _atMaxTemp = 0.0;
    _atMinTemp = 100.0;
    _atCycleCount = 0;
    _atLastSwitchTime = millis();
    _atStartTime = millis();
    _integral = 0.0;
    _lastError = 0.0;
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

    // Histerezis ile yön değiştirme
    if (_atHeating && input > _setpoint + AUTOTUNE_HYSTERESIS) {
        _atHeating = false;
        _atCycleCount++;
        unsigned long now = millis();
        Serial.print("[PID-AT] Dongu #");
        Serial.print(_atCycleCount);
        Serial.print(" - Temp: ");
        Serial.println(input);
        _atLastSwitchTime = now;
    }

    if (!_atHeating && input < _setpoint - AUTOTUNE_HYSTERESIS) {
        _atHeating = true;
        _atLastSwitchTime = millis();
    }

    // Yeterli salınım tamamlandı
    if (_atCycleCount >= AUTOTUNE_CYCLES) {
        double amplitude = (_atMaxTemp - _atMinTemp) / 2.0;
        if (amplitude < 0.01) amplitude = 0.01;

        // Periyot hesaplama
        unsigned long totalTime = millis() - _atStartTime;
        double Tu = (double)totalTime / (double)_atCycleCount / 1000.0;

        // Ku hesaplama (ultimate gain)
        double Ku = (4.0 * (double)AUTOTUNE_OUTPUT) / (3.14159 * amplitude);

        // Ziegler-Nichols PID parametreleri
        _kp = 0.6 * Ku;
        _ki = 1.2 * Ku / Tu;
        _kd = 0.075 * Ku * Tu;

        Serial.println("[PID] ===== AUTO-TUNE TAMAMLANDI =====");
        Serial.print("[PID] Ku="); Serial.print(Ku);
        Serial.print(" Tu="); Serial.println(Tu);
        Serial.print("[PID] Kp="); Serial.print(_kp);
        Serial.print(" Ki="); Serial.print(_ki);
        Serial.print(" Kd="); Serial.println(_kd);

        _state = PID_STATE_RUNNING;
        _integral = 0.0;
        _lastError = 0.0;
        return;
    }
}

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
    Serial.print("[PID] Parametreler ayarlandi Kp=");
    Serial.print(kp);
    Serial.print(" Ki=");
    Serial.print(ki);
    Serial.print(" Kd=");
    Serial.println(kd);
}

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
    _lastOutput = 0.0;
    _state = PID_STATE_IDLE;
}
