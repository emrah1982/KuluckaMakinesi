#include "AlarmService.h"
#include "SpeakerDriver.h"

AlarmService::AlarmService()
    : _historyIndex(0)
    , _historyCount(0)
    , _activeAlarm(ALARM_NONE)
    , _activeMessage("")
    , _lastAlarmTime(0)
    , _alarmActive(false)
    , _muted(false)
    , _muteUntilMs(0)
    , _mutedType(ALARM_NONE)
    , _speaker(nullptr)
{
}

void AlarmService::begin() {
}

void AlarmService::check(float temperature, float humidity, bool sensorOK,
                         uint16_t co2, uint16_t co2High, uint16_t co2Critical) {
    // Cooldown kontrolü
    if (millis() - _lastAlarmTime < ALARM_COOLDOWN && _alarmActive) {
        return;
    }

    if (!sensorOK) {
        triggerAlarm(ALARM_SENSOR_FAIL, "Sensor hatasi! Okuma yapilamiyor.");
        return;
    }

    // CO₂ kritik seviye kontrolü (en yüksek öncelik)
    if (co2Critical > 0 && co2 >= co2Critical) {
        triggerAlarm(ALARM_CO2_CRITICAL, "KRITIK CO2: " + String(co2) + " ppm");
        return;
    }
    
    // CO₂ yüksek seviye kontrolü
    if (co2High > 0 && co2 >= co2High) {
        triggerAlarm(ALARM_CO2_HIGH, "Yuksek CO2: " + String(co2) + " ppm");
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
            if (_activeAlarm == ALARM_TURNING_STOPPED || _activeAlarm == ALARM_INCUBATION_COMPLETE || _activeAlarm == ALARM_POWER_RECOVERY) {
                return;
            }
            clearAll();
        }
    }
}

// ---------------------------------------------------------------------
//  Hoparlor durumunu alarm durumuyla esitle
//
//  Ses artik DURUM tabanli: "alarm aktif ve susturulmamissa hoparlor
//  calar" degismezi her dongude saglanir. Boylece susturma suresi
//  dolar dolmaz ses geri gelir; triggerAlarm'in tekrar cagrilmasini
//  beklemek gerekmez.
//
//  isPlaying() KULLANILMAZ: alarm deseninin sessiz araliklarinda (200 ms
//  OFF, 1500 ms bekleme) false doner ve desen surekli bastan baslardi.
//  isAlarmPatternActive() bu araliklarda da true kalir.
// ---------------------------------------------------------------------
void AlarmService::update() {
    if (!_speaker) return;

    bool shouldSound = _alarmActive && !isMuted();

    if (shouldSound && !_speaker->isAlarmPatternActive()) {
        _speaker->startAlarmPattern();
    } else if (!shouldSound && _speaker->isAlarmPatternActive()) {
        // Alarm temizlendi veya susturuldu ama ses surüyorsa kes
        _speaker->stop();
    }
}

void AlarmService::triggerAlarm(AlarmType type, const String &message) {
    bool newType = (_activeAlarm != type);   // farkli tipte alarm geldiyse mute kalksin
    _activeAlarm = type;
    _activeMessage = message;
    _alarmActive = true;
    _lastAlarmTime = millis();

    // Geçmişe ekle
    _history[_historyIndex].type = type;
    _history[_historyIndex].message = message;
    _history[_historyIndex].timestamp = millis();
    _history[_historyIndex].active = true;

    _historyIndex = (_historyIndex + 1) % MAX_ALARM_HISTORY;
    if (_historyCount < MAX_ALARM_HISTORY) _historyCount++;

    // Mute kontrolu: ayni tip alarm icin sustur aktifse ve sure dolmamissa
    // hoparloru calistirma. Farkli tip veya sure doldu ise tekrar cal.
    bool muteStillValid = _muted && (millis() < _muteUntilMs) && (type == _mutedType);
    if (!muteStillValid && _speaker) {
        _speaker->startAlarmPattern();
    }
    // Yeni alarm tipi geldi -> mute reset (kullanici tekrar onaylasin)
    if (newType) {
        _muted = false;
        _mutedType = ALARM_NONE;
    }
}

void AlarmService::acknowledgeAlarm() {
    if (!_alarmActive) return;
    _muted = true;
    _mutedType = _activeAlarm;
    _muteUntilMs = millis() + ALARM_MUTE_DURATION_MS;
    if (_speaker) _speaker->stop();
}

void AlarmService::snoozeAlarm(uint32_t durationMs) {
    if (!_alarmActive) return;
    if (durationMs == 0) durationMs = ALARM_MUTE_DURATION_MS;
    _muted = true;
    _mutedType = _activeAlarm;
    _muteUntilMs = millis() + durationMs;
    if (_speaker) _speaker->stop();
}

void AlarmService::dismissAlarm() {
    if (!_alarmActive) return;
    AlarmType t = _activeAlarm;
    // Aktif alarmi kapat
    _activeAlarm = ALARM_NONE;
    _activeMessage = "";
    _alarmActive = false;
    if (_speaker) _speaker->stop();
    // Bu tipte alarm tekrar gelirse uzun sure sustur (24 saat)
    // -> Ayni "kontrol gunu" icinde tekrar tetikleme uyarisini onler
    _muted = true;
    _mutedType = t;
    _muteUntilMs = millis() + (24UL * 60UL * 60UL * 1000UL);
}

bool AlarmService::isMuted() const {
    return _muted && (millis() < _muteUntilMs);
}

bool AlarmService::shouldAutoShowModal() const {
    return _alarmActive && !isMuted();
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
    // Durum normale donduyse mute da reset (yeni alarm bagimsiz baslayabilsin)
    _muted = false;
    _mutedType = ALARM_NONE;
    if (_speaker) _speaker->stop();
}

// Profil bazli dinamik esik kontrolu — yeni v3 alarm akisi.
// Eski check() ile farki: sicaklik esikleri statik yerine targetTemp ± tolerans;
// nem esikleri dogrudan profil humLow/humHigh degerleri.
void AlarmService::checkDynamic(float temperature, float humidity, bool sensorOK,
                                float targetTemp, float tempTolerance,
                                float humLow, float humHigh,
                                uint16_t co2, uint16_t co2High, uint16_t co2Critical) {
    if (millis() - _lastAlarmTime < ALARM_COOLDOWN && _alarmActive) return;

    if (!sensorOK) {
        triggerAlarm(ALARM_SENSOR_FAIL, "Sensor hatasi! Okuma yapilamiyor.");
        return;
    }

    // CO2 oncelik (kritik > yuksek)
    if (co2Critical > 0 && co2 >= co2Critical) {
        triggerAlarm(ALARM_CO2_CRITICAL, "KRITIK CO2: " + String(co2) + " ppm");
        return;
    }
    if (co2High > 0 && co2 >= co2High) {
        triggerAlarm(ALARM_CO2_HIGH, "Yuksek CO2: " + String(co2) + " ppm");
        return;
    }

    // Profil bazli sicaklik esikleri
    float tempUpper = targetTemp + tempTolerance;
    float tempLower = targetTemp - tempTolerance;

    if (temperature > tempUpper) {
        triggerAlarm(ALARM_TEMP_HIGH,
            "Yuksek sicaklik: " + String(temperature, 1) + "C (hedef " +
            String(targetTemp, 1) + ")");
    } else if (temperature < tempLower) {
        triggerAlarm(ALARM_TEMP_LOW,
            "Dusuk sicaklik: " + String(temperature, 1) + "C (hedef " +
            String(targetTemp, 1) + ")");
    } else if (humidity > humHigh) {
        triggerAlarm(ALARM_HUM_HIGH,
            "Yuksek nem: %" + String(humidity, 1) + " (max %" + String(humHigh, 0) + ")");
    } else if (humidity < humLow) {
        triggerAlarm(ALARM_HUM_LOW,
            "Dusuk nem: %" + String(humidity, 1) + " (min %" + String(humLow, 0) + ")");
    } else {
        if (_alarmActive) {
            // Bu tipler durum normale dondu diye otomatik temizlenmemeli
            if (_activeAlarm == ALARM_TURNING_STOPPED ||
                _activeAlarm == ALARM_INCUBATION_COMPLETE ||
                _activeAlarm == ALARM_POWER_RECOVERY ||
                _activeAlarm == ALARM_SENSOR_MISMATCH) {
                return;  // Mismatch, IncubationService tarafindan clearAlarm ile temizlenir
            }
            clearAll();
        }
    }
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
