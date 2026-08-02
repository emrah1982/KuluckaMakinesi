#ifndef INCUBATION_SERVICE_H
#define INCUBATION_SERVICE_H

#include <Arduino.h>
#include "../config/Config.h"
#include "../config/AnimalProfiles.h"
#include "../hal/SensorManager.h"
#include "../hal/RelayBoard.h"
#include "../hal/HeaterDriver.h"
#include "../hal/FanDriver.h"
#include "../hal/HumidifierDriver.h"
#include "../hal/TurnerDriver.h"
#include "../hal/DisplayManager.h"
#include "../hal/RTCManager.h"
#include "../control/PIDController.h"
#include "../control/HumidityController.h"
#include "../control/FanController.h"
#include "../control/PhaseManager.h"
#include "AlarmService.h"
#include "SafetyService.h"
#include "../hal/PersistentStorage.h"

enum SystemState {
    SYS_INITIALIZING,
    SYS_AUTOTUNING,
    SYS_RUNNING,
    SYS_PAUSED,
    SYS_COMPLETED,
    SYS_EMERGENCY
};

struct SystemStatus {
    float    temperature;
    float    humidity;
    float    targetTemp;
    float    targetHumLow;
    float    targetHumHigh;
    uint8_t  heaterPWM;
    uint8_t  fanPWM;
    bool     humidifierOn;
    int      currentDay;
    int      totalDays;
    int      remainingDays;
    int      phaseRemainingDays;
    int      phaseEndDay;
    const char* phaseName;
    const char* profileName;
    SystemState state;
    bool     sensor1OK;
    bool     sensor2OK;
    double   kp, ki, kd;
    bool     alarmActive;
    String   alarmMsg;
    unsigned long uptime;
};

class IncubationService {
public:
    IncubationService();

    void begin();
    void update();

    void setProfile(uint8_t profileIndex);
    void start();
    void pause();
    void resume();
    void stop();

    SystemStatus getStatus() const;
    String getStatusJSON() const;

    // WiFi üzerinden erişilebilir kontroller
    void setPIDParams(double kp, double ki, double kd);
    void setTargetTemp(float temp);
    void setHumidityThresholds(float low, float high);
    void resetSafety();

    // Bileşenlere erişim
    PhaseManager& getPhaseManager();
    AlarmService& getAlarmService();

private:
    // HAL
    SensorManager    _sensorMgr;
    HeaterDriver     _heater;
    FanDriver        _fan;
    HumidifierDriver _humidifier;
    TurnerDriver     _turner;
    DisplayManager   _display;
    RTCManager       _rtc;

    // Control
    PIDController      _pid;
    HumidityController _humCtrl;
    FanController      _fanCtrl;
    PhaseManager       _phaseMgr;

    // Service
    AlarmService  _alarm;
    SafetyService _safety;
    PersistentStorage _storage;

    SystemState   _state;
    unsigned long _lastUpdateTime;
    unsigned long _startMillis;

    void updatePhase();
    void updateControls();
    void updateDisplay();
    void debugLog();
};

#endif // INCUBATION_SERVICE_H
