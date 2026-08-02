#include "StorageService.h"

StorageService::StorageService()
    : _customCount(0), _logHead(0), _logCount(0)
{
    memset(_customProfiles, 0, sizeof(_customProfiles));
    memset(_logBuffer, 0, sizeof(_logBuffer));
}

void StorageService::begin() {
    // NVS baslat
    _prefs.begin("kulucka", false);
    Serial.println("[STORAGE] NVS baslatildi");

    // Ozel profilleri NVS'den yukle
    loadCustomProfiles();
    Serial.print("[STORAGE] ");
    Serial.print(_customCount);
    Serial.println(" ozel profil yuklendi");
}

// ==================== AYARLAR (NVS) ====================

void StorageService::saveSettings(const StoredSettings &s) {
    _prefs.putDouble("kp", s.kp);
    _prefs.putDouble("ki", s.ki);
    _prefs.putDouble("kd", s.kd);
    _prefs.putUChar("profIdx", s.profileIndex);
    _prefs.putBool("isCustom", s.isCustomProfile);
    _prefs.putFloat("targetT", s.targetTemp);
    _prefs.putFloat("humLow", s.humLow);
    _prefs.putFloat("humHigh", s.humHigh);
    _prefs.putString("ssid", s.wifiSSID);
    _prefs.putString("pass", s.wifiPass);
    Serial.println("[STORAGE] Ayarlar NVS'e kaydedildi");
}

StoredSettings StorageService::loadSettings() {
    StoredSettings s;
    s.kp = _prefs.getDouble("kp", PID_DEFAULT_KP);
    s.ki = _prefs.getDouble("ki", PID_DEFAULT_KI);
    s.kd = _prefs.getDouble("kd", PID_DEFAULT_KD);
    s.profileIndex = _prefs.getUChar("profIdx", 0);
    s.isCustomProfile = _prefs.getBool("isCustom", false);
    s.targetTemp = _prefs.getFloat("targetT", 37.8f);
    s.humLow = _prefs.getFloat("humLow", 55.0f);
    s.humHigh = _prefs.getFloat("humHigh", 65.0f);

    String ssid = _prefs.getString("ssid", WIFI_SSID);
    String pass = _prefs.getString("pass", WIFI_PASSWORD);
    strncpy(s.wifiSSID, ssid.c_str(), 32);
    s.wifiSSID[32] = '\0';
    strncpy(s.wifiPass, pass.c_str(), 64);
    s.wifiPass[64] = '\0';

    Serial.println("[STORAGE] Ayarlar NVS'den yuklendi");
    return s;
}

void StorageService::saveWiFi(const char* ssid, const char* pass) {
    _prefs.putString("ssid", ssid);
    _prefs.putString("pass", pass);
    Serial.println("[STORAGE] WiFi bilgileri kaydedildi");
}

// ==================== OZEL PROFILLER (NVS BLOB) ====================

uint8_t StorageService::getCustomProfileCount() const {
    return _customCount;
}

CustomProfile StorageService::getCustomProfile(uint8_t index) const {
    if (index < _customCount) {
        return _customProfiles[index];
    }
    CustomProfile empty;
    memset(&empty, 0, sizeof(empty));
    return empty;
}

bool StorageService::addCustomProfile(const CustomProfile &profile) {
    if (_customCount >= MAX_CUSTOM_PROFILES) {
        Serial.println("[STORAGE] Maks profil sayisina ulasildi");
        return false;
    }
    _customProfiles[_customCount] = profile;
    _customProfiles[_customCount].active = true;
    _customCount++;
    saveCustomProfiles();
    Serial.print("[STORAGE] Ozel profil eklendi: ");
    Serial.println(profile.name);
    return true;
}

bool StorageService::updateCustomProfile(uint8_t index, const CustomProfile &profile) {
    if (index >= _customCount) return false;
    _customProfiles[index] = profile;
    _customProfiles[index].active = true;
    saveCustomProfiles();
    Serial.print("[STORAGE] Profil guncellendi: ");
    Serial.println(profile.name);
    return true;
}

bool StorageService::deleteCustomProfile(uint8_t index) {
    if (index >= _customCount) return false;
    Serial.print("[STORAGE] Profil silindi: ");
    Serial.println(_customProfiles[index].name);

    // Kaydirma
    for (uint8_t i = index; i < _customCount - 1; i++) {
        _customProfiles[i] = _customProfiles[i + 1];
    }
    _customCount--;
    memset(&_customProfiles[_customCount], 0, sizeof(CustomProfile));
    saveCustomProfiles();
    return true;
}

String StorageService::getCustomProfilesJSON() const {
    String json = "[";
    for (uint8_t i = 0; i < _customCount; i++) {
        if (i > 0) json += ",";
        const CustomProfile &cp = _customProfiles[i];
        json += "{\"index\":";
        json += String(i);
        json += ",\"name\":\"";
        json += String(cp.name);
        json += "\",\"nameEN\":\"";
        json += String(cp.nameEN);
        json += "\",\"totalDays\":";
        json += String(cp.totalDays);
        json += ",\"phaseCount\":";
        json += String(cp.phaseCount);
        json += ",\"phases\":[";
        for (uint8_t p = 0; p < cp.phaseCount; p++) {
            if (p > 0) json += ",";
            json += "{\"startDay\":";
            json += String(cp.phases[p].startDay);
            json += ",\"endDay\":";
            json += String(cp.phases[p].endDay);
            json += ",\"temp\":";
            json += String(cp.phases[p].temperature, 1);
            json += ",\"humLow\":";
            json += String(cp.phases[p].humidityLow, 1);
            json += ",\"humHigh\":";
            json += String(cp.phases[p].humidityHigh, 1);
            json += ",\"turning\":";
            json += cp.phases[p].turningEnabled ? "true" : "false";
            json += ",\"name\":\"";
            json += String(cp.phases[p].phaseName);
            json += "\"}";
        }
        json += "]}";
    }
    json += "]";
    return json;
}

void StorageService::toAnimalProfile(const CustomProfile &cp, AnimalProfile &ap) {
    ap.name = cp.name;
    ap.nameEN = cp.nameEN;
    ap.totalDays = cp.totalDays;
    ap.phaseCount = cp.phaseCount;
    for (uint8_t i = 0; i < 4; i++) {
        ap.phases[i].startDay = cp.phases[i].startDay;
        ap.phases[i].endDay = cp.phases[i].endDay;
        ap.phases[i].temperature = cp.phases[i].temperature;
        ap.phases[i].humidityLow = cp.phases[i].humidityLow;
        ap.phases[i].humidityHigh = cp.phases[i].humidityHigh;
        ap.phases[i].turningEnabled = cp.phases[i].turningEnabled;
        ap.phases[i].phaseName = cp.phases[i].phaseName;
    }
}

void StorageService::loadCustomProfiles() {
    _customCount = _prefs.getUChar("cpCount", 0);
    if (_customCount > MAX_CUSTOM_PROFILES) _customCount = 0;

    if (_customCount > 0) {
        size_t len = sizeof(CustomProfile) * _customCount;
        size_t read = _prefs.getBytes("cpData", _customProfiles, len);
        if (read != len) {
            // Veri bozulmussa sifirla
            _customCount = 0;
            memset(_customProfiles, 0, sizeof(_customProfiles));
            Serial.println("[STORAGE] Profil verisi bozuk, sifirlandi");
        }
    }
}

void StorageService::saveCustomProfiles() {
    _prefs.putUChar("cpCount", _customCount);
    if (_customCount > 0) {
        size_t len = sizeof(CustomProfile) * _customCount;
        _prefs.putBytes("cpData", _customProfiles, len);
    } else {
        _prefs.remove("cpData");
    }
    Serial.println("[STORAGE] Profiller NVS'ye kaydedildi");
}

// ==================== VERI LOGLAMA (RAM RING BUFFER) ====================

void StorageService::logData(float temp, float hum, uint8_t heaterPWM, uint8_t fanPWM, const char* phase) {
    LogEntry &entry = _logBuffer[_logHead];
    entry.timestamp = millis() / 1000;
    entry.temp = temp;
    entry.hum = hum;
    entry.heaterPWM = heaterPWM;
    entry.fanPWM = fanPWM;
    strncpy(entry.phase, phase, 15);
    entry.phase[15] = '\0';

    _logHead = (_logHead + 1) % MAX_LOG_ENTRIES;
    if (_logCount < MAX_LOG_ENTRIES) _logCount++;
}

String StorageService::getLogCSV() const {
    String csv = "timestamp,temp,hum,heater,fan,phase\n";
    if (_logCount == 0) return csv;

    // Ring buffer'dan sirali oku
    uint16_t start;
    if (_logCount < MAX_LOG_ENTRIES) {
        start = 0;
    } else {
        start = _logHead; // En eski veri
    }

    for (uint16_t i = 0; i < _logCount; i++) {
        uint16_t idx = (start + i) % MAX_LOG_ENTRIES;
        const LogEntry &e = _logBuffer[idx];
        csv += String(e.timestamp);
        csv += ",";
        csv += String(e.temp, 1);
        csv += ",";
        csv += String(e.hum, 1);
        csv += ",";
        csv += String(e.heaterPWM);
        csv += ",";
        csv += String(e.fanPWM);
        csv += ",";
        csv += String(e.phase);
        csv += "\n";
    }
    return csv;
}

void StorageService::clearLog() {
    _logHead = 0;
    _logCount = 0;
    memset(_logBuffer, 0, sizeof(_logBuffer));
    Serial.println("[STORAGE] Log temizlendi");
}

unsigned long StorageService::getLogSize() const {
    return _logCount;
}
