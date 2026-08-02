#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

#include <Arduino.h>
#include "../config/Config.h"

enum PIDState {
    PID_STATE_IDLE,
    PID_STATE_AUTOTUNING,
    PID_STATE_RUNNING
};

class PIDController {
public:
    PIDController();

    void begin();
    void setSetpoint(double setpoint);
    double getSetpoint() const;

    uint8_t compute(double input);

    // Auto-tuning (Ziegler-Nichols)
    void startAutoTune();
    bool isAutoTuning() const;
    void autoTuneStep(double input);

    // PID parametreleri
    double getKp() const;
    double getKi() const;
    double getKd() const;
    void setParameters(double kp, double ki, double kd);

    PIDState getState() const;
    double getLastOutput() const;
    double getLastError() const;

    void reset();

private:
    double _setpoint;
    double _kp, _ki, _kd;
    double _integral;
    double _lastError;
    double _lastOutput;

    PIDState _state;

    // Auto-tune değişkenleri
    bool   _atHeating;
    double _atMaxTemp;
    double _atMinTemp;
    int    _atCycleCount;
    unsigned long _atLastSwitchTime;
    unsigned long _atStartTime;
};

#endif // PID_CONTROLLER_H
