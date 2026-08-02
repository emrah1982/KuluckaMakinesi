#include "IncubationService.h"
#include <WiFi.h>

IncubationService::IncubationService()
    : _humCtrl(_humidifier)
    , _fanCtrl(_fan)
    , _safety(_heater, _humidifier, _fan)
    , _state(SYS_INITIALIZING)
    , _lastUpdateTime(0)
    , _startMillis(0)
{
}

void IncubationService::begin() {
    Serial.println("============================================");
    Serial.println("  AKILLI KULUCKA KONTROL SISTEMI v1.0");
    Serial.println("============================================");

    // HAL başlat
    bool sensorOK = _sensorMgr.begin();
    RelayBoard::instance().begin();  // PCF8574 role karti (TCA9548A MUX CH1)
    _heater.begin();
    _fan.begin();
    _humidifier.begin();
    _turner.begin();
    _display.begin();
    bool rtcOK = _rtc.begin();

    bool nvsOK = _storage.begin();
    IncubationState st = _storage.loadIncubationState();

    // Servisler başlat
    _alarm.begin();
    _safety.begin();

    // PID başlat
    _pid.begin();

    if (st.valid) {
        setProfile(st.profileIndex);
    } else {
        setProfile(0);
    }

    if (!sensorOK) {
        Serial.println("[SYSTEM] UYARI: Sensor baslatilamadi!");
        _alarm.triggerAlarm(ALARM_SENSOR_FAIL, "Baslangicta sensor hatasi");
    }

    if (!rtcOK) {
        Serial.println("[SYSTEM] UYARI: RTC baslatilamadi!");
    }

    if (rtcOK && st.valid && st.startTimestamp > 0) {
        _rtc.setStartDate(DateTime((uint32_t)st.startTimestamp));
        _state = st.isRunning ? SYS_RUNNING : SYS_PAUSED;
    } else {
        _state = SYS_INITIALIZING;
    }
    _startMillis = millis();
    _lastUpdateTime = millis();

    Serial.println("[SYSTEM] Sistem hazir - Baslatmak icin BASLAT butonuna basin");
}

void IncubationService::update() {
    // Ekran + dokunmatik her zaman calisir (kendi ic zamanlayicisi var)
    updateDisplay();

    // Loop aralığı kontrolü
    unsigned long now = millis();
    if (now - _lastUpdateTime < LOOP_INTERVAL) {
        return;
    }
    _lastUpdateTime = now;

    // Acil durum kontrolü
    if (_state == SYS_EMERGENCY || _state == SYS_COMPLETED) {
        return;
    }

    // Baslangic veya duraklatma - sadece sensor oku, guvenlik kontrolu YAPMA
    if (_state == SYS_INITIALIZING || _state == SYS_PAUSED) {
        _sensorMgr.readAll();
        _turner.stop();
        return;
    }

    // 1. Sensör oku
    bool readOK = _sensorMgr.readAll();
    float temp = _sensorMgr.getTemperature();
    float hum  = _sensorMgr.getHumidity();

    // 2. Güvenlik kontrolü
    SafetyState safetyResult = _safety.check(temp, readOK, _sensorMgr.getFailCount());
    if (safetyResult == SAFETY_SHUTDOWN) {
        _state = SYS_EMERGENCY;
        _alarm.triggerAlarm(ALARM_SAFETY_SHUTDOWN, _safety.getShutdownReason());
        _turner.forceStop();
        Serial.println("[SYSTEM] ACIL DURUM - Sistem durduruldu!");
        return;
    }

    // 3. Alarm kontrolü
    _alarm.check(temp, hum, readOK);

    // Isitici role zamanlama guncelle (her loop'ta)
    _heater.update();

    // Yumurta cevirme motoru (faz bazli)
    // turningEnabled=false oldugunda (Cikim) otomatik durur.
    static bool lastTurningEnabled = true;
    bool turningEnabled = _phaseMgr.isTurningEnabled();
    if (lastTurningEnabled && !turningEnabled) {
        _alarm.triggerAlarm(ALARM_TURNING_STOPPED, "Cikim fazi: yumurta cevirme durduruldu");
    }
    lastTurningEnabled = turningEnabled;
    _turner.update(turningEnabled);

    // 4. Auto-tuning aşaması
    if (_state == SYS_AUTOTUNING) {
        if (_pid.isAutoTuning()) {
            _pid.autoTuneStep(temp);

            // Auto-tune sırasında ısıtıcıyı kontrol et
            if (temp < _pid.getSetpoint()) {
                _heater.setPWM(AUTOTUNE_OUTPUT);
            } else {
                _heater.setPWM(0);
            }

            // Fan minimum hızda çalışsın
            _fan.setPWM(FAN_MIN_PWM);
            return;
        } else {
            // Auto-tune tamamlandı
            _state = SYS_RUNNING;
            _rtc.setStartDate(_rtc.now());
            Serial.println("[SYSTEM] Auto-tune tamamlandi, kulucka basliyor!");
        }
    }

    // 5. Normal çalışma
    if (_state == SYS_RUNNING) {
        updatePhase();
        updateControls();
        debugLog();
    }
}

void IncubationService::updatePhase() {
    int day = _rtc.getElapsedDays();
    _phaseMgr.update(day);

    if (_phaseMgr.isComplete()) {
        _state = SYS_COMPLETED;
        _heater.stop();
        _humidifier.turnOff();
        _fan.setPWM(FAN_MIN_PWM);
        _turner.forceStop();
        _alarm.triggerAlarm(ALARM_INCUBATION_COMPLETE, "Kulucka tamamlandi!");
        Serial.println("[SYSTEM] KULUCKA TAMAMLANDI!");
        return;
    }

    // PID hedef sıcaklığını güncelle
    _pid.setSetpoint(_phaseMgr.getTargetTemperature());

    // Nem eşiklerini güncelle
    _humCtrl.setThresholds(_phaseMgr.getHumidityLow(), _phaseMgr.getHumidityHigh());
}

void IncubationService::updateControls() {
    float temp = _sensorMgr.getTemperature();
    float hum  = _sensorMgr.getHumidity();

    // PID ile ısıtıcı kontrolü
    uint8_t heaterPWM = _pid.compute(temp);
    _heater.setPWM(heaterPWM);

    // Nem kontrolü
    _humCtrl.update(hum);

    // Fan kontrolü (sıcaklığa bağlı)
    _fanCtrl.update(temp);
}

void IncubationService::debugLog() {
    static unsigned long lastLog = 0;
    if (millis() - lastLog < 5000) return;
    lastLog = millis();

    float temp = _sensorMgr.getTemperature();
    float hum  = _sensorMgr.getHumidity();

    Serial.print("[STATUS] T=");
    Serial.print(temp, 1);
    Serial.print("C H=");
    Serial.print(hum, 1);
    Serial.print("% Heater=");
    Serial.print(_heater.getCurrentPWM());
    Serial.print(" Fan=");
    Serial.print(_fan.getCurrentPWM());
    Serial.print(" Gun=");
    Serial.print(_rtc.getElapsedDays());
    Serial.print("/");
    Serial.print(_phaseMgr.getTotalDays());
    Serial.print(" Evre=");
    Serial.print(_phaseMgr.getPhaseName());

    // RAM monitor (heap)
    Serial.print(" | Heap=");
    Serial.print(ESP.getFreeHeap() / 1024);
    Serial.println("KB");
}

void IncubationService::updateDisplay() {
    int remainingDays = _phaseMgr.getRemainingDays();
    int phaseRemaining = _phaseMgr.getPhaseRemainingDays();
    const char* profileName = _phaseMgr.getCurrentProfile() ? _phaseMgr.getCurrentProfile()->name : "---";

    DisplayData dd;
    dd.temperature      = _sensorMgr.getTemperature();
    dd.humidity         = _sensorMgr.getHumidity();
    dd.targetTemp       = _phaseMgr.getTargetTemperature();
    dd.targetHumLow     = _phaseMgr.getHumidityLow();
    dd.targetHumHigh    = _phaseMgr.getHumidityHigh();
    dd.heaterPWM        = _heater.getCurrentPWM();
    dd.fanPWM           = _fan.getCurrentPWM();
    dd.humidifierOn     = _humidifier.isActive();
    dd.currentDay       = _phaseMgr.getCurrentDay();
    dd.totalDays        = _phaseMgr.getTotalDays();
    dd.remainingDays    = remainingDays;
    dd.phaseRemainingDays = phaseRemaining;
    {
        DateTime startDt = _rtc.getStartDate();
        char startBuf[11];
        snprintf(startBuf, sizeof(startBuf), "%02d/%02d/%04d", startDt.day(), startDt.month(), startDt.year());
        dd.startDate = String(startBuf);
        DateTime hatchDt = startDt + TimeSpan((dd.totalDays > 0 ? (dd.totalDays - 1) : 0), 0, 0, 0);
        char hatchBuf[11];
        snprintf(hatchBuf, sizeof(hatchBuf), "%02d/%02d/%04d", hatchDt.day(), hatchDt.month(), hatchDt.year());
        dd.hatchDate = String(hatchBuf);
    }
    dd.phaseName        = _phaseMgr.getPhaseName();
    dd.profileName      = profileName;
    dd.systemState      = (int)_state;
    dd.turningEnabled   = _phaseMgr.isTurningEnabled();
    dd.sensor1OK        = _sensorMgr.isSensor1OK();
    dd.sensor2OK        = _sensorMgr.isSensor2OK();
    dd.uptimeSec        = (millis() - _startMillis) / 1000;
    dd.alarmActive      = _alarm.hasActiveAlarm();
    dd.alarmMsg         = _alarm.getActiveAlarmMessage();
    dd.kp = _pid.getKp();
    dd.ki = _pid.getKi();
    dd.kd = _pid.getKd();
    dd.profile = _phaseMgr.getCurrentProfile();
    dd.currentPhaseIndex = 0;
    const AnimalProfile* prof = _phaseMgr.getCurrentProfile();
    const IncubationPhase* curPhase = _phaseMgr.getCurrentPhase();
    if (prof && curPhase) {
        for (uint8_t i = 0; i < prof->phaseCount; i++) {
            if (&prof->phases[i] == curPhase) { dd.currentPhaseIndex = i; break; }
        }
    }
    dd.apActive     = (WiFi.getMode() & WIFI_AP) != 0;
    dd.staConnected = WiFi.isConnected();
    dd.apIP         = WiFi.softAPIP().toString();
    dd.staIP        = WiFi.localIP().toString();
    dd.apClients    = WiFi.softAPgetStationNum();
    dd.freeHeap     = ESP.getFreeHeap();

    TouchAction action = _display.update(dd);

    switch (action) {
        case TOUCH_START:        start();        break;
        case TOUCH_PAUSE:        pause();        break;
        case TOUCH_RESUME:       resume();       break;
        case TOUCH_STOP:         stop();         break;
        case TOUCH_SAFETY_RESET: resetSafety();  break;
        case TOUCH_SET_START_DATE: {
            uint16_t y;
            uint8_t m, d;
            _display.getEditedStartDate(y, m, d);
            DateTime dt(y, m, d, 0, 0, 0);
            _rtc.setStartDate(dt);
            _storage.saveIncubationState(_phaseMgr.getProfileIndex(), (uint32_t)dt.unixtime(), (_state == SYS_RUNNING));
            break;
        }
        case TOUCH_PROFILE_SELECT: {
            uint8_t idx = _display.getSelectedProfileIdx();
            if (idx < PROFILE_COUNT) {
                setProfile(idx);
                Serial.printf("[INCUB] Profil degistirildi: %d - %s\n", idx, ALL_PROFILES[idx]->name);
            }
            break;
        }
        default: break;
    }

    _alarm.setLED(_state == SYS_RUNNING || _state == SYS_AUTOTUNING);
}

void IncubationService::setProfile(uint8_t profileIndex) {
    _phaseMgr.setProfile(profileIndex);

    // Profil değiştiğinde PID ve nem hedeflerini güncelle
    _pid.setSetpoint(_phaseMgr.getTargetTemperature());
    _humCtrl.setThresholds(_phaseMgr.getHumidityLow(), _phaseMgr.getHumidityHigh());

    Serial.print("[SYSTEM] Profil secildi: ");
    Serial.println(_phaseMgr.getCurrentProfile()->name);
}

void IncubationService::start() {
    Serial.printf("[SYSTEM] start() cagrildi, durum=%d\n", (int)_state);
    if (_state == SYS_PAUSED || _state == SYS_INITIALIZING || _state == SYS_EMERGENCY || _state == SYS_COMPLETED) {
        _safety.reset();
        _state = SYS_AUTOTUNING;
        _pid.startAutoTune();
        DateTime st = _rtc.now();
        _rtc.setStartDate(st);
        _storage.saveIncubationState(_phaseMgr.getProfileIndex(), (uint32_t)st.unixtime(), true);
        _startMillis = millis();
        Serial.println("[SYSTEM] Kulucka baslatildi");
    } else {
        Serial.printf("[SYSTEM] start() reddedildi, durum=%d\n", (int)_state);
    }
}

void IncubationService::pause() {
    Serial.printf("[SYSTEM] pause() cagrildi, durum=%d\n", (int)_state);
    if (_state == SYS_RUNNING || _state == SYS_AUTOTUNING) {
        _state = SYS_PAUSED;
        _heater.stop();
        _humidifier.turnOff();
        _fan.setPWM(FAN_MIN_PWM);
        _turner.stop();
        DateTime st = _rtc.getStartDate();
        _storage.saveIncubationState(_phaseMgr.getProfileIndex(), (uint32_t)st.unixtime(), false);
        Serial.println("[SYSTEM] Sistem duraklatildi");
    } else {
        Serial.printf("[SYSTEM] pause() reddedildi, durum=%d\n", (int)_state);
    }
}

void IncubationService::resume() {
    Serial.printf("[SYSTEM] resume() cagrildi, durum=%d\n", (int)_state);
    if (_state == SYS_PAUSED) {
        _state = SYS_RUNNING;
        DateTime st = _rtc.getStartDate();
        _storage.saveIncubationState(_phaseMgr.getProfileIndex(), (uint32_t)st.unixtime(), true);
        Serial.println("[SYSTEM] Sistem devam ediyor");
    } else {
        Serial.printf("[SYSTEM] resume() reddedildi, durum=%d\n", (int)_state);
    }
}

void IncubationService::stop() {
    Serial.printf("[SYSTEM] stop() cagrildi, durum=%d\n", (int)_state);
    _safety.reset();
    _state = SYS_PAUSED;
    _heater.stop();
    _humidifier.turnOff();
    _fan.setPWM(FAN_MIN_PWM);
    _turner.stop();
    _pid.reset();
    DateTime st = _rtc.getStartDate();
    _storage.saveIncubationState(_phaseMgr.getProfileIndex(), (uint32_t)st.unixtime(), false);
    Serial.println("[SYSTEM] Sistem durduruldu");
}

SystemStatus IncubationService::getStatus() const {
    SystemStatus s;
    s.temperature   = _sensorMgr.getTemperature();
    s.humidity      = _sensorMgr.getHumidity();
    s.targetTemp    = _phaseMgr.getTargetTemperature();
    s.targetHumLow  = _phaseMgr.getHumidityLow();
    s.targetHumHigh = _phaseMgr.getHumidityHigh();
    s.heaterPWM     = _heater.getCurrentPWM();
    s.fanPWM        = _fan.getCurrentPWM();
    s.humidifierOn  = _humidifier.isActive();
    s.currentDay    = _rtc.getElapsedDays();
    s.totalDays     = _phaseMgr.getTotalDays();
    s.remainingDays = _phaseMgr.getRemainingDays();
    s.phaseRemainingDays = _phaseMgr.getPhaseRemainingDays();
    s.phaseEndDay   = _phaseMgr.getPhaseEndDay();
    s.phaseName     = _phaseMgr.getPhaseName();
    s.profileName   = _phaseMgr.getCurrentProfile() ? _phaseMgr.getCurrentProfile()->name : "Yok";
    s.state         = _state;
    s.sensor1OK     = _sensorMgr.isSensor1OK();
    s.sensor2OK     = _sensorMgr.isSensor2OK();
    s.kp            = _pid.getKp();
    s.ki            = _pid.getKi();
    s.kd            = _pid.getKd();
    s.alarmActive   = _alarm.hasActiveAlarm();
    s.alarmMsg      = _alarm.getActiveAlarmMessage();
    s.uptime        = millis() - _startMillis;
    return s;
}

String IncubationService::getStatusJSON() const {
    SystemStatus s = getStatus();

    // RAM optimizasyonu: snprintf ile tek seferde buffer'a yaz
    char buf[640];
    String timeStr = _rtc.getFormattedTime();
    String dateStr = _rtc.getFormattedDate();

    DateTime startDt = _rtc.getStartDate();
    char startBuf[11];
    snprintf(startBuf, sizeof(startBuf), "%02d/%02d/%04d", startDt.day(), startDt.month(), startDt.year());
    DateTime hatchDt = startDt + TimeSpan((s.totalDays > 0 ? (s.totalDays - 1) : 0), 0, 0, 0);
    char hatchBuf[11];
    snprintf(hatchBuf, sizeof(hatchBuf), "%02d/%02d/%04d", hatchDt.day(), hatchDt.month(), hatchDt.year());
    int len = snprintf(buf, sizeof(buf),
        "{\"temp\":%.1f,\"hum\":%.1f,\"targetTemp\":%.1f,"
        "\"targetHumLow\":%.1f,\"targetHumHigh\":%.1f,"
        "\"heaterPWM\":%u,\"fanPWM\":%u,\"humidifier\":%s,"
        "\"day\":%d,\"totalDays\":%d,\"remainingDays\":%d,"
        "\"phase\":\"%s\",\"phaseRemaining\":%d,\"phaseEndDay\":%d,"
        "\"profile\":\"%s\","
        "\"state\":%d,\"sensor1\":%s,\"sensor2\":%s,"
        "\"kp\":%.2f,\"ki\":%.2f,\"kd\":%.2f,"
        "\"alarm\":%s,\"alarmMsg\":\"%s\",\"uptime\":%lu,"
        "\"rtcTime\":\"%s\",\"rtcDate\":\"%s\",\"startDate\":\"%s\",\"hatchDate\":\"%s\"}",
        s.temperature, s.humidity, s.targetTemp,
        s.targetHumLow, s.targetHumHigh,
        s.heaterPWM, s.fanPWM, s.humidifierOn ? "true" : "false",
        s.currentDay, s.totalDays, s.remainingDays,
        s.phaseName, s.phaseRemainingDays, s.phaseEndDay,
        s.profileName,
        (int)s.state, s.sensor1OK ? "true" : "false", s.sensor2OK ? "true" : "false",
        s.kp, s.ki, s.kd,
        s.alarmActive ? "true" : "false", s.alarmMsg.c_str(), s.uptime / 1000,
        timeStr.c_str(), dateStr.c_str(), startBuf, hatchBuf);

    return String(buf);
}

void IncubationService::setPIDParams(double kp, double ki, double kd) {
    _pid.setParameters(kp, ki, kd);
}

void IncubationService::setTargetTemp(float temp) {
    _pid.setSetpoint(temp);
}

void IncubationService::setHumidityThresholds(float low, float high) {
    _humCtrl.setThresholds(low, high);
}

void IncubationService::resetSafety() {
    _safety.reset();
    if (_state == SYS_EMERGENCY) {
        _state = SYS_PAUSED;
        Serial.println("[SYSTEM] Acil durum sifirlandi, sistem duraklatildi");
    }
}

PhaseManager& IncubationService::getPhaseManager() {
    return _phaseMgr;
}

AlarmService& IncubationService::getAlarmService() {
    return _alarm;
}
