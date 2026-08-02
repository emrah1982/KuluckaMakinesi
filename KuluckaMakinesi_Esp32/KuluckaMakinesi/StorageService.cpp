#include "StorageService.h"
 #include <SPI.h>
 #include <SD.h>
 #include <esp_task_wdt.h>
#include "WdtFeed.h"
 #include <new>

StorageService::StorageService()
    : _customCount(0), _sdReady(false), _sdLoggingEnabled(false), _eggCount(100), _logHead(0), _logCount(0)
{
    memset(_customProfiles, 0, sizeof(_customProfiles));
    memset(_logBuffer, 0, sizeof(_logBuffer));
    // Profil override map: hepsi -1 (override yok)
    for (uint8_t i = 0; i < PROFILE_COUNT; i++) _profileOverrideMap[i] = -1;
}

void StorageService::begin() {
    // NVS baslat
    _prefs.begin("kulucka", false);
    Serial.println("[STORAGE] NVS baslatildi");

    // Sema migration zinciri:
    //   v1 (8 alan)  -> v3 (15 alan, dogrudan tasinir)
    //   v2 (14 alan) -> v3 (turningAngleDeg eklendi)
    // Idempotent: cpSchemaVer dogru deger ise no-op.
    migrateCustomProfilesV1ToV3();
    migrateCustomProfilesV2ToV3();

    // Ozel profilleri NVS'den yukle
    loadCustomProfiles();
    Serial.print("[STORAGE] ");
    Serial.print(_customCount);
    Serial.println(" ozel profil yuklendi");

    // Profil override map (v4 — 2026-04-25): yoksa varsayilan tum -1
    loadProfileOverrideMap();
}

bool StorageService::beginSD() {
#if SD_ENABLED == 0
    // SD kart devre disi (Config.h'da SD_ENABLED=0)
    Serial.println("[STORAGE] SD devre disi (SD_ENABLED=0)");
    _sdReady = false;
    return false;
#else
    // SD kart baslangicinda watchdog reset
    wdtFeed();
    
    // SD.begin() bloke edebilir, timeout ile koruma
    unsigned long startMs = millis();
    _sdReady = SD.begin(SD_CS_PIN);
    unsigned long elapsed = millis() - startMs;
    
    wdtFeed();
    
    if (_sdReady) {
        Serial.print("[STORAGE] SD baslatildi (");
        Serial.print(elapsed);
        Serial.println(" ms)");
    } else {
        Serial.println("[STORAGE] SD baslatilamadi");
    }
    return _sdReady;
#endif
}

void StorageService::updateSD() {
#if SD_ENABLED == 0
    return; // SD devre disi
#else
    static unsigned long lastCheckMs = 0;
    static uint8_t       retryCount = 0;
    const unsigned long  now = millis();
    const unsigned long  CHECK_INTERVAL_MS = 10000;
    const uint8_t        MAX_RETRIES = 3;

    if (_sdReady) return;
    if (retryCount >= MAX_RETRIES) return;   // Vazgec — kart yok veya bozuk
    if (now - lastCheckMs < CHECK_INTERVAL_MS) return;
    lastCheckMs = now;

    wdtFeed();
    retryCount++;
    if (beginSD()) {
        _sdLoggingEnabled = true;
        retryCount = 0;   // Basarili, sayaci sifirla
    } else if (retryCount >= MAX_RETRIES) {
        Serial.println("[STORAGE] SD vazgecildi (kart yok veya bozuk)");
    }
#endif
}

bool StorageService::isSDReady() const {
    return _sdReady;
}

void StorageService::setSDLoggingEnabled(bool enabled) {
    _sdLoggingEnabled = enabled;
}

bool StorageService::isSDLoggingEnabled() const {
    return _sdLoggingEnabled;
}

void StorageService::appendSDLog(uint32_t unixTime, const char* animalName, float temp, float hum) {
    if (!_sdReady || !_sdLoggingEnabled) return;

    const char* path = "/logs.csv";

    bool fileExists = SD.exists(path);
    File f = SD.open(path, FILE_APPEND);
    if (!f) {
        Serial.println("[STORAGE] SD log dosyasi acilamadi");
        _sdReady = false;
        _sdLoggingEnabled = false;
        return;
    }

    if (!fileExists || f.size() == 0) {
        f.println("unix_time,animal,egg_count,temp,hum");
    }

    f.print(unixTime);
    f.print(",");
    if (animalName && animalName[0] != '\0') {
        f.print(animalName);
    } else {
        f.print("-");
    }
    f.print(",");
    f.print(_eggCount);
    f.print(",");
    f.print(temp, 2);
    f.print(",");
    f.println(hum, 2);
    f.close();
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
    _prefs.putUShort("eggCount", s.eggCount);
    _prefs.putString("ssid", s.wifiSSID);
    _prefs.putString("pass", s.wifiPass);
    _prefs.putString("yumurtaIP", s.yumurtaIP);
    _prefs.putBool("yumurtaEn", s.yumurtaEnabled);
    _eggCount = s.eggCount;
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
    s.eggCount = _prefs.getUShort("eggCount", 100);

    String ssid = _prefs.getString("ssid", WIFI_SSID);
    String pass = _prefs.getString("pass", WIFI_PASSWORD);
    strncpy(s.wifiSSID, ssid.c_str(), 32);
    s.wifiSSID[32] = '\0';
    strncpy(s.wifiPass, pass.c_str(), 64);
    s.wifiPass[64] = '\0';
    String yip = _prefs.getString("yumurtaIP", "");
    strncpy(s.yumurtaIP, yip.c_str(), 39);
    s.yumurtaIP[39] = '\0';
    s.yumurtaEnabled = _prefs.getBool("yumurtaEn", true);  // varsayilan: aktif

    _eggCount = s.eggCount;

    Serial.println("[STORAGE] Ayarlar NVS'den yuklendi");
    return s;
}

uint16_t StorageService::getEggCount() const {
    return _eggCount;
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

    // Override map'inde silinen index'e bagli tum girisleri temizle ve
    // daha buyuk indexleri 1 azalt (kaydirma sonucu kayma).
    compactOverrideMapAfterDelete(index);
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
            json += ",\"tempEnd\":";
            json += String(cp.phases[p].tempEnd, 1);
            json += ",\"humLow\":";
            json += String(cp.phases[p].humidityLow, 1);
            json += ",\"humHigh\":";
            json += String(cp.phases[p].humidityHigh, 1);
            json += ",\"turning\":";
            json += cp.phases[p].turningEnabled ? "true" : "false";
            json += ",\"turnIntMin\":";
            json += String(cp.phases[p].turningIntervalMin);
            json += ",\"turnDurSec\":";
            json += String(cp.phases[p].turningDurationSec);
            json += ",\"turnAngleDeg\":";
            json += String(cp.phases[p].turningAngleDeg);
            json += ",\"cool\":";
            json += cp.phases[p].coolingEnabled ? "true" : "false";
            json += ",\"coolMin\":";
            json += String(cp.phases[p].coolingDurationMin);
            json += ",\"coolPerDay\":";
            json += String(cp.phases[p].coolingPerDay);
            json += ",\"spray\":";
            json += cp.phases[p].sprayingEnabled ? "true" : "false";
            json += ",\"spraySec\":";
            json += String(cp.phases[p].sprayingDurationSec);
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
        ap.phases[i].startDay          = cp.phases[i].startDay;
        ap.phases[i].endDay            = cp.phases[i].endDay;
        ap.phases[i].temperature       = cp.phases[i].temperature;
        ap.phases[i].tempEnd           = cp.phases[i].tempEnd;
        ap.phases[i].humidityLow       = cp.phases[i].humidityLow;
        ap.phases[i].humidityHigh      = cp.phases[i].humidityHigh;
        ap.phases[i].turningEnabled    = cp.phases[i].turningEnabled;
        ap.phases[i].turningIntervalMin = cp.phases[i].turningIntervalMin;
        ap.phases[i].turningDurationSec = cp.phases[i].turningDurationSec;
        ap.phases[i].turningAngleDeg    = cp.phases[i].turningAngleDeg;
        ap.phases[i].coolingEnabled     = cp.phases[i].coolingEnabled;
        ap.phases[i].coolingDurationMin = cp.phases[i].coolingDurationMin;
        ap.phases[i].coolingPerDay      = cp.phases[i].coolingPerDay;
        ap.phases[i].sprayingEnabled    = cp.phases[i].sprayingEnabled;
        ap.phases[i].sprayingDurationSec = cp.phases[i].sprayingDurationSec;
        ap.phases[i].phaseName         = cp.phases[i].phaseName;
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
    _prefs.putUChar("cpSchemaVer", 3);
    if (_customCount > 0) {
        size_t len = sizeof(CustomProfile) * _customCount;
        _prefs.putBytes("cpData", _customProfiles, len);
    } else {
        _prefs.remove("cpData");
    }
    Serial.println("[STORAGE] Profiller NVS'ye kaydedildi (v3)");
}

// ---- NVS schema migration ------------------------------------------------
// V1 (8 alanli faz, ne turning detay ne cooling) ve V2 (14 alan, aci yok)
// formatlari icin ayri snapshot struct'lar tutulur. Tum yeni kayitlar V3.
//
// V3 farki: faz icine `turningAngleDeg` (1 byte) eklendi.
namespace {
    struct CustomProfileV1 {
        char    name[32];
        char    nameEN[32];
        uint8_t totalDays;
        uint8_t phaseCount;
        struct {
            uint8_t startDay;
            uint8_t endDay;
            float   temperature;
            float   tempEnd;
            float   humidityLow;
            float   humidityHigh;
            bool    turningEnabled;
            char    phaseName[16];
        } phases[4];
        bool    active;
    };

    struct CustomProfileV2 {
        char    name[32];
        char    nameEN[32];
        uint8_t totalDays;
        uint8_t phaseCount;
        struct {
            uint8_t  startDay;
            uint8_t  endDay;
            float    temperature;
            float    tempEnd;
            float    humidityLow;
            float    humidityHigh;
            bool     turningEnabled;
            uint16_t turningIntervalMin;
            uint8_t  turningDurationSec;
            bool     coolingEnabled;
            uint8_t  coolingDurationMin;
            uint8_t  coolingPerDay;
            bool     sprayingEnabled;
            uint8_t  sprayingDurationSec;
            char     phaseName[16];
        } phases[4];
        bool    active;
    };
}

void StorageService::migrateCustomProfilesV1ToV3() {
    uint8_t schemaVer = _prefs.getUChar("cpSchemaVer", 1);
    if (schemaVer >= 2) return;   // V1 disindaki sürümler bu fonksiyonu atlar

    uint8_t count = _prefs.getUChar("cpCount", 0);
    if (count == 0 || count > MAX_CUSTOM_PROFILES) {
        _prefs.putUChar("cpSchemaVer", 3);
        Serial.println("[STORAGE] Schema v3 (bos) isaretlendi");
        return;
    }

    CustomProfileV1 *oldProfiles = new (std::nothrow) CustomProfileV1[count];
    if (!oldProfiles) {
        Serial.println("[STORAGE] V1 migration: bellek yetersiz, atlaniyor");
        return;
    }

    size_t oldLen = sizeof(CustomProfileV1) * count;
    size_t read = _prefs.getBytes("cpData", oldProfiles, oldLen);
    if (read != oldLen) {
        _prefs.remove("cpData");
        _prefs.putUChar("cpCount", 0);
        _prefs.putUChar("cpSchemaVer", 3);
        delete[] oldProfiles;
        Serial.println("[STORAGE] V1 veri bozuk, temizlendi");
        return;
    }

    memset(_customProfiles, 0, sizeof(_customProfiles));
    for (uint8_t i = 0; i < count && i < MAX_CUSTOM_PROFILES; i++) {
        CustomProfile &dst = _customProfiles[i];
        const CustomProfileV1 &src = oldProfiles[i];

        memcpy(dst.name, src.name, sizeof(dst.name));
        memcpy(dst.nameEN, src.nameEN, sizeof(dst.nameEN));
        dst.totalDays  = src.totalDays;
        dst.phaseCount = src.phaseCount;
        dst.active     = src.active;

        for (uint8_t p = 0; p < 4; p++) {
            dst.phases[p].startDay     = src.phases[p].startDay;
            dst.phases[p].endDay       = src.phases[p].endDay;
            dst.phases[p].temperature  = src.phases[p].temperature;
            dst.phases[p].tempEnd      = src.phases[p].tempEnd;
            dst.phases[p].humidityLow  = src.phases[p].humidityLow;
            dst.phases[p].humidityHigh = src.phases[p].humidityHigh;
            dst.phases[p].turningEnabled = src.phases[p].turningEnabled;

            dst.phases[p].turningIntervalMin  = src.phases[p].turningEnabled ? 60 : 0;
            dst.phases[p].turningDurationSec  = src.phases[p].turningEnabled ? 15 : 0;
            dst.phases[p].turningAngleDeg     = src.phases[p].turningEnabled
                                                  ? TURNER_DEFAULT_ANGLE_DEG : 0;
            dst.phases[p].coolingEnabled      = false;
            dst.phases[p].coolingDurationMin  = 0;
            dst.phases[p].coolingPerDay       = 0;
            dst.phases[p].sprayingEnabled     = false;
            dst.phases[p].sprayingDurationSec = 0;

            memcpy(dst.phases[p].phaseName, src.phases[p].phaseName,
                   sizeof(dst.phases[p].phaseName));
        }
    }
    _customCount = count;
    delete[] oldProfiles;

    saveCustomProfiles();   // cpSchemaVer=3 isaretler
    Serial.print("[STORAGE] V1 -> V3 migration tamamlandi (");
    Serial.print(count);
    Serial.println(" profil)");
}

void StorageService::migrateCustomProfilesV2ToV3() {
    uint8_t schemaVer = _prefs.getUChar("cpSchemaVer", 1);
    if (schemaVer != 2) return;   // sadece tam v2 -> v3

    uint8_t count = _prefs.getUChar("cpCount", 0);
    if (count == 0 || count > MAX_CUSTOM_PROFILES) {
        _prefs.putUChar("cpSchemaVer", 3);
        Serial.println("[STORAGE] V2 -> V3: bos kayit, schema guncellendi");
        return;
    }

    CustomProfileV2 *oldProfiles = new (std::nothrow) CustomProfileV2[count];
    if (!oldProfiles) {
        Serial.println("[STORAGE] V2 migration: bellek yetersiz, atlaniyor");
        return;
    }

    size_t oldLen = sizeof(CustomProfileV2) * count;
    size_t read = _prefs.getBytes("cpData", oldProfiles, oldLen);
    if (read != oldLen) {
        _prefs.remove("cpData");
        _prefs.putUChar("cpCount", 0);
        _prefs.putUChar("cpSchemaVer", 3);
        delete[] oldProfiles;
        Serial.println("[STORAGE] V2 veri bozuk, temizlendi");
        return;
    }

    memset(_customProfiles, 0, sizeof(_customProfiles));
    for (uint8_t i = 0; i < count && i < MAX_CUSTOM_PROFILES; i++) {
        CustomProfile &dst = _customProfiles[i];
        const CustomProfileV2 &src = oldProfiles[i];

        memcpy(dst.name, src.name, sizeof(dst.name));
        memcpy(dst.nameEN, src.nameEN, sizeof(dst.nameEN));
        dst.totalDays  = src.totalDays;
        dst.phaseCount = src.phaseCount;
        dst.active     = src.active;

        for (uint8_t p = 0; p < 4; p++) {
            dst.phases[p].startDay           = src.phases[p].startDay;
            dst.phases[p].endDay             = src.phases[p].endDay;
            dst.phases[p].temperature        = src.phases[p].temperature;
            dst.phases[p].tempEnd            = src.phases[p].tempEnd;
            dst.phases[p].humidityLow        = src.phases[p].humidityLow;
            dst.phases[p].humidityHigh       = src.phases[p].humidityHigh;
            dst.phases[p].turningEnabled     = src.phases[p].turningEnabled;
            dst.phases[p].turningIntervalMin = src.phases[p].turningIntervalMin;
            dst.phases[p].turningDurationSec = src.phases[p].turningDurationSec;
            // YENI: aci varsayilani — turning aktifse default, degilse 0
            dst.phases[p].turningAngleDeg    = src.phases[p].turningEnabled
                                                  ? TURNER_DEFAULT_ANGLE_DEG : 0;
            dst.phases[p].coolingEnabled      = src.phases[p].coolingEnabled;
            dst.phases[p].coolingDurationMin  = src.phases[p].coolingDurationMin;
            dst.phases[p].coolingPerDay       = src.phases[p].coolingPerDay;
            dst.phases[p].sprayingEnabled     = src.phases[p].sprayingEnabled;
            dst.phases[p].sprayingDurationSec = src.phases[p].sprayingDurationSec;

            memcpy(dst.phases[p].phaseName, src.phases[p].phaseName,
                   sizeof(dst.phases[p].phaseName));
        }
    }
    _customCount = count;
    delete[] oldProfiles;

    saveCustomProfiles();
    Serial.print("[STORAGE] V2 -> V3 migration tamamlandi (");
    Serial.print(count);
    Serial.println(" profil)");
}

// ==================== PROFIL OVERRIDE (2026-04-25) ====================

void StorageService::fromAnimalProfile(const AnimalProfile &ap, CustomProfile &cp) {
    memset(&cp, 0, sizeof(cp));
    if (ap.name)   { strncpy(cp.name,   ap.name,   sizeof(cp.name)   - 1); }
    if (ap.nameEN) { strncpy(cp.nameEN, ap.nameEN, sizeof(cp.nameEN) - 1); }
    cp.totalDays  = ap.totalDays;
    cp.phaseCount = ap.phaseCount;
    cp.active     = true;
    for (uint8_t i = 0; i < 4; i++) {
        cp.phases[i].startDay           = ap.phases[i].startDay;
        cp.phases[i].endDay             = ap.phases[i].endDay;
        cp.phases[i].temperature        = ap.phases[i].temperature;
        cp.phases[i].tempEnd            = ap.phases[i].tempEnd;
        cp.phases[i].humidityLow        = ap.phases[i].humidityLow;
        cp.phases[i].humidityHigh       = ap.phases[i].humidityHigh;
        cp.phases[i].turningEnabled     = ap.phases[i].turningEnabled;
        cp.phases[i].turningIntervalMin = ap.phases[i].turningIntervalMin;
        cp.phases[i].turningDurationSec = ap.phases[i].turningDurationSec;
        cp.phases[i].turningAngleDeg    = ap.phases[i].turningAngleDeg;
        cp.phases[i].coolingEnabled     = ap.phases[i].coolingEnabled;
        cp.phases[i].coolingDurationMin = ap.phases[i].coolingDurationMin;
        cp.phases[i].coolingPerDay      = ap.phases[i].coolingPerDay;
        cp.phases[i].sprayingEnabled    = ap.phases[i].sprayingEnabled;
        cp.phases[i].sprayingDurationSec = ap.phases[i].sprayingDurationSec;
        if (ap.phases[i].phaseName) {
            strncpy(cp.phases[i].phaseName, ap.phases[i].phaseName,
                    sizeof(cp.phases[i].phaseName) - 1);
        }
    }
}

bool StorageService::cloneProfileToOverride(uint8_t profileIdx) {
    if (profileIdx >= PROFILE_COUNT) return false;
    if (_profileOverrideMap[profileIdx] >= 0) {
        // Zaten override var, tekrar klonlama
        return true;
    }
    if (_customCount >= MAX_CUSTOM_PROFILES) {
        Serial.println("[STORAGE] Override icin custom slot kalmadi");
        return false;
    }
    const AnimalProfile* src = ALL_PROFILES[profileIdx];
    if (!src) return false;

    CustomProfile cp;
    fromAnimalProfile(*src, cp);
    _customProfiles[_customCount] = cp;
    _profileOverrideMap[profileIdx] = (int8_t)_customCount;
    _customCount++;

    saveCustomProfiles();
    saveProfileOverrideMap();
    Serial.print("[STORAGE] Override olusturuldu: ");
    Serial.print(src->name);
    Serial.print(" -> custom[");
    Serial.print(_profileOverrideMap[profileIdx]);
    Serial.println("]");
    return true;
}

bool StorageService::clearProfileOverride(uint8_t profileIdx) {
    if (profileIdx >= PROFILE_COUNT) return false;
    int8_t cidx = _profileOverrideMap[profileIdx];
    if (cidx < 0) return false;
    // CustomProfile slotunu sil; deleteCustomProfile zaten map kompakt eder
    return deleteCustomProfile((uint8_t)cidx);
}

bool StorageService::hasProfileOverride(uint8_t profileIdx) const {
    if (profileIdx >= PROFILE_COUNT) return false;
    return _profileOverrideMap[profileIdx] >= 0;
}

int8_t StorageService::getOverrideCustomIdx(uint8_t profileIdx) const {
    if (profileIdx >= PROFILE_COUNT) return -1;
    return _profileOverrideMap[profileIdx];
}

bool StorageService::getOverridenProfile(uint8_t profileIdx, AnimalProfile &out) const {
    int8_t cidx = getOverrideCustomIdx(profileIdx);
    if (cidx < 0 || cidx >= (int8_t)_customCount) return false;
    // toAnimalProfile non-const, ama davranısı saf okuma — const_cast guvenli
    const_cast<StorageService*>(this)->toAnimalProfile(_customProfiles[cidx], out);
    // CO2 ve defaultEggCount alanlari CustomProfile'da yok, hazir profilden al
    if (profileIdx < PROFILE_COUNT && ALL_PROFILES[profileIdx]) {
        out.defaultEggCount = ALL_PROFILES[profileIdx]->defaultEggCount;
        out.co2Low          = ALL_PROFILES[profileIdx]->co2Low;
        out.co2High         = ALL_PROFILES[profileIdx]->co2High;
        out.co2Critical     = ALL_PROFILES[profileIdx]->co2Critical;
    }
    return true;
}

void StorageService::compactOverrideMapAfterDelete(uint8_t deletedCustomIdx) {
    bool changed = false;
    for (uint8_t i = 0; i < PROFILE_COUNT; i++) {
        int8_t v = _profileOverrideMap[i];
        if (v < 0) continue;
        if (v == (int8_t)deletedCustomIdx) {
            _profileOverrideMap[i] = -1;
            changed = true;
        } else if (v > (int8_t)deletedCustomIdx) {
            _profileOverrideMap[i] = v - 1;
            changed = true;
        }
    }
    if (changed) saveProfileOverrideMap();
}

void StorageService::loadProfileOverrideMap() {
    // v4 schema: ayri NVS anahtari "ovrMap" (binary blob, PROFILE_COUNT byte).
    // Yoksa hepsi -1 baslar (constructor zaten oyle yaptı).
    size_t len = sizeof(_profileOverrideMap);
    size_t read = _prefs.getBytes("ovrMap", _profileOverrideMap, len);
    if (read != len) {
        // Anahtarlar yok veya bozuk → varsayilan -1
        for (uint8_t i = 0; i < PROFILE_COUNT; i++) _profileOverrideMap[i] = -1;
        Serial.println("[STORAGE] Override map: yeni baslatildi (hepsi -1)");
    } else {
        // Sanity: deger _customCount sinirini asarsa -1'e dusur
        for (uint8_t i = 0; i < PROFILE_COUNT; i++) {
            if (_profileOverrideMap[i] >= (int8_t)_customCount) {
                _profileOverrideMap[i] = -1;
            }
        }
        uint8_t cnt = 0;
        for (uint8_t i = 0; i < PROFILE_COUNT; i++)
            if (_profileOverrideMap[i] >= 0) cnt++;
        Serial.print("[STORAGE] Override map yuklendi (");
        Serial.print(cnt);
        Serial.println(" override aktif)");
    }
}

void StorageService::saveProfileOverrideMap() {
    _prefs.putBytes("ovrMap", _profileOverrideMap, sizeof(_profileOverrideMap));
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
