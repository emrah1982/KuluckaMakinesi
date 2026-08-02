#include "SafetyService.h"
#include "RelayBoard.h"   // acil kapatmada roleleri israrla kapatmak icin

SafetyService::SafetyService(HeaterDriver &heater, HumidifierDriver &humidifier, FanDriver &fan)
    : _heater(heater)
    , _humidifier(humidifier)
    , _fan(fan)
    , _state(SAFETY_OK)
    , _shutdownReason("")
    , _shutdown(false)
    , _lowTempStartTime(0)
    , _lowTempActive(false)
    , _shutdownTime(0)
    , _shutdownTemp(0.0f)
    , _runaway(false)
    , _runawayReason("")
{
}

void SafetyService::begin() {
    Serial.println("[SAFETY] Guvenlik servisi baslatildi");
}

SafetyState SafetyService::check(float temperature, bool sensorOK, uint8_t sensorFailCount,
                                  uint8_t sensor1FailCount, uint8_t sensor2FailCount) {
    if (_shutdown) return SAFETY_SHUTDOWN;

    // Tum sensorler ariza kontrolu
    if (!sensorOK && sensorFailCount >= SAFETY_SENSOR_FAIL_COUNT) {
        emergencyShutdown("Sensor hatasi - ardisik " + String(sensorFailCount) + " basarisiz okuma");
        return SAFETY_SHUTDOWN;
    }

    // Tek sensor ariza kontrolu - diger sensor calissa bile uyar
    // Sensor tamamen olurse fuzyon tek sensore duser, guvenilirlik azalir
    if (sensor1FailCount >= SAFETY_SENSOR_FAIL_COUNT && sensor2FailCount >= SAFETY_SENSOR_FAIL_COUNT) {
        emergencyShutdown("Her iki sensor de arizali! S1:" + String(sensor1FailCount) +
                          " S2:" + String(sensor2FailCount) + " ardisik hata");
        return SAFETY_SHUTDOWN;
    }

    // Aşırı sıcaklık kontrolü
    if (sensorOK && temperature >= SAFETY_TEMP_MAX) {
        emergencyShutdown("Asiri sicaklik: " + String(temperature, 1) + "C (max " + String(SAFETY_TEMP_MAX, 1) + "C)");
        return SAFETY_SHUTDOWN;
    }

    // Alt sicaklik kritik kontrolu (isitici sonsuz calisma korumasi)
    // 2 dk boyunca SAFETY_TEMP_CRITICAL_LOW altinda kalirsa -> shutdown
    if (sensorOK && temperature < SAFETY_TEMP_CRITICAL_LOW && temperature > 0) {
        if (!_lowTempActive) {
            _lowTempActive = true;
            _lowTempStartTime = millis();
            Serial.printf("[SAFETY] UYARI: Sicaklik kritik dusuk: %.1fC (<%s)\n",
                          temperature, String(SAFETY_TEMP_CRITICAL_LOW, 1).c_str());
        } else if ((millis() - _lowTempStartTime) >= SAFETY_LOW_TEMP_DURATION) {
            emergencyShutdown("Sicaklik " + String(SAFETY_LOW_TEMP_DURATION / 1000) +
                "sn boyunca " + String(temperature, 1) + "C - sensor/isitici ariza suplesi");
            return SAFETY_SHUTDOWN;
        }
        _state = SAFETY_WARNING;
        return SAFETY_WARNING;
    } else {
        _lowTempActive = false;
    }

    // Sensor sapma kontrolu (mantıksız düşük değer - uyarı)
    if (sensorOK && temperature < SAFETY_TEMP_MIN && temperature >= SAFETY_TEMP_CRITICAL_LOW) {
        _state = SAFETY_WARNING;
        Serial.println("[SAFETY] UYARI: Sicaklik cok dusuk, sensor hatasi olabilir!");
        return SAFETY_WARNING;
    }

    _state = SAFETY_OK;
    return SAFETY_OK;
}

void SafetyService::emergencyShutdown(const String &reason) {
    // Zaten kapaliysa dogrulama zamanlayicisini sifirlama (kacis tespiti bozulur)
    bool alreadyDown = _shutdown;

    _shutdown = true;
    _state = SAFETY_SHUTDOWN;
    _shutdownReason = reason;

    // Tüm çıkışları kapat
    _heater.stop();
    _humidifier.turnOff();
    // Fan'ı açık bırak (soğutma için)
    _fan.setPWM(FAN_MAX_PWM);

    // Roleleri ISRARLA kapat: normal yol I2C hatasinda sessizce basarisiz
    // olabilir. forceAllOff() bus kurtarma ile birlikte tekrar dener.
    if (!RelayBoard::instance().forceAllOff()) {
        enterRunaway("Roleler kapatilamadi (I2C/PCF8574 arizasi)");
    }

    if (!alreadyDown) {
        _shutdownTime = millis();
        _shutdownTemp = 0.0f;   // ilk gecerli okumada doldurulur
    }

    Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    Serial.println("[SAFETY] ACIL KAPATMA: " + reason);
    Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
}

// ---------------------------------------------------------------------
//  Kapatma sonrasi dogrulama - yapisik role / termal kacis tespiti
//
//  Kapattiktan sonra sicaklik gercekten dusuyor mu? Dusmuyorsa isitici
//  hala enerjileniyordur (role kontagi yapismis veya I2C komutu gecmemis).
//  Bu, kulucka makinesini yakabilecek senaryodur; sessizce beklenemez.
// ---------------------------------------------------------------------
void SafetyService::verifyShutdown(float temperature, bool sensorOK) {
    if (!_shutdown || _runaway) return;
    if (!sensorOK) return;              // sensor yoksa karar veremeyiz

    // Kapatma anindaki referans sicakligi ilk gecerli okumada yakala
    if (_shutdownTemp <= 0.0f) {
        _shutdownTemp = temperature;
        return;
    }

    if ((millis() - _shutdownTime) < SAFETY_VERIFY_DELAY_MS) {
        // Sure dolmadan da acik bir yukselis varsa erken karar ver
        if (temperature >= SAFETY_TEMP_MAX &&
            temperature > (_shutdownTemp + SAFETY_VERIFY_TEMP_DROP)) {
            enterRunaway("Kapatmaya ragmen sicaklik yukseliyor: " +
                         String(_shutdownTemp, 1) + "C -> " +
                         String(temperature, 1) + "C");
        }
        return;
    }

    // Sure doldu: beklenen dusus gerceklesti mi?
    float drop = _shutdownTemp - temperature;
    if (drop < SAFETY_VERIFY_TEMP_DROP) {
        enterRunaway("Kapatmadan " + String(SAFETY_VERIFY_DELAY_MS / 1000) +
                     "sn sonra sicaklik dusmedi (" + String(_shutdownTemp, 1) +
                     "C -> " + String(temperature, 1) + "C). Role yapismis olabilir.");
    } else {
        // Dusus dogrulandi; tekrar tekrar kontrol etmeye gerek yok
        _shutdownTemp = temperature;
        _shutdownTime = millis();
    }
}

void SafetyService::enterRunaway(const String &reason) {
    if (_runaway) return;
    _runaway = true;
    _runawayReason = reason;
    _state = SAFETY_SHUTDOWN;

    // Elde kalan tek sogutma araci fan: tam guce al ve orada tut
    _fan.setSoftBehavior(false);
    _fan.setPWM(SAFETY_RUNAWAY_FAN_PWM);

    // Roleleri kapatmayi bir kez daha dene (bus bu arada duzelmis olabilir)
    RelayBoard::instance().forceAllOff();

    Serial.println("#######################################");
    Serial.println("[SAFETY] TERMAL KACIS: " + reason);
    Serial.println("[SAFETY] Fan tam guce alindi. FIZIKSEL MUDAHALE GEREKLI:");
    Serial.println("[SAFETY] Isitici beslemesini elle kesin!");
    Serial.println("#######################################");
}

void SafetyService::reportIOFailure(const String &reason) {
    if (!_shutdown) emergencyShutdown("I/O arizasi: " + reason);
    enterRunaway("Roleler kontrol edilemiyor: " + reason);
}

bool SafetyService::isRunaway() const {
    return _runaway;
}

String SafetyService::getRunawayReason() const {
    return _runawayReason;
}

void SafetyService::reset() {
    _shutdown = false;
    _state = SAFETY_OK;
    _shutdownReason = "";
    _lowTempActive = false;
    _lowTempStartTime = 0;
    _shutdownTime = 0;
    _shutdownTemp = 0.0f;
    _runaway = false;
    _runawayReason = "";
    _fan.setSoftBehavior(true);
    Serial.println("[SAFETY] Guvenlik sifirlandi");
}

SafetyState SafetyService::getState() const {
    return _state;
}

String SafetyService::getShutdownReason() const {
    return _shutdownReason;
}

bool SafetyService::isShutdown() const {
    return _shutdown;
}
