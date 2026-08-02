#ifndef ALARM_SERVICE_H
#define ALARM_SERVICE_H

#include <Arduino.h>
#include "../config/Config.h"
#include "../hal/RelayBoard.h"

enum AlarmType {
    ALARM_NONE,
    ALARM_TEMP_HIGH,
    ALARM_TEMP_LOW,
    ALARM_HUM_HIGH,
    ALARM_HUM_LOW,
    ALARM_SENSOR_FAIL,
    ALARM_SAFETY_SHUTDOWN,
    ALARM_TURNING_STOPPED,
    ALARM_INCUBATION_COMPLETE
};

struct AlarmEvent {
    AlarmType   type;
    String      message;
    unsigned long timestamp;
    bool        active;
};

#define MAX_ALARM_HISTORY 20

class AlarmService {
public:
    AlarmService();

    void begin();
    void check(float temperature, float humidity, bool sensorOK);
    void triggerAlarm(AlarmType type, const String &message);
    void clearAlarm(AlarmType type);
    void clearAll();

    void setBuzzer(bool on);
    void setLED(bool on);

    bool hasActiveAlarm() const;
    AlarmType getActiveAlarmType() const;
    String getActiveAlarmMessage() const;

    const AlarmEvent* getHistory() const;
    uint8_t getHistoryCount() const;

    String getAlarmJSON() const;

private:
    AlarmEvent _history[MAX_ALARM_HISTORY];
    uint8_t    _historyIndex;
    uint8_t    _historyCount;

    AlarmType  _activeAlarm;
    String     _activeMessage;
    unsigned long _lastAlarmTime;
    bool       _alarmActive;
};

#endif // ALARM_SERVICE_H
