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
    bool     valid;             // Veri gecerli mi
};

class PersistentStorage {
public:
    PersistentStorage();
    
    bool begin();
    
    // Kulucka durumu kaydet/yukle
    void saveIncubationState(uint8_t profileIndex, uint32_t startTimestamp, bool isRunning);
    IncubationState loadIncubationState();
    void clearIncubationState();
    
    // PID parametreleri kaydet/yukle
    void savePID(float kp, float ki, float kd);
    bool loadPID(float &kp, float &ki, float &kd);
    
    // Ozel profil kaydet/yukle (NVS kullanir)
    // Bu fonksiyonlar ayri bir dosyada implement edilebilir
    
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
};

#endif // PERSISTENT_STORAGE_H
