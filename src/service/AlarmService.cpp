#include "AlarmService.h"

AlarmService::AlarmService()
    : _historyIndex(0)
    , _historyCount(0)
    , _activeAlarm(ALARM_NONE)
    , _activeMessage("")
    , _lastAlarmTime(0)
    , _alarmActive(false)
{
}

void AlarmService::begin() {
    Serial.println("[ALARM] Alarm servisi baslatildi");
}

void AlarmService::check(float temperature, float humidity, bool sensorOK) {
    // Cooldown kontrolü
    if (millis() - _lastAlarmTime < ALARM_COOLDOWN && _alarmActive) {
        return;
    }

    if (!sensorOK) {
        triggerAlarm(ALARM_SENSOR_FAIL, "Sensor hatasi! Okuma yapilamiyor.");
        return;
    }

    if (temperature > ALARM_THRESHOLD_TEMP_HIGH) {
        triggerAlarm(ALARM_TEMP_HIGH, "Yuksek sicaklik: " + String(temperature, 1) + "C");
    } else if (temperature < ALARM_THRESHOLD_TEMP_LOW) {
        triggerAlarm(ALARM_TEMP_LOW, "Dusuk sicaklik: " + String(temperature, 1) + "C");
    } else if (humidity > ALARM_THRESHOLD_HUM_HIGH) {
        triggerAlarm(ALARM_HUM_HIGH, "Yuksek nem: %" + String(humidity, 1));
    } else if (humidity < ALARM_THRESHOLD_HUM_LOW) {
        triggerAlarm(ALARM_HUM_LOW, "Dusuk nem: %" + String(humidity, 1));
    } else {
        if (_alarmActive) {
            if (_activeAlarm == ALARM_TURNING_STOPPED || _activeAlarm == ALARM_INCUBATION_COMPLETE) {
                return;
            }
            clearAll();
        }
    }
}

void AlarmService::triggerAlarm(AlarmType type, const String &message) {
    _activeAlarm = type;
    _activeMessage = message;
    _alarmActive = true;
    _lastAlarmTime = millis();

    // Buzzer ac
    RelayBoard::instance().setBuzzer(true);

    // Geçmişe ekle
    _history[_historyIndex].type = type;
    _history[_historyIndex].message = message;
    _history[_historyIndex].timestamp = millis();
    _history[_historyIndex].active = true;

    _historyIndex = (_historyIndex + 1) % MAX_ALARM_HISTORY;
    if (_historyCount < MAX_ALARM_HISTORY) _historyCount++;

    Serial.print("[ALARM] ");
    Serial.println(message);
}

void AlarmService::clearAlarm(AlarmType type) {
    if (_activeAlarm == type) {
        _activeAlarm = ALARM_NONE;
        _activeMessage = "";
        _alarmActive = false;
    }
}

void AlarmService::clearAll() {
    _activeAlarm = ALARM_NONE;
    _activeMessage = "";
    _alarmActive = false;

    // Buzzer kapat
    RelayBoard::instance().setBuzzer(false);
}

void AlarmService::setBuzzer(bool on) {
    RelayBoard::instance().setBuzzer(on);
}

void AlarmService::setLED(bool on) {
    RelayBoard::instance().setLED(on);
}

bool AlarmService::hasActiveAlarm() const {
    return _alarmActive;
}

AlarmType AlarmService::getActiveAlarmType() const {
    return _activeAlarm;
}

String AlarmService::getActiveAlarmMessage() const {
    return _activeMessage;
}

const AlarmEvent* AlarmService::getHistory() const {
    return _history;
}

uint8_t AlarmService::getHistoryCount() const {
    return _historyCount;
}

String AlarmService::getAlarmJSON() const {
    String json = "{\"active\":";
    json += _alarmActive ? "true" : "false";
    json += ",\"type\":";
    json += String((int)_activeAlarm);
    json += ",\"message\":\"";
    json += _activeMessage;
    json += "\",\"history\":[";

    uint8_t count = min(_historyCount, (uint8_t)5);
    for (uint8_t i = 0; i < count; i++) {
        int idx = (_historyIndex - 1 - i + MAX_ALARM_HISTORY) % MAX_ALARM_HISTORY;
        if (i > 0) json += ",";
        json += "{\"type\":";
        json += String((int)_history[idx].type);
        json += ",\"msg\":\"";
        json += _history[idx].message;
        json += "\"}";
    }
    json += "]}";
    return json;
}
