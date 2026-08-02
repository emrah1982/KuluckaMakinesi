#include "SafetyService.h"

SafetyService::SafetyService(HeaterDriver &heater, HumidifierDriver &humidifier, FanDriver &fan)
    : _heater(heater)
    , _humidifier(humidifier)
    , _fan(fan)
    , _state(SAFETY_OK)
    , _shutdownReason("")
    , _shutdown(false)
{
}

void SafetyService::begin() {
    Serial.println("[SAFETY] Guvenlik servisi baslatildi");
}

SafetyState SafetyService::check(float temperature, bool sensorOK, uint8_t sensorFailCount) {
    if (_shutdown) return SAFETY_SHUTDOWN;

    // Sensör hatası kontrolü
    if (!sensorOK && sensorFailCount >= SAFETY_SENSOR_FAIL_COUNT) {
        emergencyShutdown("Sensor hatasi - ardisik " + String(sensorFailCount) + " basarisiz okuma");
        return SAFETY_SHUTDOWN;
    }

    // Aşırı sıcaklık kontrolü
    if (sensorOK && temperature >= SAFETY_TEMP_MAX) {
        emergencyShutdown("Asiri sicaklik: " + String(temperature, 1) + "C (max " + String(SAFETY_TEMP_MAX, 1) + "C)");
        return SAFETY_SHUTDOWN;
    }

    // Sensör sapma kontrolü (mantıksız düşük değer)
    if (sensorOK && temperature < SAFETY_TEMP_MIN && temperature > 0) {
        _state = SAFETY_WARNING;
        Serial.println("[SAFETY] UYARI: Sicaklik cok dusuk, sensor hatasi olabilir!");
        return SAFETY_WARNING;
    }

    _state = SAFETY_OK;
    return SAFETY_OK;
}

void SafetyService::emergencyShutdown(const String &reason) {
    _shutdown = true;
    _state = SAFETY_SHUTDOWN;
    _shutdownReason = reason;

    // Tüm çıkışları kapat
    _heater.stop();
    _humidifier.turnOff();
    // Fan'ı açık bırak (soğutma için)
    _fan.setPWM(FAN_MAX_PWM);

    Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    Serial.println("[SAFETY] ACIL KAPATMA: " + reason);
    Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
}

void SafetyService::reset() {
    _shutdown = false;
    _state = SAFETY_OK;
    _shutdownReason = "";
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
