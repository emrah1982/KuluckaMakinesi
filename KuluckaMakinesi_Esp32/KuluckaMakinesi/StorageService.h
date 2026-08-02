#ifndef STORAGE_SERVICE_H
#define STORAGE_SERVICE_H

#include <Arduino.h>
#include <Preferences.h>
#include "Config.h"
#include "AnimalProfiles.h"

#define MAX_CUSTOM_PROFILES 10
#define MAX_LOG_ENTRIES     200   // RAM'de tutulacak max log satiri

struct StoredSettings {
    double   kp;
    double   ki;
    double   kd;
    uint8_t  profileIndex;
    bool     isCustomProfile;
    float    targetTemp;
    float    humLow;
    float    humHigh;
    uint16_t eggCount;
    char     wifiSSID[33];
    char     wifiPass[65];
    char     yumurtaIP[40];  // Yumurta ESP32 IP adresi (bos = devre disi)
    bool     yumurtaEnabled; // Yumurta IR servisi aktif/pasif
};

// CustomProfile v2 — faz bazli turning/cooling/spray parametreleri
// NVS'de "cpSchemaVer" anahtari 2 olarak tutulur. V1 bulundugunda
// otomatik migrate edilir (yeni alanlar varsayilan degerlerle doldurulur).
struct CustomProfile {
    char    name[32];
    char    nameEN[32];
    uint8_t totalDays;
    uint8_t phaseCount;
    struct {
        uint8_t  startDay;
        uint8_t  endDay;
        float    temperature;
        float    tempEnd;             // Gradyan bitis sicakligi (0=sabit)
        float    humidityLow;
        float    humidityHigh;
        bool     turningEnabled;
        uint16_t turningIntervalMin;  // 0 = Config varsayilani
        uint8_t  turningDurationSec;  // 0 = aciden hesapla
        uint8_t  turningAngleDeg;     // 0 = Config varsayilani (TURNER_DEFAULT_ANGLE_DEG)
        bool     coolingEnabled;
        uint8_t  coolingDurationMin;
        uint8_t  coolingPerDay;       // 1 veya 2
        bool     sprayingEnabled;
        uint8_t  sprayingDurationSec;
        char     phaseName[16];
    } phases[4];
    bool    active;
};

struct LogEntry {
    unsigned long timestamp;  // millis/1000
    float  temp;
    float  hum;
    uint8_t heaterPWM;
    uint8_t fanPWM;
    char   phase[16];
};

class StorageService {
public:
    StorageService();

    void begin();

    bool beginSD();
    void updateSD();
    bool isSDReady() const;
    void setSDLoggingEnabled(bool enabled);
    bool isSDLoggingEnabled() const;
    void appendSDLog(uint32_t unixTime, const char* animalName, float temp, float hum);

    // Ayarlar (NVS)
    void saveSettings(const StoredSettings &s);
    StoredSettings loadSettings();
    void saveWiFi(const char* ssid, const char* pass);

    uint16_t getEggCount() const;

    // Ozel profiller (NVS blob)
    uint8_t getCustomProfileCount() const;
    CustomProfile getCustomProfile(uint8_t index) const;
    bool addCustomProfile(const CustomProfile &profile);
    bool updateCustomProfile(uint8_t index, const CustomProfile &profile);
    bool deleteCustomProfile(uint8_t index);
    String getCustomProfilesJSON() const;

    // Ozel profili AnimalProfile formatina cevir
    void toAnimalProfile(const CustomProfile &cp, AnimalProfile &ap);

    // ---- Profil Override (2026-04-25) ----
    // Hazir profiller (ALL_PROFILES) icin "kullanici tarafindan duzenlenmis"
    // versiyon. Her hazir profile bir CustomProfile slotu baglanir; hazir
    // profil dokunulmaz, override silinince fabrika ayari geri gelir.
    // Map -1 = override yok; >=0 = _customProfiles dizisindeki slot indexi.
    bool   cloneProfileToOverride(uint8_t profileIdx);   // Hazir profili klonla
    bool   clearProfileOverride(uint8_t profileIdx);     // Override'i kaldir
    bool   hasProfileOverride(uint8_t profileIdx) const; // Override var mi
    int8_t getOverrideCustomIdx(uint8_t profileIdx) const;  // -1 = yok
    // Override edilmis CustomProfile'i AnimalProfile formatinda dondurur
    bool   getOverridenProfile(uint8_t profileIdx, AnimalProfile &out) const;
    // Bir custom profil silindiginde override map'inde bu indexe bagli
    // tum girisler -1'e setlenir; daha buyuk indexler 1 azaltilir.
    void   compactOverrideMapAfterDelete(uint8_t deletedCustomIdx);

    // Veri loglama (RAM ring buffer)
    void logData(float temp, float hum, uint8_t heaterPWM, uint8_t fanPWM, const char* phase);
    String getLogCSV() const;
    void clearLog();
    unsigned long getLogSize() const;

private:
    Preferences      _prefs;
    CustomProfile    _customProfiles[MAX_CUSTOM_PROFILES];
    uint8_t          _customCount;

    bool             _sdReady;
    bool             _sdLoggingEnabled;

    uint16_t         _eggCount;

    // Log ring buffer (RAM)
    LogEntry         _logBuffer[MAX_LOG_ENTRIES];
    uint16_t         _logHead;
    uint16_t         _logCount;

    // Profil override map: idx hazir profil indexi (0..PROFILE_COUNT-1),
    // deger ise _customProfiles dizisindeki slot (-1 = override yok).
    int8_t           _profileOverrideMap[PROFILE_COUNT];

    // NVS profil islemleri
    void loadCustomProfiles();
    void saveCustomProfiles();
    // V1 (8 alanli faz) -> V3 (turning interval/duration/aci + cooling/spray)
    void migrateCustomProfilesV1ToV3();
    // V2 (14 alan, aci yok) -> V3 (15 alan, aci eklendi)
    void migrateCustomProfilesV2ToV3();
    // Profil override map NVS islemleri (v4 ile geldi)
    void loadProfileOverrideMap();
    void saveProfileOverrideMap();
    // CustomProfile uzerinden hazir profil bytelarini doldur
    void fromAnimalProfile(const AnimalProfile &ap, CustomProfile &cp);
};

#endif // STORAGE_SERVICE_H
