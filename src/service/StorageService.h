#ifndef STORAGE_SERVICE_H
#define STORAGE_SERVICE_H

#include <Arduino.h>
#include <Preferences.h>
#include "../config/Config.h"
#include "../config/AnimalProfiles.h"

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
    char     wifiSSID[33];
    char     wifiPass[65];
};

struct CustomProfile {
    char    name[32];
    char    nameEN[32];
    uint8_t totalDays;
    uint8_t phaseCount;
    struct {
        uint8_t startDay;
        uint8_t endDay;
        float   temperature;
        float   humidityLow;
        float   humidityHigh;
        bool    turningEnabled;
        char    phaseName[16];
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

    // Ayarlar (NVS)
    void saveSettings(const StoredSettings &s);
    StoredSettings loadSettings();
    void saveWiFi(const char* ssid, const char* pass);

    // Ozel profiller (NVS blob)
    uint8_t getCustomProfileCount() const;
    CustomProfile getCustomProfile(uint8_t index) const;
    bool addCustomProfile(const CustomProfile &profile);
    bool updateCustomProfile(uint8_t index, const CustomProfile &profile);
    bool deleteCustomProfile(uint8_t index);
    String getCustomProfilesJSON() const;

    // Ozel profili AnimalProfile formatina cevir
    void toAnimalProfile(const CustomProfile &cp, AnimalProfile &ap);

    // Veri loglama (RAM ring buffer)
    void logData(float temp, float hum, uint8_t heaterPWM, uint8_t fanPWM, const char* phase);
    String getLogCSV() const;
    void clearLog();
    unsigned long getLogSize() const;

private:
    Preferences      _prefs;
    CustomProfile    _customProfiles[MAX_CUSTOM_PROFILES];
    uint8_t          _customCount;

    // Log ring buffer (RAM)
    LogEntry         _logBuffer[MAX_LOG_ENTRIES];
    uint16_t         _logHead;
    uint16_t         _logCount;

    // NVS profil islemleri
    void loadCustomProfiles();
    void saveCustomProfiles();
};

#endif // STORAGE_SERVICE_H
