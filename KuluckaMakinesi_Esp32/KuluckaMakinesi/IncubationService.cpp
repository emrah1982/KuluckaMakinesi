#include "IncubationService.h"
#include "StorageService.h"
#include "I2CMux.h"      // I/O saglik denetimi (bus durumu / kurtarma sayaci)
#include <WiFi.h>
#include <esp_task_wdt.h>

#include "WdtFeed.h"   // wdtFeed(): abone degilken sessiz watchdog besleme

IncubationService::IncubationService()
    : _humCtrl(_humidifier)
    , _fanCtrl(_fan)
    , _safety(_heater, _humidifier, _fan)
    , _customProfileStorage(nullptr)
    , _state(SYS_INITIALIZING)
    , _lastUpdateTime(0)
    , _startMillis(0)
    , _co2Value(400)  // Baslangic: normal atmosferik CO2 (~400 ppm)
    , _co2Valid(false)  // Gercek okuma gelene kadar gecersiz
    , _eggLastValidMs(0)
    , _eggEverValid(false)
    , _historyIndex(0)
    , _historyCount(0)
    , _lastHistoryTime(0)
    , _lastProgressSaveTime(0)
    , _humManualOverride(false)
    , _cleaningStartTime(0)
    , _cleaningPrevState(SYS_PAUSED)
    , _cleaningHeater(CLEANING_DEFAULT_HEATER)
    , _cleaningFan(CLEANING_DEFAULT_FAN)
    , _cleaningHum(CLEANING_DEFAULT_HUM)
    , _cleaningTurner(CLEANING_DEFAULT_TURNER)
    , _candlingLastDay(-1)
    , _phaseLogCount(0)
    , _nvsTimeBaseUnix(0)
    , _nvsTimeBaseMs(0)
{
    // History buffer'i sifirla
    memset(_tempHistory, 0, sizeof(_tempHistory));
    memset(_humHistory, 0, sizeof(_humHistory));
    memset(_co2History, 0, sizeof(_co2History));
    memset(&_overrideProfileBuffer, 0, sizeof(_overrideProfileBuffer));
    memset(_phaseLog, 0, sizeof(_phaseLog));
}

void IncubationService::begin() {
    DEBUG_PRINTLN("=== KULUCKA v1.0 ===");
    _eggTempSvc.begin();   // Uzak yumurta IR servisi - yedek (FreeRTOS task)
    wdtFeed();

    // HAL başlat
    bool sensorOK = _sensorMgr.begin();   // I2CMux::begin() burada cagrilir
    wdtFeed();
    // Yerel yumurta IR sensoru (MLX90614, MUX CH4) - birincil kaynak.
    // Yoksa sistem calismaya devam eder, EggTempService yedegi kullanilir.
    _eggIR.begin();
    wdtFeed();
    RelayBoard::instance().begin();
    wdtFeed();
    _heater.begin();
    _fan.begin();
    _humidifier.begin();
    _turner.begin();
    _coolSpray.begin();
    wdtFeed();
    _display.begin();   // splash animasyonu icinde WDT reset var
    wdtFeed();
    bool rtcOK = _rtc.begin();
    wdtFeed();

    // Servisler başlat
    _alarm.begin();
    _safety.begin();
    _pid.begin();
    _speaker.begin();
    _alarm.setSpeaker(&_speaker);   // alarm tetiklenince hoparlor calsin
    
    // CO2 sensoru baslat (SCD30, MUX CH3). Opsiyonel: sensor takili degilse
    // sistem calismaya devam eder, CO2 alarmlari sessizce atlanir.
    if (_co2Sensor.begin()) {
        Serial.printf("[SYS] CO2 Sensor: %s (MUX CH%d)\n",
                      _co2Sensor.getTypeName(), MUX_CH_CO2);
    } else {
        Serial.println("[SYS] CO2 sensor bulunamadi - CO2 alarmlari devre disi");
    }
    wdtFeed();

    // ---- Tani: bir sey bulunamadiysa tum kanallari tara ----
    // "BULUNAMADI" mesaji tek basina yetersiz: cihaz hic yok mu, yoksa baska
    // bir kanalda mi? Tarama bunu dogrudan gosterir ve kablo tasima
    // hatalarini aninda ortaya cikarir.
    if (!sensorOK || !_eggIR.isReady() || !RelayBoard::instance().isHealthy()) {
        Serial.println("[SYS] Eksik cihaz var - kanal taramasi yapiliyor...");
        I2CMux::scanAllChannels();
        wdtFeed();
    }

    DEBUG_PRINTLN("[SYS] Storage basliyor...");
    // NVS'den sensor kalibrasyon yukle
    _storage.begin();
    DEBUG_PRINTLN("[SYS] Storage OK");
    float tOff1, hOff1, tOff2, hOff2;
    if (_storage.loadCalibration(tOff1, hOff1, tOff2, hOff2)) {
        SensorCalibration cal = {tOff1, hOff1, tOff2, hOff2};
        _sensorMgr.setCalibration(cal);
    }

    // ===== WALL CLOCK KURTARMA (Gercek Dunya Zamani) =====
    // Hedef: cihaz acildiginda gerçek tarih/saat bilinsin diye:
    //   1. RTC oku: zaman gecerliyse (year >= 2024) → kullan, NVS'i guncelle
    //   2. RTC bozuksa NVS'den son bilinen zamani al → RTC'ye yaz + member base
    //   3. NVS de bossa: ya kullanici NTP/UI'dan set edecek, ya da derleme zamani
    {
        DateTime nowDt = _rtc.now();
        bool rtcValid = RTCManager::isValidDate(nowDt);

        // DIKKAT: isValidDate() sadece tarihin makul olup olmadigina bakar.
        // RTC donanimi olu oldugunda now() millis tabanli sahte bir saat
        // dondurur ve o tarih de "gecerli" gorunur. Bu yuzden donanim
        // sagligini AYRICA sormak gerekir; yoksa DS3231 hic yokken bile
        // "RTC OK" yazilir ve gercek ariza gizlenir.
        bool rtcHw = _rtc.isHealthy();

        if (rtcValid && rtcHw) {
            // RTC iyi durumda — NVS'i guncel zamanla seed et
            _storage.saveLastKnownTime((uint32_t)nowDt.unixtime());
            _nvsTimeBaseUnix = (uint32_t)nowDt.unixtime();
            _nvsTimeBaseMs   = millis();
            DEBUG_PRINTF("[CLOCK] RTC OK: %04u-%02u-%02u %02u:%02u, NVS guncellendi\n",
                         nowDt.year(), nowDt.month(), nowDt.day(),
                         nowDt.hour(), nowDt.minute());
        } else if (rtcValid && !rtcHw) {
            // Zaman makul ama donanim yok: millis tabanli sayim yuruyor.
            // Saat gosterilebilir ama guc kesintisinde kaybolur.
            _nvsTimeBaseUnix = (uint32_t)nowDt.unixtime();
            _nvsTimeBaseMs   = millis();
            Serial.printf("[CLOCK] UYARI: DS3231 YOK (%s). Saat millis tabanli "
                          "yurutuluyor: %04u-%02u-%02u %02u:%02u\n",
                          _rtc.getStatusString(),
                          nowDt.year(), nowDt.month(), nowDt.day(),
                          nowDt.hour(), nowDt.minute());
            Serial.println("[CLOCK] UYARI: Guc kesilirse tarih/saat kaybolur, "
                           "kulucka gun sayaci bozulabilir.");
        } else {
            // RTC bozuk — NVS'den son bilinen zamani restore et
            uint32_t lastKnown = _storage.loadLastKnownTime();
            if (lastKnown > 1700000000UL) {
                _nvsTimeBaseUnix = lastKnown;
                _nvsTimeBaseMs   = millis();
                if (_rtc.setUnixTime(lastKnown)) {
                    DateTime restored(lastKnown);
                    DEBUG_PRINTF("[CLOCK] NVS restore: %04u-%02u-%02u (RTC pili olmus olabilir)\n",
                                 restored.year(), restored.month(), restored.day());
                }
                // RTC setUnixTime basarisiz olsa bile _nvsTimeBaseUnix var → TFT'de saat gosterilir
            } else {
                DEBUG_PRINTLN("[CLOCK] UYARI: RTC bozuk + NVS bos. NTP/manuel set bekleniyor.");
            }
        }
    }

    // Varsayılan profil: Tavuk (index 0)
    setProfile(0);

    if (!sensorOK) {
        DEBUG_PRINTLN("[SYS] Sensor HATA!");
        _alarm.triggerAlarm(ALARM_SENSOR_FAIL, "Sensor hatasi");
    }

    if (!rtcOK) {
        DEBUG_PRINTLN("[SYS] RTC HATA!");
    }

    _state = SYS_INITIALIZING;
    _startMillis = millis();
    _lastUpdateTime = millis();

    DEBUG_PRINTLN("[SYS] Kurtarma kontrolu...");
    // ===== GUC KESINTISI KURTARMA (CIFT IZLEME) =====
    // BIRINCIL : RTC zaman damgasi (pil sagliysa %100 dogru)
    // IKINCIL  : NVS elapsedDays (RTC pili olse bile kurtarma saglar)
    IncubationState savedState = _storage.loadIncubationState();

    if (savedState.valid && savedState.isRunning) {
        uint32_t nowUnix  = rtcOK ? _rtc.now().unixtime() : 0;
        bool rtcTimeValid = rtcOK &&
                            (savedState.startTimestamp > 1600000000UL) &&
                            (savedState.startTimestamp <= nowUnix) &&
                            (nowUnix - savedState.startTimestamp < 86400UL * 60);

        bool nvsElapsedValid = (savedState.elapsedDays > 0 ||
                                savedState.lastSaveTimestamp > 0);

        if (rtcTimeValid || nvsElapsedValid) {
            DEBUG_PRINTLN("[SYS] GUC KESINTISI KURTARMA basliyor...");

            // Profili geri yukle (egg count'u NVS'den okumak icin setProfile ONCESI)
            if (savedState.profileIndex < PROFILE_COUNT) {
                _phaseMgr.setProfile(savedState.profileIndex);
                _pid.setSetpoint(_phaseMgr.getTargetTemperature());
                _humCtrl.setThresholds(_phaseMgr.getHumidityLow(),
                                       _phaseMgr.getHumidityHigh());
            }

            if (rtcTimeValid) {
                // BIRINCIL: RTC saglam — baslangic tarihini geri yukle
                DateTime savedStart(savedState.startTimestamp);
                _rtc.setStartDate(savedStart);
                DEBUG_PRINTF("[SYS] RTC kurtarma: start=%lu\n", savedState.startTimestamp);
            } else {
                // IKINCIL: RTC pili olmus — NVS elapsedDays'den sentetik baslangic hesapla
                // Son kaydedilen gun + son kayit ile simdiki zaman arasi (millis tabani)
                // Bu basit bir tahmin; RTC olmadan millis() kullanir.
                // millis() tabanli sahte baslangic: simdi - elapsedDays*86400 sn geri git
                // Gercek gun hesaplamasi RTCManager'da millis tabanliya aliniyor
                _rtc.setElapsedDaysFallback(savedState.elapsedDays);
                DEBUG_PRINTF("[SYS] NVS kurtarma (RTC pili olmus): gun=%d\n",
                             savedState.elapsedDays);
            }

            // PID yukle (autotune atla)
            float kp, ki, kd;
            if (_storage.loadPID(kp, ki, kd)) {
                _pid.setParameters((double)kp, (double)ki, (double)kd);
                DEBUG_PRINTLN("[SYS] PID NVS'den yuklendi");
            }

            _safety.reset();
            // Paused kaydedilmisse paused olarak don, yoksa running
            _state = savedState.isPaused ? SYS_PAUSED : SYS_RUNNING;

            int elapsedDays = _rtc.getElapsedDays();
            _alarm.triggerAlarm(ALARM_POWER_RECOVERY,
                String(savedState.isPaused ? "Duraklatilmis" : "Calisan") +
                " kulucka kurtarildi: Gun " + String(elapsedDays));
            DEBUG_PRINTF("[SYS] Kurtarma OK — Gun:%d State:%s\n",
                         elapsedDays, savedState.isPaused ? "PAUSED" : "RUNNING");
        } else {
            DEBUG_PRINTLN("[SYS] Kurtarma: RTC ve NVS gecersiz, yeni baslangic");
        }
    }

    DEBUG_PRINTLN("[SYS] Hazir");
}

void IncubationService::update() {
    // Ekran + dokunmatik her zaman calisir (kendi ic zamanlayicisi var)
    updateDisplay();

    // Fan PWM ramp/kickstart guncellemesi (her loop'ta cagrilmali)
    _fan.update();

    // Grafik history guncelle (her 5 saniyede)
    updateHistory();

    // Loop aralığı kontrolü
    unsigned long now = millis();
    if (now - _lastUpdateTime < LOOP_INTERVAL) {
        return;
    }
    _lastUpdateTime = now;

    // Yerel yumurta IR sensoru (MLX90614, MUX CH4) - kendi okuma araligi var
    _eggIR.update();

    // CO2 sensoru (SCD30, MUX CH3). Duraklatma dahil tum modlarda okunur ki
    // havalandirma sorunu kulucka duraklatilmisken de fark edilsin.
    if (_co2Sensor.isReady()) {
        if (_co2Sensor.read() && _co2Sensor.isValid()) {
            _co2Value = _co2Sensor.getCO2();
            _co2Valid = true;
        } else {
            // Okuma basarisiz: son deger korunur ama alarm uretilmez
            _co2Valid = false;
        }
    }

    // Periyodik NVS ilerleme kaydi (her 30 dakikada bir)
    // Guc kesintisinde dogru gun bilgisiyle kurtarilmak icin kritik.
    if (_state == SYS_RUNNING || _state == SYS_PAUSED) {
        const unsigned long PROGRESS_SAVE_INTERVAL = 30UL * 60UL * 1000UL; // 30 dk
        if (now - _lastProgressSaveTime >= PROGRESS_SAVE_INTERVAL) {
            _lastProgressSaveTime = now;
            _storage.updateIncubationProgress(
                (uint16_t)_rtc.getElapsedDays(),
                _rtc.now().unixtime(),
                (_state == SYS_PAUSED)
            );
        }
    }

    // ===== WALL CLOCK NVS KAYDI (her 5 dakika) =====
    // Kuluckanin durumundan bagimsiz: RTC'den okunan gercek zaman gecerliyse
    // NVS'i guncel tut → elektrik kesintisi sonra RTC pili biterse bile
    // son ~5 dakika hassasiyetle kurtarilir.
    {
        static unsigned long s_lastClockSaveMs = 0;
        const unsigned long CLOCK_SAVE_INTERVAL = 5UL * 60UL * 1000UL;   // 5 dk
        if (now - s_lastClockSaveMs >= CLOCK_SAVE_INTERVAL) {
            s_lastClockSaveMs = now;
            DateTime curr = _rtc.now();
            if (RTCManager::isValidDate(curr)) {
                _storage.saveLastKnownTime((uint32_t)curr.unixtime());
            }
        }
    }

    // Temizlik modu: PID/otomatik kontrol bypass, sadece kullanici ayarlari
    if (_state == SYS_CLEANING) {
        // Sensor oku (izleme icin — ama kontrol amacli kullanilmiyor)
        _sensorMgr.readAll();
        // Timeout kontrolu — guvenlik icin otomatik cikis
        if (now - _cleaningStartTime >= CLEANING_TIMEOUT_MS) {
            DEBUG_PRINTLN("[CLEAN] Timeout — otomatik cikis");
            stopCleaning();
            return;
        }
        // Manuel ciksilari uygula (her dongude — canli slider degisimi icin)
        _heater.setPWM(_cleaningHeater);
        _fan.setPWM(_cleaningFan);
        if (_cleaningHum) _humidifier.turnOn();
        else              _humidifier.turnOff();
        // Turner: aktifken kisa araliklar (1 dk) ile cevirme dongusu calissin
        // (driver kendi state machine'iyle hareket eder); pasifken durdur.
        if (_cleaningTurner) _turner.update(true, 1);
        else                 _turner.stop();
        _heater.update();
        return;
    }

    // Acil durum: kontrol yok ama IZLEME devam eder.
    // Eskiden burada kosulsuz return vardi; bu yuzden kapatma sonrasi
    // sicaklik izlenmiyordu. Role kontagi yapisirsa isitici calismaya devam
    // eder ve kimse fark etmezdi. Artik her dongude dogrulama yapiliyor.
    if (_state == SYS_EMERGENCY) {
        bool  readOK = _sensorMgr.readAll();
        float temp   = _sensorMgr.getTemperature();

        checkIOHealth();

        _safety.verifyShutdown(temp, readOK);
        if (_safety.isRunaway()) {
            _alarm.triggerAlarm(ALARM_THERMAL_RUNAWAY, _safety.getRunawayReason());
        }

        // Cikislari kapali tutmaya devam et. I2C bu arada toparlandiysa
        // daha once kaybolan "kapat" komutu nihayet karsi tarafa gecer.
        _heater.stop();
        _heater.update();
        _turner.stop();
        _speaker.update();   // alarm sesi acil durumda da calmali
        return;
    }

    if (_state == SYS_COMPLETED) {
        return;
    }

    // Baslangic - sadece sensor oku
    if (_state == SYS_INITIALIZING) {
        _sensorMgr.readAll();
        _turner.stop();
        return;
    }

    // Duraklatma - sensor oku + alarm kontrolu + ACIL ISIL KONTROL
    // PID aktif degil; sadece alarm durumuna gore minimal tepki.
    if (_state == SYS_PAUSED) {
        bool readOK = _sensorMgr.readAll();
        float temp = _sensorMgr.getTemperature();
        float hum  = _sensorMgr.getHumidity();
        const AnimalProfile* prof = _phaseMgr.getCurrentProfile();
        // CO2 verisi gecersizse esikleri 0 gecer -> AlarmService CO2 kontrolunu atlar.
        // Sensor yokken uydurma deger uzerinden alarm vermek yaniltici olur.
        uint16_t co2High = (_co2Valid && prof) ? prof->co2High     : (_co2Valid ? 5000 : 0);
        uint16_t co2Crit = (_co2Valid && prof) ? prof->co2Critical : (_co2Valid ? 7000 : 0);
        float tgtT  = _phaseMgr.getTargetTemperature();
        float humLo = _phaseMgr.getHumidityLow();
        float humHi = _phaseMgr.getHumidityHigh();

        // Alarm kontrol
        _alarm.checkDynamic(temp, hum, readOK,
                            tgtT, ALARM_TEMP_TOLERANCE, humLo, humHi,
                            _co2Value, co2High, co2Crit);

        // Sensor uyumsuzluk kontrolu (FUSION_DIVERGE_COUNT art arda uyumsuz okuma)
        if (_sensorMgr.getFusionStatus() == FUSION_DIVERGED &&
            _sensorMgr.getDivergeCount() >= FUSION_DIVERGE_COUNT) {
            _alarm.triggerAlarm(ALARM_SENSOR_MISMATCH,
                "Sensor uyumsuzlugu! S1:" + String(_sensorMgr.getSensor1Temp(), 1) +
                "C S2:" + String(_sensorMgr.getSensor2Temp(), 1) + "C");
        } else if (_alarm.getActiveAlarmType() == ALARM_SENSOR_MISMATCH &&
                   _sensorMgr.getFusionStatus() == FUSION_BOTH_OK) {
            _alarm.clearAlarm(ALARM_SENSOR_MISMATCH);
        }

        // Acil isil kontrol: bang-bang + histerezis (PID yok, guvenli minimal kontrol)
        // Histerezis: asiri hizli anahtalamay (relay bounce) onler.
        // ISITMA : temp < tgtT - PAUSE_HEAT_HYST  → heater ON, fan kapat
        // SOGUTMA: temp > tgtT + PAUSE_COOL_HYST  → heater OFF, fan max
        // BÖLGE  : arada            → heater OFF, fan minimum
        AlarmType activeAlarm = _alarm.getActiveAlarmType();

        const float heatOn  = tgtT - PAUSE_HEAT_HYSTERESIS;   // isitma baslangic
        const float coolOn  = tgtT + PAUSE_COOL_HYSTERESIS;   // sogutma baslangic

        if (temp < heatOn) {
            // Dusuk sicaklik: ısıtıcı ON, fan cok dusuk (~80rpm)
            // Sifir: sicak nokta olusur. Yuksek: isi kacar.
            // Dusuk sirkülasyon: homojen isi dagılımı, kayip az.
            _heater.setPWM(PAUSE_EMERGENCY_HEATER_PWM);
            _fan.setPWM(PAUSE_HEAT_FAN_PWM);
            DEBUG_PRINTF("[PAUSE] Isitma: %.1fC < %.1fC (fan cok dusuk)\n", temp, heatOn);
        } else if (temp > coolOn || activeAlarm == ALARM_TEMP_HIGH) {
            // Yuksek sicaklik veya CO2 kritik: ısıtıcı OFF, fan max
            _heater.stop();
            _fan.setPWM(255);
            DEBUG_PRINTF("[PAUSE] Sogutma: %.1fC > %.1fC\n", temp, coolOn);
        } else if (activeAlarm == ALARM_CO2_CRITICAL || activeAlarm == ALARM_CO2_HIGH) {
            // Yuksek CO2: havalandirma icin fan max, ısıtıcı kapat
            _heater.stop();
            _fan.setPWM(255);
            DEBUG_PRINTLN("[PAUSE] CO2 alarm: havalandirma fan max");
        } else {
            // Sicaklik normal bolgede: ısıtıcı OFF, minimum sirkülasyon
            _heater.stop();
            _fan.setPWM(PAUSE_IDLE_FAN_PWM);
        }

        // Nem kontrolu: profil bazli dinamik esikler — SYS_RUNNING ile AYNI mantik
        // _humCtrl icinde bang-bang zaten var; sadece esikleri profil fazindan guncelle
        _humCtrl.setThresholds(humLo, humHi);
        _humCtrl.update(hum);

        _heater.update();
        _speaker.update();
        _turner.stop();
        return;
    }

    // 1. Sensör oku
    bool readOK = _sensorMgr.readAll();
    float temp = _sensorMgr.getTemperature();
    float hum  = _sensorMgr.getHumidity();
    
    // 1b. CO2 sensoru update() basinda tum modlar icin okundu (_co2Value/_co2Valid)

    // 2. Bayat veri kontrolü - sensör uzun süredir güncellenmiyorsa
    if (_sensorMgr.isStale() && _state == SYS_RUNNING) {
        _alarm.triggerAlarm(ALARM_SENSOR_FAIL, "Sensor verisi bayat (" +
            String(SENSOR_STALE_TIMEOUT_MS / 1000) + "sn guncelleme yok)");
    }

    // 3. Güvenlik kontrolü (per-sensor fail count dahil)
    SafetyState safetyResult = _safety.check(temp, readOK, _sensorMgr.getFailCount(),
                                              _sensorMgr.getFailCount1(), _sensorMgr.getFailCount2());

    // 3b. I/O saglik kontrolu: cikislar gercekten kontrol edilebiliyor mu?
    // Roleye yazamiyorsak "kapat" komutlari bosa gidiyor demektir.
    checkIOHealth();

    if (safetyResult == SAFETY_SHUTDOWN || _safety.isShutdown()) {
        _state = SYS_EMERGENCY;
        _alarm.triggerAlarm(ALARM_SAFETY_SHUTDOWN, _safety.getShutdownReason());
        _turner.forceStop();
        _coolSpray.cancel();

        // Kapatma gercekten ise yaradi mi? Sicaklik dusmuyorsa role yapismis
        // olabilir -> termal kacis. Bu kontrol her dongude yapilmali.
        _safety.verifyShutdown(temp, readOK);
        if (_safety.isRunaway()) {
            _alarm.triggerAlarm(ALARM_THERMAL_RUNAWAY, _safety.getRunawayReason());
        }

        // Acil durumda da hoparlor state machine'i ilerlemeli, aksi halde
        // alarm sesi hic calmaz (erken return nedeniyle atlaniyordu).
        _speaker.update();

        DEBUG_PRINTLN("[SYS] ACIL!");
        return;
    }

    // 4. Alarm kontrolu (dinamik profil esikleri + CO2)
    {
        const AnimalProfile* prof = _phaseMgr.getCurrentProfile();
        // CO2 verisi gecersizse esikleri 0 gecer -> AlarmService CO2 kontrolunu atlar.
        // Sensor yokken uydurma deger uzerinden alarm vermek yaniltici olur.
        uint16_t co2High = (_co2Valid && prof) ? prof->co2High     : (_co2Valid ? 5000 : 0);
        uint16_t co2Crit = (_co2Valid && prof) ? prof->co2Critical : (_co2Valid ? 7000 : 0);
        float tgtT  = _phaseMgr.getTargetTemperature();
        float humLo = _phaseMgr.getHumidityLow();
        float humHi = _phaseMgr.getHumidityHigh();
        _alarm.checkDynamic(temp, hum, readOK,
                            tgtT, ALARM_TEMP_TOLERANCE, humLo, humHi,
                            _co2Value, co2High, co2Crit);

        // Yumurta (kabuk) sicaklik alarmlari - embriyonun gercek sicakligi
        checkEggTempAlarms(tgtT);

        // Sensor uyumsuzluk kontrolu
        if (_sensorMgr.getFusionStatus() == FUSION_DIVERGED &&
            _sensorMgr.getDivergeCount() >= FUSION_DIVERGE_COUNT) {
            _alarm.triggerAlarm(ALARM_SENSOR_MISMATCH,
                "Sensor uyumsuzlugu! S1:" + String(_sensorMgr.getSensor1Temp(), 1) +
                "C S2:" + String(_sensorMgr.getSensor2Temp(), 1) + "C");
        } else if (_alarm.getActiveAlarmType() == ALARM_SENSOR_MISMATCH &&
                   _sensorMgr.getFusionStatus() == FUSION_BOTH_OK) {
            _alarm.clearAlarm(ALARM_SENSOR_MISMATCH);
        }

        _speaker.update();    // hoparlor non-blocking state machine
    }

    // 5. Isitici role zamanlama guncelle (her loop'ta)
    _heater.update();

    // 6. Yumurta cevirme motoru (faz bazli profil parametreleriyle)
    // turningEnabled=false veya intervalMin=0 oldugunda otomatik durur.
    static bool lastTurningEnabled = true;
    bool turningEnabled = _phaseMgr.isTurningEnabled();
    uint16_t turningIntervalMin = _phaseMgr.getTurningIntervalMin();
    uint8_t  turningDurationSec = _phaseMgr.getTurningDurationSec();
    uint8_t  turningAngleDeg    = _phaseMgr.getTurningAngleDeg();
    if (lastTurningEnabled && !turningEnabled) {
        _alarm.triggerAlarm(ALARM_TURNING_STOPPED, "Cikim fazi: yumurta cevirme durduruldu");
    }
    lastTurningEnabled = turningEnabled;
    _turner.update(turningEnabled, turningIntervalMin, turningDurationSec, turningAngleDeg);

    // 6b. Gunluk sogutma & sprey (Secenek B — mevcut donanimla simule)
    DateTime tNow = _rtc.now();
    _coolSpray.update(
        tNow.hour(), tNow.minute(), tNow.day(),
        _phaseMgr.isCoolingEnabled(),
        _phaseMgr.getCoolingDurationMin(),
        _phaseMgr.getCoolingPerDay(),
        _phaseMgr.isSprayingEnabled(),
        _phaseMgr.getSprayingDurationSec()
    );

    // 7. Auto-tuning aşaması
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
            // Auto-tune tamamlandı - PID parametrelerini NVS'ye kaydet
            _state = SYS_RUNNING;
            _storage.savePID((float)_pid.getKp(), (float)_pid.getKi(), (float)_pid.getKd());
            _rtc.setStartDate(_rtc.now());

            // Kulucka durumunu NVS'ye kaydet (guc kesintisi kurtarma icin)
            _storage.saveIncubationState(
                _phaseMgr.getProfileIndex(),
                _rtc.now().unixtime(),
                true
            );
            DEBUG_PRINTLN("[SYS] Tune OK, PID ve durum NVS'ye kaydedildi");
        }
    }

    // 8. Normal çalışma
    if (_state == SYS_RUNNING) {
        updatePhase();
        updateControls();
        debugLog();
    }
}

void IncubationService::updatePhase() {
    int day = _rtc.getElapsedDays();
    const IncubationPhase* prevPhase = _phaseMgr.getCurrentPhase();
    _phaseMgr.update(day);

    if (_phaseMgr.isComplete()) {
        _state = SYS_COMPLETED;
        _heater.stop();
        _humidifier.turnOff();
        _fan.setPWM(FAN_MIN_PWM);
        _turner.forceStop();
        _coolSpray.cancel();
        _alarm.triggerAlarm(ALARM_INCUBATION_COMPLETE, "Kulucka tamamlandi!");
        DEBUG_PRINTLN("[SYS] TAMAM!");
        return;
    }

    // PID hedef sicakligini guncelle
    _pid.setSetpoint(_phaseMgr.getTargetTemperature());

    // Faz gecisi olduysa manuel override'i temizle (yeni fazin esikleri gecerli)
    if (_phaseMgr.getCurrentPhase() != prevPhase && prevPhase != nullptr) {
        clearHumidityOverride();
        DEBUG_PRINTLN("[HUM] Faz gecisi: manuel override temizlendi, profil esikleri aktif");

        // ===== FAZ GECIS ALARMI (Lockdown ozel vurgulu) =====
        const IncubationPhase* newPhase = _phaseMgr.getCurrentPhase();
        if (newPhase) {
            const char* prevName = prevPhase->phaseName ? prevPhase->phaseName : "?";
            const char* newName  = newPhase->phaseName  ? newPhase->phaseName  : "?";
            char msg[96];
            // Lockdown tespiti: yeni faz turning kapali (cevirme durduruldu)
            bool isLockdown = !newPhase->turningEnabled && prevPhase->turningEnabled;
            if (isLockdown) {
                snprintf(msg, sizeof(msg),
                         "LOCKDOWN! %s -> %s | Cevirme DURDU, nem yukseltildi",
                         prevName, newName);
            } else {
                snprintf(msg, sizeof(msg),
                         "Faz Gecisi: %s -> %s (Gun %d)",
                         prevName, newName, day);
            }
            _alarm.triggerAlarm(ALARM_PHASE_TRANSITION, msg);
            DEBUG_PRINTF("[PHASE] %s\n", msg);

            // Faz log'unu guncelle (oncekini kapat + yenisini ekle)
            recordPhaseTransition(prevPhase, newPhase, day);
        }
    }

    // Nem esiklerini guncelle — sadece manuel override yoksa
    if (!_humManualOverride) {
        _humCtrl.setThresholds(_phaseMgr.getHumidityLow(), _phaseMgr.getHumidityHigh());
    }

    // ===== CANDLING (DOL KONTROLU) BILDIRIMLERI =====
    // (1) BUGUN kontrol gunu mu? Ana alarm tetikle
    // (2) YARIN kontrol gunu mu? Erken hatirlatma (T-1 gun)
    // _candlingLastDay ile ayni gun tekrar tetiklemeleri onlenir

    // (1) Bugun kontrol gunu
    if (day != _candlingLastDay && isCandlingDay(_candlingSched, day)) {
        _candlingLastDay = day;
        const char* label = getCandlingLabel(_candlingSched, day);
        char msg[80];
        snprintf(msg, sizeof(msg), "BUGUN Dol Kontrolu: %s (Gun %d)", label, day);
        _alarm.triggerAlarm(ALARM_CANDLING_DUE, msg);
        DEBUG_PRINTF("[CANDLE] BUGUN: %s — gun %d\n", label, day);
    }

    // (2) Yarin kontrol gunu (T-1 erken hatirlatma)
    // Yarin = day+1. Eger yarin kontrol gunu ise + bugun henuz uyarmadiysak tetikle
    static int s_lastCandlingTomorrowDay = -1;
    int tomorrow = day + 1;
    if (day != s_lastCandlingTomorrowDay && isCandlingDay(_candlingSched, tomorrow)) {
        s_lastCandlingTomorrowDay = day;
        const char* label = getCandlingLabel(_candlingSched, tomorrow);
        char msg[80];
        snprintf(msg, sizeof(msg), "YARIN Dol Kontrolu: %s (Gun %d)", label, tomorrow);
        _alarm.triggerAlarm(ALARM_CANDLING_TOMORROW, msg);
        DEBUG_PRINTF("[CANDLE] YARIN hatirlatma: %s — gun %d\n", label, tomorrow);
    }
}

void IncubationService::updateControls() {
    float temp = _sensorMgr.getTemperature();
    float hum  = _sensorMgr.getHumidity();

    // PID hesaplamasi her durumda cagrilir (integral windup onlemi, auto-tune)
    uint8_t heaterPWM = _pid.compute(temp);

#if HEATER_BANG_BANG_MODE
    // --- Basit termostat (bang-bang) histerezisli ---
    // Mevcut role durumunu koruyarak histerezis bandinda tikkalemeyi onler.
    // Bu mod aktifken PID cikisi yok sayilir (sadece auto-tune icin hesaplanir).
    float setT = _pid.getSetpoint();
    bool wasOn = _heater.isActive();
    bool shouldBeOn;
    if      (temp <= setT - HEATER_HYST_LO_C) shouldBeOn = true;   // ON esigi
    else if (temp >= setT + HEATER_HYST_HI_C) shouldBeOn = false;  // OFF esigi
    else                                       shouldBeOn = wasOn; // bant ici: koru
    heaterPWM = shouldBeOn ? 255 : 0;
#endif

    // --- CoolingSprayDriver override'lari (Secenek B) ---
    if (_coolSpray.shouldOverrideHeater()) {
        // COOLING: isitici kapali, fan full
        _heater.setPWM(0);
    } else {
        _heater.setPWM(heaterPWM);
    }

    if (_coolSpray.shouldOverrideFan()) {
        // COOLING: fan full PWM (hizli sogutma)
        _fan.setPWM(255);
    } else {
        // Fan kontrolü (sıcaklığa bağlı)
        _fanCtrl.update(temp);
    }

    if (_coolSpray.shouldOverrideHumidifier()) {
        // SPRAYING: humidifier zorla ON
        _humidifier.turnOn();
    } else {
        // Normal nem kontrolü
        _humCtrl.update(hum);
    }
}

void IncubationService::updateHistory() {
    // Her 5 saniyede bir history'ye ekle
    const unsigned long HISTORY_INTERVAL = 5000;
    
    unsigned long now = millis();
    if (now - _lastHistoryTime < HISTORY_INTERVAL) {
        return;
    }
    _lastHistoryTime = now;
    
    // Mevcut sicaklik, nem ve CO2 degerlerini al
    float temp = _sensorMgr.getTemperature();
    float hum = _sensorMgr.getHumidity();
    // CO2 sensoru yoksa/okunamiyorsa 0 kaydedilir; grafikte uydurma bir
    // taban cizgi gostermek yerine "veri yok" olarak gorunur.
    uint16_t co2 = _co2Valid ? _co2Value : 0;

    // Gecersiz deger kontrolu
    if (temp < 0 || temp > 60 || hum < 0 || hum > 100) {
        return;
    }

    // FIFO mantigi - eski verileri kaydir, yeni veriyi sona ekle
    if (_historyCount >= GRAPH_HISTORY_SIZE) {
        memmove(&_tempHistory[0], &_tempHistory[1], (GRAPH_HISTORY_SIZE - 1) * sizeof(float));
        memmove(&_humHistory[0], &_humHistory[1], (GRAPH_HISTORY_SIZE - 1) * sizeof(float));
        memmove(&_co2History[0], &_co2History[1], (GRAPH_HISTORY_SIZE - 1) * sizeof(uint16_t));
        _tempHistory[GRAPH_HISTORY_SIZE - 1] = temp;
        _humHistory[GRAPH_HISTORY_SIZE - 1] = hum;
        _co2History[GRAPH_HISTORY_SIZE - 1] = co2;
    } else {
        _tempHistory[_historyCount] = temp;
        _humHistory[_historyCount] = hum;
        _co2History[_historyCount] = co2;
        _historyCount++;
    }
}

void IncubationService::debugLog() {
#if DEBUG_ENABLED
    static unsigned long lastLog = 0;
    if (millis() - lastLog < 5000) return;
    lastLog = millis();

    float temp = _sensorMgr.getTemperature();
    float hum  = _sensorMgr.getHumidity();

    Serial.printf("[STATUS] T=%.1fC H=%.1f%% Heater=%d Fan=%d Gun=%d/%d Evre=%s | Heap=%luKB\n",
        temp, hum, _heater.getCurrentPWM(), _fan.getCurrentPWM(),
        _rtc.getElapsedDays(), _phaseMgr.getTotalDays(),
        _phaseMgr.getPhaseName(), ESP.getFreeHeap() / 1024);
#endif
}

void IncubationService::updateDisplay() {
    int remainingDays = _phaseMgr.getRemainingDays();
    int phaseRemaining = _phaseMgr.getPhaseRemainingDays();
    const char* profileName = _phaseMgr.getCurrentProfile() ? _phaseMgr.getCurrentProfile()->name : "---";

    DisplayData dd;
    dd.temperature      = _sensorMgr.getTemperature();
    dd.humidity         = _sensorMgr.getHumidity();
    // Sensor yoksa 0 goster; 400 ppm "taban deger" gostermek gercek olcum
    // izlenimi verir ve havalandirma sorununu maskeler.
    dd.co2              = _co2Valid ? _co2Value : 0;
    dd.co2Valid         = _co2Valid;
    dd.targetTemp       = _phaseMgr.getTargetTemperature();
    dd.targetHumLow     = _phaseMgr.getHumidityLow();
    dd.targetHumHigh    = _phaseMgr.getHumidityHigh();
    // CO2 limitleri (profil bazli)
    const AnimalProfile* currentProf = _phaseMgr.getCurrentProfile();
    dd.co2Low           = currentProf ? currentProf->co2Low : 3000;
    dd.co2High          = currentProf ? currentProf->co2High : 5000;
    dd.co2Critical      = currentProf ? currentProf->co2Critical : 7000;
    dd.heaterPWM        = _heater.getCurrentPWM();
    dd.fanPWM           = _fan.getCurrentPWM();
    dd.humidifierOn     = _humidifier.isActive();
    dd.currentDay       = _phaseMgr.getCurrentDay();
    dd.totalDays        = _phaseMgr.getTotalDays();
    dd.remainingDays    = remainingDays;
    dd.phaseRemainingDays = phaseRemaining;
    dd.phaseName        = _phaseMgr.getPhaseName();
    dd.profileName      = profileName;
    dd.systemState      = (int)_state;
    dd.turningEnabled    = _phaseMgr.isTurningEnabled();
    dd.turningIntervalMin = _phaseMgr.getTurningIntervalMin();
    dd.sensor1OK        = _sensorMgr.isSensor1OK();
    dd.sensor2OK        = _sensorMgr.isSensor2OK();
    dd.sensor1Present   = _sensorMgr.isSensor1Present();
    dd.sensor2Present   = _sensorMgr.isSensor2Present();
    dd.uptimeSec        = (millis() - _startMillis) / 1000;
    dd.eggCount         = _storage.loadEggCount(100);
    dd.alarmActive      = _alarm.hasActiveAlarm();
    dd.alarmMsg         = _alarm.getActiveAlarmMessage();
    dd.alarmType        = _alarm.getActiveAlarmType();
    dd.alarmMuted       = _alarm.isMuted();
    dd.alarmAutoShow    = _alarm.shouldAutoShowModal();
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
    dd.profileIndex = _phaseMgr.getProfileIndex();
    
    // Grafik verisi
    dd.tempHistory  = _tempHistory;
    dd.humHistory   = _humHistory;
    dd.historyCount = _historyCount;
    dd.historyMax   = GRAPH_HISTORY_SIZE;
    // Yumurta IR sicaklik: yerel MLX90614 oncelikli, WiFi servisi yedek
    uint8_t eggSrc = EGG_SRC_NONE;
    resolveEggTemp(dd.eggTemp, dd.eggTempValid, eggSrc);
    dd.eggSensorEnabled    = _eggTempSvc.isEnabled();
    dd.eggDiscoveryStatus  = (uint8_t)_eggTempSvc.getDiscoveryStatus();

    // Temizlik modu bilgisi
    dd.cleaningActive      = isCleaning();
    dd.cleaningRemainMs    = getCleaningTimeRemainingMs();

    // Candling (dol kontrolu) bilgisi
    dd.candlingToday       = isCandlingDayToday();
    dd.candlingLabel       = getCandlingLabelToday();
    dd.candlingLockdownDay = _candlingSched.lockdownDay;

    // Baslangic tarihi: RTC'den al (her cycle'da NVS okumamak icin)
    DateTime startDt = _rtc.getStartDate();
    char startBuf[11];
    snprintf(startBuf, sizeof(startBuf), "%02d/%02d/%04d", startDt.day(), startDt.month(), startDt.year());
    dd.startDate = String(startBuf);
    dd.startDay = startDt.day();
    dd.startMonth = startDt.month();
    dd.startYear = startDt.year();

    // Bugunun tarihi/saati: RTC'den (cached read, hizli)
    // Eger RTC gecersiz tarih donuyorsa (year > 2099 veya < 2024) → NVS+millis offset ile hesapla
    DateTime nowDt = _rtc.now();
    if (!RTCManager::isValidDate(nowDt)) {
        // RTC bozuk — NVS lastKnownTime + millis offset ile hesapla
        // _nvsTimeBaseUnix begin()'de yuklendi, setSystemTime()'da guncellenir
        if (_nvsTimeBaseUnix > 1700000000UL) {
            uint32_t elapsedSec = (millis() - _nvsTimeBaseMs) / 1000UL;
            nowDt = DateTime(_nvsTimeBaseUnix + elapsedSec);
        }
    }
    dd.todayDay    = nowDt.day();
    dd.todayMonth  = nowDt.month();
    dd.todayYear   = nowDt.year();
    dd.todayHour   = nowDt.hour();
    dd.todayMinute = nowDt.minute();
    dd.todayDow    = nowDt.dayOfTheWeek();   // 0=Pazar..6=Cumartesi

    // mDNS kesfi OK durumuna gectiginde IP'yi NVS'e kalici yap
    // (TFT veya web'den tetiklenmis olabilir; tek noktadan persist)
    static uint8_t s_lastDiscStatus = 0;
    if (dd.eggDiscoveryStatus == 2 /*DISC_OK*/ && s_lastDiscStatus != 2) {
        if (_customProfileStorage) {
            String ip = _eggTempSvc.getIP();
            StoredSettings st = _customProfileStorage->loadSettings();
            if (strcmp(st.yumurtaIP, ip.c_str()) != 0) {
                strncpy(st.yumurtaIP, ip.c_str(), sizeof(st.yumurtaIP) - 1);
                st.yumurtaIP[sizeof(st.yumurtaIP) - 1] = '\0';
                _customProfileStorage->saveSettings(st);
                DEBUG_PRINTF("[EggTemp] mDNS IP NVS'e kaydedildi: %s\n", ip.c_str());
            }
        }
    }
    s_lastDiscStatus = dd.eggDiscoveryStatus;

    TouchAction action = _display.update(dd);

    switch (action) {
        case TOUCH_START:        start();        break;
        case TOUCH_PAUSE:        pause();        break;
        case TOUCH_RESUME:       resume();       break;
        case TOUCH_STOP:         stop();         break;
        case TOUCH_SAFETY_RESET: resetSafety();  break;
        case TOUCH_EGG_DEC: {
            uint16_t ec = _storage.loadEggCount(100);
            if (ec > 0) ec--;
            _storage.saveEggCount(ec);
            break;
        }
        case TOUCH_EGG_INC: {
            uint16_t ec = _storage.loadEggCount(100);
            if (ec < 9999) ec++;
            _storage.saveEggCount(ec);
            break;
        }
        case TOUCH_EGG_DISCOVER: {
            // mDNS ile yumurta.local kesfini baslat (asenkron, hemen doner)
            _eggTempSvc.requestDiscovery();
            DEBUG_PRINTLN("[EggTemp] TFT: mDNS kesfi tetiklendi");
            break;
        }
        case TOUCH_CLEANING_TOGGLE: {
            // Aktifse durdur, degilse basla
            DEBUG_PRINTF("[CLEAN] TFT buton — mevcut state=%d cleaning=%d\n",
                         (int)_state, isCleaning() ? 1 : 0);
            if (isCleaning()) {
                stopCleaning();
            } else {
                bool ok = startCleaning();
                DEBUG_PRINTF("[CLEAN] startCleaning sonuc: %s\n", ok ? "OK" : "REDDEDILDI");
            }
            break;
        }
        case TOUCH_EGG_SENSOR_TOGGLE: {
            // IR Yumurta servisini aktif/pasif arasinda gec ve NVS'e kaydet
            bool newState = !_eggTempSvc.isEnabled();
            _eggTempSvc.setEnabled(newState);
            // StoredSettings StorageService'te tutulur (PersistentStorage degil)
            if (_customProfileStorage) {
                StoredSettings st = _customProfileStorage->loadSettings();
                st.yumurtaEnabled = newState;
                _customProfileStorage->saveSettings(st);
            }
            break;
        }
        case TOUCH_PROFILE_SELECT: {
            uint8_t idx = _display.getSelectedProfileIdx();
            if (idx < PROFILE_COUNT) {
                setProfile(idx);
                DEBUG_PRINTF("[P] %s\n", ALL_PROFILES[idx]->name);
            }
            break;
        }
        // ---- Profil Override Editor (TFT, KONTROL tab) ----
        case TOUCH_EDIT_TEMP_DEC:    adjustActivePhaseTemp(-0.1f);   break;
        case TOUCH_EDIT_TEMP_INC:    adjustActivePhaseTemp(+0.1f);   break;
        case TOUCH_EDIT_HUMLOW_DEC:  adjustActivePhaseHumLow(-1.0f); break;
        case TOUCH_EDIT_HUMLOW_INC:  adjustActivePhaseHumLow(+1.0f); break;
        case TOUCH_EDIT_HUMHIGH_DEC: adjustActivePhaseHumHigh(-1.0f);break;
        case TOUCH_EDIT_HUMHIGH_INC: adjustActivePhaseHumHigh(+1.0f);break;
        case TOUCH_EDIT_TURNINT_DEC: adjustActivePhaseTurningInterval(-5); break;
        case TOUCH_EDIT_TURNINT_INC: adjustActivePhaseTurningInterval(+5); break;
        case TOUCH_EDIT_RESET_FACTORY:
            if (resetActiveProfileOverride()) {
                DEBUG_PRINTLN("[EDIT] Aktif profil fabrika ayarlarina dondu");
            }
            break;
        case TOUCH_ALARM_ACK:
            acknowledgeAlarm();
            break;
        case TOUCH_ALARM_SNOOZE:
            snoozeActiveAlarm();
            break;
        case TOUCH_ALARM_DISMISS:
            dismissActiveAlarm();
            break;
        default: break;
    }

    // LED rolesi kaldirildi - PCF8574 P3 artik Turner yon icin kullaniliyor
}

void IncubationService::setProfile(uint8_t profileIndex) {
    // Override kontrol: kullanici bu hazir profili duzenlemis mi?
    bool usedOverride = false;
    if (_customProfileStorage &&
        _customProfileStorage->hasProfileOverride(profileIndex)) {
        if (_customProfileStorage->getOverridenProfile(profileIndex,
                                                      _overrideProfileBuffer)) {
            _phaseMgr.setProfile(&_overrideProfileBuffer, profileIndex);
            usedOverride = true;
            DEBUG_PRINTF("[P] Override aktif: %s\n", _overrideProfileBuffer.name);
        }
    }
    if (!usedOverride) {
        _phaseMgr.setProfile(profileIndex);
    }

    // Profil degistiginde PID ve nem hedeflerini guncelle
    _pid.setSetpoint(_phaseMgr.getTargetTemperature());
    _humCtrl.setThresholds(_phaseMgr.getHumidityLow(), _phaseMgr.getHumidityHigh());
    clearHumidityOverride();  // Profil degisince manuel override silinir

    // Profil değiştiğinde yumurta sayısını varsayılan değere ayarla
    const AnimalProfile* prof = _phaseMgr.getCurrentProfile();
    if (prof) {
        _storage.saveEggCount(prof->defaultEggCount);
        DEBUG_PRINTF("[P] %s (Yumurta: %d)\n", prof->name, prof->defaultEggCount);
    }

    // Candling (dol kontrolu) programini guncelle
    updateCandlingSchedule();
}

void IncubationService::setCustomProfileStorage(StorageService* s) {
    _customProfileStorage = s;
    // DisplayManager'a da bagla — profil listesinde override rozeti cizilebilsin
    _display.setCustomProfileStorage(s);
}

StorageService* IncubationService::getCustomProfileStorage() const {
    return _customProfileStorage;
}

// ---------- Profil Override editor yardimci fonksiyonlari ----------

namespace {
    // Aktif fazin CustomProfile.phases dizisindeki indeksini bulur.
    // Hata durumda -1 doner.
    int findActivePhaseIdxInCustom(const CustomProfile &cp, const IncubationPhase* curPhase) {
        if (!curPhase) return -1;
        // En guvenli yol: gun araligina gore eslestirme (pointer kiyaslamasi olmaz,
        // cunku CustomProfile farkli bir bellek alaninda yasiyor)
        for (uint8_t i = 0; i < cp.phaseCount && i < 4; i++) {
            if (cp.phases[i].startDay == curPhase->startDay &&
                cp.phases[i].endDay   == curPhase->endDay) {
                return (int)i;
            }
        }
        // Esleme bulunamazsa ilk fazi don
        return cp.phaseCount > 0 ? 0 : -1;
    }
}

bool IncubationService::adjustActivePhaseTemp(float delta) {
    if (!_customProfileStorage) return false;
    uint8_t profIdx = _phaseMgr.getProfileIndex();
    const IncubationPhase* curPhase = _phaseMgr.getCurrentPhase();
    if (!curPhase) return false;

    // Override yoksa once klonla
    if (!_customProfileStorage->hasProfileOverride(profIdx)) {
        if (!_customProfileStorage->cloneProfileToOverride(profIdx)) {
            DEBUG_PRINTLN("[EDIT] Override olusturulamadi (slot dolu)");
            return false;
        }
    }
    int8_t cidx = _customProfileStorage->getOverrideCustomIdx(profIdx);
    if (cidx < 0) return false;

    CustomProfile cp = _customProfileStorage->getCustomProfile((uint8_t)cidx);
    int phIdx = findActivePhaseIdxInCustom(cp, curPhase);
    if (phIdx < 0) return false;

    // Sicakligi delta kadar kaydir; gradyan varsa tempEnd'e de uygula
    cp.phases[phIdx].temperature += delta;
    if (cp.phases[phIdx].tempEnd > 0.0f) {
        cp.phases[phIdx].tempEnd += delta;
    }
    // Sinir kontrol
    if (cp.phases[phIdx].temperature < 20.0f) cp.phases[phIdx].temperature = 20.0f;
    if (cp.phases[phIdx].temperature > 45.0f) cp.phases[phIdx].temperature = 45.0f;
    if (cp.phases[phIdx].tempEnd > 0.0f) {
        if (cp.phases[phIdx].tempEnd < 20.0f) cp.phases[phIdx].tempEnd = 20.0f;
        if (cp.phases[phIdx].tempEnd > 45.0f) cp.phases[phIdx].tempEnd = 45.0f;
    }

    _customProfileStorage->updateCustomProfile((uint8_t)cidx, cp);
    reloadActiveProfile();  // egg count'a dokunmaz, sadece override'i yeniden yukler
    return true;
}

bool IncubationService::adjustActivePhaseHumLow(float delta) {
    if (!_customProfileStorage) return false;
    uint8_t profIdx = _phaseMgr.getProfileIndex();
    const IncubationPhase* curPhase = _phaseMgr.getCurrentPhase();
    if (!curPhase) return false;

    if (!_customProfileStorage->hasProfileOverride(profIdx)) {
        if (!_customProfileStorage->cloneProfileToOverride(profIdx)) return false;
    }
    int8_t cidx = _customProfileStorage->getOverrideCustomIdx(profIdx);
    if (cidx < 0) return false;

    CustomProfile cp = _customProfileStorage->getCustomProfile((uint8_t)cidx);
    int phIdx = findActivePhaseIdxInCustom(cp, curPhase);
    if (phIdx < 0) return false;

    cp.phases[phIdx].humidityLow += delta;
    if (cp.phases[phIdx].humidityLow < 10.0f) cp.phases[phIdx].humidityLow = 10.0f;
    if (cp.phases[phIdx].humidityLow > 95.0f) cp.phases[phIdx].humidityLow = 95.0f;
    // humLow > humHigh - MIN_GAP olmasini engelle (HumidityController::setThresholds ile ayni minimum)
    if (cp.phases[phIdx].humidityLow > cp.phases[phIdx].humidityHigh - 2.0f) {
        cp.phases[phIdx].humidityLow = cp.phases[phIdx].humidityHigh - 2.0f;
    }

    _customProfileStorage->updateCustomProfile((uint8_t)cidx, cp);
    reloadActiveProfile();
    return true;
}

bool IncubationService::adjustActivePhaseHumHigh(float delta) {
    if (!_customProfileStorage) return false;
    uint8_t profIdx = _phaseMgr.getProfileIndex();
    const IncubationPhase* curPhase = _phaseMgr.getCurrentPhase();
    if (!curPhase) return false;

    if (!_customProfileStorage->hasProfileOverride(profIdx)) {
        if (!_customProfileStorage->cloneProfileToOverride(profIdx)) return false;
    }
    int8_t cidx = _customProfileStorage->getOverrideCustomIdx(profIdx);
    if (cidx < 0) return false;

    CustomProfile cp = _customProfileStorage->getCustomProfile((uint8_t)cidx);
    int phIdx = findActivePhaseIdxInCustom(cp, curPhase);
    if (phIdx < 0) return false;

    cp.phases[phIdx].humidityHigh += delta;
    if (cp.phases[phIdx].humidityHigh < 10.0f) cp.phases[phIdx].humidityHigh = 10.0f;
    if (cp.phases[phIdx].humidityHigh > 95.0f) cp.phases[phIdx].humidityHigh = 95.0f;
    // humHigh < humLow + MIN_GAP olmasini engelle
    if (cp.phases[phIdx].humidityHigh < cp.phases[phIdx].humidityLow + 2.0f) {
        cp.phases[phIdx].humidityHigh = cp.phases[phIdx].humidityLow + 2.0f;
    }

    _customProfileStorage->updateCustomProfile((uint8_t)cidx, cp);
    reloadActiveProfile();
    return true;
}

bool IncubationService::adjustActivePhaseTurningInterval(int16_t deltaMin) {
    if (!_customProfileStorage) return false;
    uint8_t profIdx = _phaseMgr.getProfileIndex();
    const IncubationPhase* curPhase = _phaseMgr.getCurrentPhase();
    if (!curPhase) return false;

    if (!_customProfileStorage->hasProfileOverride(profIdx)) {
        if (!_customProfileStorage->cloneProfileToOverride(profIdx)) return false;
    }
    int8_t cidx = _customProfileStorage->getOverrideCustomIdx(profIdx);
    if (cidx < 0) return false;

    CustomProfile cp = _customProfileStorage->getCustomProfile((uint8_t)cidx);
    int phIdx = findActivePhaseIdxInCustom(cp, curPhase);
    if (phIdx < 0) return false;

    // Mevcut interval + delta, sinir kontrol
    int32_t cur = (int32_t)cp.phases[phIdx].turningIntervalMin;
    int32_t next = cur + (int32_t)deltaMin;
    if (next < 0) next = 0;
    if (next > 600) next = 600;     // max 10 saat

    cp.phases[phIdx].turningIntervalMin = (uint16_t)next;
    // 0 set edilirse turning otomatik DISABLE
    cp.phases[phIdx].turningEnabled = (next > 0);

    _customProfileStorage->updateCustomProfile((uint8_t)cidx, cp);
    reloadActiveProfile();
    return true;
}

bool IncubationService::resetActiveProfileOverride() {
    if (!_customProfileStorage) return false;
    uint8_t profIdx = _phaseMgr.getProfileIndex();
    if (!_customProfileStorage->hasProfileOverride(profIdx)) return false;
    bool ok = _customProfileStorage->clearProfileOverride(profIdx);
    if (ok) reloadActiveProfile();  // override yok, hazir profile geri don
    return ok;
}

void IncubationService::reloadActiveProfile() {
    uint8_t profIdx = _phaseMgr.getProfileIndex();
    bool used = false;
    if (_customProfileStorage && _customProfileStorage->hasProfileOverride(profIdx)) {
        if (_customProfileStorage->getOverridenProfile(profIdx, _overrideProfileBuffer)) {
            _phaseMgr.setProfile(&_overrideProfileBuffer, profIdx);
            used = true;
        }
    }
    if (!used) {
        _phaseMgr.setProfile(profIdx);  // Hazir profil (egg count icin setProfile() degil!)
    }
    _pid.setSetpoint(_phaseMgr.getTargetTemperature());
    _humCtrl.setThresholds(_phaseMgr.getHumidityLow(), _phaseMgr.getHumidityHigh());
    clearHumidityOverride();  // Profil yeniden yuklendi, web override gecersiz
}

void IncubationService::start() {
    if (_state == SYS_CLEANING) stopCleaning();   // Once temizligi bitir
    if (_state == SYS_PAUSED || _state == SYS_INITIALIZING || _state == SYS_EMERGENCY || _state == SYS_COMPLETED) {
        _safety.reset();
        _state = SYS_AUTOTUNING;
        _pid.startAutoTune();
        _rtc.setStartDate(_rtc.now());
        _startMillis = millis();

        // NVS'ye kaydet (guc kesintisi kurtarma icin)
        _storage.saveIncubationState(
            _phaseMgr.getProfileIndex(),
            _rtc.now().unixtime(),
            true
        );

        // Faz log'u sifirla + ilk fazi kaydet
        _phaseLogCount = 0;
        const IncubationPhase* firstPhase = _phaseMgr.getCurrentPhase();
        if (firstPhase) {
            recordPhaseTransition(nullptr, firstPhase, 1);
        }
        DEBUG_PRINTLN("[SYS] Start");
    }
}

void IncubationService::pause() {
    if (_state == SYS_CLEANING) return;   // Temizlikten cikmak icin DURDUR/TEMIZLIK kullan
    if (_state == SYS_RUNNING || _state == SYS_AUTOTUNING) {
        _state = SYS_PAUSED;
        _heater.stop();
        _humidifier.turnOff();
        _fan.setPWM(FAN_MIN_PWM);
        _turner.stop();
        _coolSpray.cancel();
        // Pause anini NVS'ye kaydet — guc kesintisinde PAUSED olarak kurtarilir
        _storage.updateIncubationProgress(
            (uint16_t)_rtc.getElapsedDays(),
            _rtc.now().unixtime(),
            true  // isPaused
        );
        DEBUG_PRINTLN("[SYS] Pause (NVS guncellendi)");
    }
}

void IncubationService::resume() {
    if (_state == SYS_PAUSED) {
        _state = SYS_RUNNING;
        // Resume anini NVS'ye kaydet
        _storage.updateIncubationProgress(
            (uint16_t)_rtc.getElapsedDays(),
            _rtc.now().unixtime(),
            false  // isPaused
        );
        DEBUG_PRINTLN("[SYS] Resume (NVS guncellendi)");
    }
}

void IncubationService::stop() {
    if (_state == SYS_CLEANING) {
        // Temizlikteyken DURDUR'a basildiysa: temizligi sonlandir, normal pause'a gec
        stopCleaning();
        return;   // stopCleaning zaten state'i geri yukledi
    }
    _safety.reset();
    _state = SYS_PAUSED;
    _heater.stop();
    _humidifier.turnOff();
    _fan.setPWM(FAN_MIN_PWM);
    _turner.stop();
    _coolSpray.cancel();
    _pid.reset();

    // NVS'deki kulucka durumunu temizle
    _storage.clearIncubationState();
    DEBUG_PRINTLN("[SYS] Stop");
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
    resolveEggTemp(s.eggTemp, s.eggTempValid, s.eggTempSource);
    s.eggSensorIP   = _eggTempSvc.getIP();
    return s;
}

// ---------------------------------------------------------------------
//  I/O saglik denetimi
//
//  Kritik varsayim: "role kapat" komutu her zaman ise yarar. Gercekte I2C
//  hattinda sorun olursa PCF8574 son cikis durumunu korur ve isitici ACIK
//  kalir. RelayBoard artik her yazmanin sonucunu izliyor; burada o bilgiyi
//  alarma ve guvenlik servisine bagliyoruz.
// ---------------------------------------------------------------------
void IncubationService::checkIOHealth() {
    RelayBoard& rb = RelayBoard::instance();

    if (rb.getWriteFailCount() >= RELAY_WRITE_FAIL_LIMIT) {
        String msg = "Role kartina yazilamiyor (ardisik " +
                     String(rb.getWriteFailCount()) + " hata, toplam " +
                     String(rb.getTotalWriteFails()) + ")";
        _alarm.triggerAlarm(ALARM_IO_FAIL, msg);
        // Cikislar kontrol edilemiyor: isitici son durumunda takili olabilir.
        // Guvenlik servisi kacis moduna gecer (fan tam guc + surekli alarm).
        _safety.reportIOFailure(msg);
        _state = SYS_EMERGENCY;
        return;
    }

    if (!I2CMux::isReady()) {
        _alarm.triggerAlarm(ALARM_IO_FAIL,
            "I2C multiplexer yanit vermiyor (kurtarma denemesi: " +
            String(I2CMux::getRecoverCount()) + ")");
        return;
    }

    // Bus toparlandiysa alarmi temizle
    if (_alarm.getActiveAlarmType() == ALARM_IO_FAIL && rb.isHealthy()) {
        _alarm.clearAlarm(ALARM_IO_FAIL);
    }
}

// ---------------------------------------------------------------------
//  Yumurta (kabuk) sicaklik alarmlari
//
//  Kabuk sicakligi embriyonun gercek sicakligina en yakin olcumdur; ortam
//  sicakligi bunun sadece dolayli gostergesidir. Asiri isinma embriyoyu
//  saatler icinde oldurur, bu yuzden ust sinir kritiktir.
//
//  Not: Kulucka'nin ilk gunlerinde embriyo isi uretmedigi icin kabuk
//  sicakligi hedefin altinda seyreder; son gunlerde metabolik isi ile
//  hedefin ustune cikar. Alt sinir toleransi bu yuzden genis tutuldu.
// ---------------------------------------------------------------------
void IncubationService::checkEggTempAlarms(float targetTemp) {
#if EGG_TEMP_ALARM_ENABLED
    float   eggT   = 0.0f;
    bool    valid  = false;
    uint8_t source = EGG_SRC_NONE;
    resolveEggTemp(eggT, valid, source);

    unsigned long now = millis();

    if (valid) {
        _eggLastValidMs = now;
        _eggEverValid   = true;
        _alarm.clearAlarm(ALARM_EGG_SENSOR_LOST);
    } else {
        // Kaynak kaybi: yalnizca daha once calisiyorduysa alarm ver.
        // Hic takilmamis bir sensor icin surekli alarm anlamsiz olurdu.
        if (_eggEverValid && (now - _eggLastValidMs) >= EGG_SOURCE_LOST_MS) {
            _alarm.triggerAlarm(ALARM_EGG_SENSOR_LOST,
                "Yumurta IR kaynagi yok (" +
                String((now - _eggLastValidMs) / 1000) + "sn)");
        }
        return;   // olcum yoksa sicaklik alarmi da uretilemez
    }

    // Cok dusuk degerler kapak acikligi / yeni yerlestirilmis yumurta
    // olabilir; bu araligi alarm disi birakiyoruz.
    if (eggT < EGG_TEMP_ALARM_MIN) {
        _alarm.clearAlarm(ALARM_EGG_TEMP_HIGH);
        _alarm.clearAlarm(ALARM_EGG_TEMP_LOW);
        return;
    }

    const char* srcName = (source == EGG_SRC_LOCAL) ? "yerel" : "uzak";

    if (eggT > targetTemp + EGG_TEMP_TOLERANCE_HIGH) {
        _alarm.triggerAlarm(ALARM_EGG_TEMP_HIGH,
            "Yumurta sicak: " + String(eggT, 1) + "C (hedef " +
            String(targetTemp, 1) + "C, " + srcName + ")");
    } else if (eggT < targetTemp - EGG_TEMP_TOLERANCE_LOW) {
        _alarm.triggerAlarm(ALARM_EGG_TEMP_LOW,
            "Yumurta soguk: " + String(eggT, 1) + "C (hedef " +
            String(targetTemp, 1) + "C, " + srcName + ")");
    } else {
        _alarm.clearAlarm(ALARM_EGG_TEMP_HIGH);
        _alarm.clearAlarm(ALARM_EGG_TEMP_LOW);
    }
#else
    (void)targetTemp;
#endif
}

// ---------------------------------------------------------------------
//  Yumurta sicakligi kaynak secimi
//  1) Yerel MLX90614 (MUX CH4)  -> birincil
//  2) EggTempService (WiFi/HTTP) -> yedek
//  Ikisi de gecersizse valid=false, kaynak EGG_SRC_NONE.
// ---------------------------------------------------------------------
void IncubationService::resolveEggTemp(float &temp, bool &valid,
                                       uint8_t &source) const {
    if (_eggIR.isReady() && _eggIR.isValid()) {
        temp   = _eggIR.getObjectTemp();
        valid  = true;
        source = EGG_SRC_LOCAL;
        return;
    }

    if (_eggTempSvc.isEnabled() && _eggTempSvc.isValid()) {
        temp   = _eggTempSvc.getEggTemp();
        valid  = true;
        source = EGG_SRC_REMOTE;
        return;
    }

    temp   = 0.0f;
    valid  = false;
    source = EGG_SRC_NONE;
}

uint32_t IncubationService::getUnixTime() const {
    return (uint32_t)_rtc.now().unixtime();
}

bool IncubationService::setSystemTime(uint32_t unixSec) {
    if (unixSec < 1700000000UL) {
        Serial.printf("[CLOCK] setSystemTime: gecersiz unix=%u (< 2023)\n", unixSec);
        return false;   // 2023-Kasim oncesi gecersiz
    }
    // NVS'e HER DURUMDA kaydet — RTC bozuk olsa bile saat kalici olarak korunur
    _storage.saveLastKnownTime(unixSec);

    // Member base'i guncelle — updateDisplay() bunu kullanir (RTC bozuksa NVS+millis fallback)
    _nvsTimeBaseUnix = unixSec;
    _nvsTimeBaseMs   = millis();
    Serial.printf("[CLOCK] NVS saved + base updated: unix=%u\n", unixSec);

    // RTC'ye yazmaya CALIS — basarisiz olsa bile false donmeyiz
    // (kullaniciya 'set basarili' bildirilir, NVS'de zaten saklanmis)
    bool rtcOk = _rtc.setUnixTime(unixSec);
    if (rtcOk) {
        Serial.printf("[CLOCK] RTC adjusted: unix=%u\n", unixSec);
    } else {
        Serial.println("[CLOCK] UYARI: RTC yazilamadi (modul bozuk?), NVS+millis fallback aktif");
    }
    return true;   // NVS basariliysa OK doneriz
}

// Gecmis veri (sicaklik/nem/CO2) JSON formatinda dondur
// Format: { "interval": 5, "count": N, "temp": [...], "hum": [...], "co2": [...] }
// interval = saniye cinsinden okuma araligi (DB'ye eklerken timestamp hesaplamak icin)
String IncubationService::getHistoryJSON() const {
    // 60 deger * (~6 char + virgul) ≈ 500 byte/dizi, 3 dizi + meta ≈ 1.7KB
    String out;
    out.reserve(2048);
    out += "{\"interval\":5,\"count\":";
    out += String(_historyCount);
    out += ",\"unixTime\":";
    out += String((uint32_t)_rtc.now().unixtime());
    out += ",\"temp\":[";
    for (uint16_t i = 0; i < _historyCount; i++) {
        if (i > 0) out += ",";
        out += String(_tempHistory[i], 1);
    }
    out += "],\"hum\":[";
    for (uint16_t i = 0; i < _historyCount; i++) {
        if (i > 0) out += ",";
        out += String(_humHistory[i], 1);
    }
    out += "],\"co2\":[";
    for (uint16_t i = 0; i < _historyCount; i++) {
        if (i > 0) out += ",";
        out += String(_co2History[i]);
    }
    out += "]}";
    return out;
}

// ==================== FAZ GECIS LOG ====================

void IncubationService::recordPhaseTransition(const IncubationPhase* prev,
                                               const IncubationPhase* next, int day) {
    uint32_t nowUnix = (uint32_t)_rtc.now().unixtime();

    // 1) Onceki entry'i kapat (varsa)
    if (prev != nullptr && _phaseLogCount > 0) {
        // Son entry oncekine ait olmali — onu kapat
        PhaseLogEntry &last = _phaseLog[_phaseLogCount - 1];
        if (last.endUnix == 0) {
            last.endUnix = nowUnix;
            last.endDay  = (uint16_t)day;
        }
    }

    // 2) Yeni entry ekle (yer varsa)
    if (next != nullptr && _phaseLogCount < MAX_PHASE_LOG_ENTRIES) {
        PhaseLogEntry &entry = _phaseLog[_phaseLogCount];
        entry.phaseName = next->phaseName ? next->phaseName : "?";
        entry.startUnix = nowUnix;
        entry.endUnix   = 0;       // halen aktif
        entry.startDay  = (uint16_t)day;
        entry.endDay    = 0;
        _phaseLogCount++;
        DEBUG_PRINTF("[PHASE-LOG] +%s (Gun %d) — toplam %u entry\n",
                     entry.phaseName, day, _phaseLogCount);
    } else if (_phaseLogCount >= MAX_PHASE_LOG_ENTRIES) {
        DEBUG_PRINTLN("[PHASE-LOG] UYARI: Buffer dolu, yeni entry eklenmedi");
    }
}

// Format: { "count":N, "entries":[{"name":"...", "startUnix":..., "endUnix":...,
//          "startDay":..., "endDay":..., "durationSec":..., "active":bool}, ...] }
String IncubationService::getPhaseLogJSON() const {
    String out;
    out.reserve(512);
    out += "{\"count\":";
    out += String(_phaseLogCount);
    out += ",\"entries\":[";
    uint32_t nowUnix = (uint32_t)_rtc.now().unixtime();
    for (uint8_t i = 0; i < _phaseLogCount; i++) {
        const PhaseLogEntry &e = _phaseLog[i];
        if (i > 0) out += ",";
        bool active = (e.endUnix == 0);
        uint32_t durationSec = active ? (nowUnix - e.startUnix)
                                      : (e.endUnix - e.startUnix);
        out += "{\"name\":\"";
        out += (e.phaseName ? e.phaseName : "?");
        out += "\",\"startUnix\":";
        out += String(e.startUnix);
        out += ",\"endUnix\":";
        out += String(e.endUnix);
        out += ",\"startDay\":";
        out += String(e.startDay);
        out += ",\"endDay\":";
        out += String(e.endDay);
        out += ",\"durationSec\":";
        out += String(durationSec);
        out += ",\"active\":";
        out += (active ? "true" : "false");
        out += "}";
    }
    out += "]}";
    return out;
}

String IncubationService::getStatusJSON() const {
    SystemStatus s = getStatus();

    // RAM optimizasyonu: snprintf ile tek seferde buffer'a yaz
    // 1000: eggTempSource + io{} tani alanlari eklendikten sonraki pay
    char buf[1000];
    String timeStr = _rtc.getFormattedTime();
    String dateStr = _rtc.getFormattedDate();

    DateTime startDt = _rtc.getStartDate();
    char startBuf[11];
    snprintf(startBuf, sizeof(startBuf), "%02d/%02d/%04d", startDt.day(), startDt.month(), startDt.year());
    DateTime hatchDt = startDt + TimeSpan((s.totalDays > 0 ? (s.totalDays - 1) : 0), 0, 0, 0);
    char hatchBuf[11];
    snprintf(hatchBuf, sizeof(hatchBuf), "%02d/%02d/%04d", hatchDt.day(), hatchDt.month(), hatchDt.year());
    bool turningEnabled = _phaseMgr.isTurningEnabled();
    snprintf(buf, sizeof(buf),
        "{\"temp\":%.1f,\"hum\":%.1f,\"targetTemp\":%.1f,"
        "\"targetHumLow\":%.1f,\"targetHumHigh\":%.1f,"
        "\"heaterPWM\":%u,\"fanPWM\":%u,\"humidifier\":%s,"
        "\"day\":%d,\"totalDays\":%d,\"remainingDays\":%d,"
        "\"phase\":\"%s\",\"phaseRemaining\":%d,\"phaseEndDay\":%d,"
        "\"profile\":\"%s\","
        "\"state\":%d,\"sensor1\":%s,\"sensor2\":%s,"
        "\"kp\":%.2f,\"ki\":%.2f,\"kd\":%.2f,"
        "\"alarm\":%s,\"alarmMsg\":\"%s\",\"alarmType\":%d,\"alarmMuted\":%s,\"alarmAutoShow\":%s,\"uptime\":%lu,"
        "\"rtcTime\":\"%s\",\"rtcDate\":\"%s\",\"startDate\":\"%s\",\"hatchDate\":\"%s\","
        "\"turningEnabled\":%s,"
        "\"eggTemp\":%.2f,\"eggTempValid\":%s,\"eggTempSource\":%u,"
        "\"eggIRLocalOK\":%s,\"eggIRAmbient\":%.1f,"
        "\"eggSensorIP\":\"%s\",\"eggSensorEnabled\":%s,"
        "\"io\":{\"relayOK\":%s,\"relayFails\":%lu,\"muxOK\":%s,\"muxRecover\":%lu,\"runaway\":%s},"
        "\"cleaning\":{\"active\":%s,\"heater\":%u,\"fan\":%u,\"hum\":%s,\"turner\":%s,\"remainMs\":%lu},"
        "\"candling\":{\"isToday\":%s,\"label\":\"%s\",\"lockdownDay\":%u,\"nextCandlingDay\":%u}}",
        s.temperature, s.humidity, s.targetTemp,
        s.targetHumLow, s.targetHumHigh,
        s.heaterPWM, s.fanPWM, s.humidifierOn ? "true" : "false",
        s.currentDay, s.totalDays, s.remainingDays,
        s.phaseName, s.phaseRemainingDays, s.phaseEndDay,
        s.profileName,
        (int)s.state, s.sensor1OK ? "true" : "false", s.sensor2OK ? "true" : "false",
        s.kp, s.ki, s.kd,
        s.alarmActive ? "true" : "false", s.alarmMsg.c_str(),
        (int)_alarm.getActiveAlarmType(),
        _alarm.isMuted() ? "true" : "false",
        _alarm.shouldAutoShowModal() ? "true" : "false",
        s.uptime / 1000,
        timeStr.c_str(), dateStr.c_str(), startBuf, hatchBuf,
        turningEnabled ? "true" : "false",
        s.eggTemp, s.eggTempValid ? "true" : "false",
        (unsigned)s.eggTempSource,
        (_eggIR.isReady() && _eggIR.isValid()) ? "true" : "false",
        _eggIR.getAmbientTemp(),
        s.eggSensorIP.c_str(),
        _eggTempSvc.isEnabled() ? "true" : "false",
        RelayBoard::instance().isHealthy() ? "true" : "false",
        (unsigned long)RelayBoard::instance().getTotalWriteFails(),
        I2CMux::isReady() ? "true" : "false",
        (unsigned long)I2CMux::getRecoverCount(),
        _safety.isRunaway() ? "true" : "false",
        isCleaning() ? "true" : "false",
        _cleaningHeater, _cleaningFan,
        _cleaningHum    ? "true" : "false",
        _cleaningTurner ? "true" : "false",
        getCleaningTimeRemainingMs(),
        isCandlingDayToday() ? "true" : "false",
        getCandlingLabelToday() ? getCandlingLabelToday() : "",
        _candlingSched.lockdownDay,
        nextCandlingDay(_candlingSched, s.currentDay));

    return String(buf);
}

void IncubationService::setPIDParams(double kp, double ki, double kd) {
    _pid.setParameters(kp, ki, kd);
}

void IncubationService::setTargetTemp(float temp) {
    _pid.setSetpoint(temp);
}

void IncubationService::setHumidityThresholds(float low, float high) {
    applyHumidityOverride(low, high);
}

void IncubationService::applyHumidityOverride(float low, float high) {
    _humCtrl.setThresholds(low, high);  // setThresholds icinde validasyon var
    _humManualOverride = true;
    DEBUG_PRINTF("[HUM] Manuel override aktif: %.0f%%-%.0f%%\n",
                 _humCtrl.getLowThreshold(), _humCtrl.getHighThreshold());
}

void IncubationService::clearHumidityOverride() {
    _humManualOverride = false;
}

float IncubationService::getHumidityLow() const {
    return _humCtrl.getLowThreshold();
}

float IncubationService::getHumidityHigh() const {
    return _humCtrl.getHighThreshold();
}

bool IncubationService::isHumidityManualOverride() const {
    return _humManualOverride;
}

void IncubationService::resetSafety() {
    _safety.reset();
    if (_state == SYS_EMERGENCY) {
        _state = SYS_PAUSED;
        DEBUG_PRINTLN("[SYS] Reset");
    }
}

void IncubationService::acknowledgeAlarm() {
    _alarm.acknowledgeAlarm();
    DEBUG_PRINTLN("[ALARM] Kullanici sustur — speaker durdu");
}

void IncubationService::snoozeActiveAlarm(uint32_t durationMs) {
    AlarmType t = _alarm.getActiveAlarmType();
    // Default sure: candling icin 1 saat, diger alarmlar icin 10 dk
    if (durationMs == 0) {
        durationMs = (t == ALARM_CANDLING_DUE) ? CANDLING_SNOOZE_DURATION_MS : ALARM_MUTE_DURATION_MS;
    }
    _alarm.snoozeAlarm(durationMs);
    DEBUG_PRINTF("[ALARM] Ertelendi: %lu dk\n", durationMs / 60000UL);
}

void IncubationService::dismissActiveAlarm() {
    AlarmType t = _alarm.getActiveAlarmType();
    if (t == ALARM_CANDLING_DUE) {
        // Candling: 24 saat sustur + bugun icinde tekrar tetiklemeyi engelle
        _alarm.dismissAlarm();
        _candlingLastDay = _phaseMgr.getCurrentDay();
        DEBUG_PRINTF("[CANDLE] Bugun (Gun %d) kapatildi — tekrar tetiklenmez\n", _candlingLastDay);
    } else {
        // Standart alarmlar (sicaklik/nem/CO2/sensor): sadece temizle.
        // Durum hala bozuksa kontrol mantigi tarafindan tekrar tetiklenir (guvenlik).
        _alarm.clearAll();
        DEBUG_PRINTLN("[ALARM] Standart alarm temizlendi (durum kotu ise tekrar tetiklenir)");
    }
}

// ==================== TEMIZLIK / BAKIM MODU ====================

bool IncubationService::startCleaning() {
    if (_state == SYS_CLEANING) return true;   // Zaten aktif
    // EMERGENCY'de izin verme (guvenlik). INITIALIZING'de izin VAR
    // (sistem henuz kontrol uretmiyor — temizlik en cok bu evrede istenir).
    if (_state == SYS_EMERGENCY) {
        DEBUG_PRINTLN("[CLEAN] EMERGENCY durumunda baslatilamaz");
        return false;
    }
    _cleaningPrevState = _state;
    _cleaningStartTime = millis();
    // Guvenli varsayilanlar
    _cleaningHeater = CLEANING_DEFAULT_HEATER;
    _cleaningFan    = CLEANING_DEFAULT_FAN;
    _cleaningHum    = CLEANING_DEFAULT_HUM;
    _cleaningTurner = CLEANING_DEFAULT_TURNER;
    // Aktif alarmi sustur (temizlik sirasi sinirsiz alarm gelmesin)
    _alarm.acknowledgeAlarm();
    _state = SYS_CLEANING;
    DEBUG_PRINTLN("[CLEAN] Temizlik modu BASLADI");
    return true;
}

void IncubationService::stopCleaning() {
    if (_state != SYS_CLEANING) return;
    // Tum manuel cikislari kapat
    _heater.setPWM(0);
    _heater.update();
    _humidifier.turnOff();
    _turner.stop();
    // Fan minimumda (sessizce calismaya devam)
    _fan.setPWM(FAN_MIN_PWM);
    // Onceki duruma don
    _state = _cleaningPrevState;
    DEBUG_PRINTF("[CLEAN] Temizlik BITTI — onceki state'e donuldu (%d)\n", (int)_state);
}

bool IncubationService::isCleaning() const {
    return _state == SYS_CLEANING;
}

void IncubationService::setCleaningOutputs(uint8_t heaterPWM, uint8_t fanPWM,
                                           bool humidifierOn, bool turnerOn) {
    _cleaningHeater = heaterPWM;
    _cleaningFan    = fanPWM;
    _cleaningHum    = humidifierOn;
    _cleaningTurner = turnerOn;
    DEBUG_PRINTF("[CLEAN] Manuel cikis: H=%u F=%u Hum=%d Turn=%d\n",
                 heaterPWM, fanPWM, (int)humidifierOn, (int)turnerOn);
}

unsigned long IncubationService::getCleaningTimeRemainingMs() const {
    if (_state != SYS_CLEANING) return 0;
    unsigned long elapsed = millis() - _cleaningStartTime;
    if (elapsed >= CLEANING_TIMEOUT_MS) return 0;
    return CLEANING_TIMEOUT_MS - elapsed;
}

PhaseManager& IncubationService::getPhaseManager() {
    return _phaseMgr;
}

AlarmService& IncubationService::getAlarmService() {
    return _alarm;
}

PersistentStorage& IncubationService::getPersistentStorage() {
    return _storage;
}

void IncubationService::setSensorCalibration(const SensorCalibration &cal) {
    _sensorMgr.setCalibration(cal);
    _storage.saveCalibration(cal.tempOffset1, cal.humOffset1, cal.tempOffset2, cal.humOffset2);
}

SensorCalibration IncubationService::getSensorCalibration() const {
    return _sensorMgr.getCalibration();
}

void IncubationService::setCO2Value(uint16_t ppm) {
    // Manuel deger sadece sensor yokken kullanilir
    if (!_co2Sensor.isReady()) {
        _co2Value = ppm;
    }
}

uint16_t IncubationService::getCO2Value() const {
    return _co2Value;
}

bool IncubationService::hasCO2Sensor() const {
    return _co2Sensor.isReady();
}

const char* IncubationService::getCO2SensorType() const {
    return _co2Sensor.getTypeName();
}

// ==================== CANDLING (DOL KONTROLU) ====================

void IncubationService::updateCandlingSchedule() {
    const AnimalProfile* prof = _phaseMgr.getCurrentProfile();
    if (prof) {
        buildCandlingSchedule(prof, _candlingSched);
        _candlingLastDay = -1;  // Reset
        DEBUG_PRINTF("[CANDLE] %s: %d gun, lockdown=%d, %d kontrol gunu\n",
                     prof->name, prof->totalDays,
                     _candlingSched.lockdownDay, _candlingSched.checkCount);
        for (int i = 0; i < _candlingSched.checkCount; i++) {
            DEBUG_PRINTF("[CANDLE]   Gun %d: %s\n",
                         _candlingSched.checkDays[i], _candlingSched.labels[i]);
        }
    } else {
        memset(&_candlingSched, 0, sizeof(_candlingSched));
        _candlingLastDay = -1;
    }
}

bool IncubationService::isCandlingDayToday() const {
    int day = _rtc.getElapsedDays();
    return isCandlingDay(_candlingSched, day);
}

const char* IncubationService::getCandlingLabelToday() const {
    int day = _rtc.getElapsedDays();
    return getCandlingLabel(_candlingSched, day);
}
