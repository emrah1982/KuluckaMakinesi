#ifndef PERSISTENT_STORAGE_H
#define PERSISTENT_STORAGE_H

#include <Arduino.h>
#include <Preferences.h>

// ============================================================
// KALICI VERI SAKLAMA (NVS - Non-Volatile Storage)
// Elektrik kesintisine dayanikli veri saklama
// DS3231 RTC ile birlikte calisir
// ============================================================

struct IncubationState {
    uint8_t  profileIndex;      // Secili profil (0-8)
    uint32_t startTimestamp;    // Baslangic tarihi (Unix timestamp)
    bool     isRunning;         // Kulucka aktif mi
    bool     isPaused;          // Duraklatilmis miydi
    uint16_t elapsedDays;       // Son kaydedilen gun sayisi (RTC yedegi)
    uint32_t lastSaveTimestamp; // Son kayit zamani (gecen gun hesaplama icin)
    bool     valid;             // Veri gecerli mi
};

class PersistentStorage {
public:
    PersistentStorage();
    
    bool begin();
    
    // Kulucka durumu kaydet/yukle
    void saveIncubationState(uint8_t profileIndex, uint32_t startTimestamp,
                             bool isRunning, bool isPaused = false,
                             uint16_t elapsedDays = 0, uint32_t lastSaveTs = 0);
    // Sadece periyodik guncelleme icin (profil/startTime degismez)
    void updateIncubationProgress(uint16_t elapsedDays, uint32_t lastSaveTs, bool isPaused);
    IncubationState loadIncubationState();
    void clearIncubationState();
    
    // PID parametreleri kaydet/yukle
    void savePID(float kp, float ki, float kd);
    bool loadPID(float &kp, float &ki, float &kd);

    // Kullanici tarafindan secilen planlanan baslangic tarihi
    void     savePlannedStart(uint32_t ts);
    uint32_t loadPlannedStart();

    // Sensor kalibrasyon offset'leri kaydet/yukle
    void saveCalibration(float tOff1, float hOff1, float tOff2, float hOff2);
    bool loadCalibration(float &tOff1, float &hOff1, float &tOff2, float &hOff2);

    void     saveEggCount(uint16_t eggCount);
    uint16_t loadEggCount(uint16_t defaultEggCount = 100);

    // ---------- Wall Clock (gercek dunya zamani) ----------
    // Cihaz herhangi bir anda RTC'den gecerli zaman okuduktan sonra cagrilir.
    // Elektrik kesintisinde RTC pili de bittiyse en azindan son bilinen zaman
    // korunur ve millis offset ile yaklasik dogru zaman hesaplanabilir.
    void     saveLastKnownTime(uint32_t unixSec);
    uint32_t loadLastKnownTime();   // 0 = hic kaydedilmedi

private:
    Preferences _prefs;
    bool _initialized;
    
    static const char* NAMESPACE;
    static const char* KEY_PROFILE;
    static const char* KEY_START_TIME;
    static const char* KEY_RUNNING;
    static const char* KEY_VALID;
    static const char* KEY_KP;
    static const char* KEY_KI;
    static const char* KEY_KD;
    static const char* KEY_EGG_COUNT;
    static const char* KEY_ELAPSED_DAYS;
    static const char* KEY_LAST_SAVE_TS;
    static const char* KEY_IS_PAUSED;
    static const char* KEY_LAST_KNOWN_TIME;   // Wall clock fallback
};

#endif // PERSISTENT_STORAGE_H
