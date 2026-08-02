#include "PersistentStorage.h"

// NVS namespace ve key tanimlari
const char* PersistentStorage::NAMESPACE   = "kulucka";
const char* PersistentStorage::KEY_PROFILE    = "profile";
const char* PersistentStorage::KEY_START_TIME = "startTime";
const char* PersistentStorage::KEY_RUNNING    = "running";
const char* PersistentStorage::KEY_VALID      = "valid";
const char* PersistentStorage::KEY_KP         = "kp";
const char* PersistentStorage::KEY_KI         = "ki";
const char* PersistentStorage::KEY_KD         = "kd";

PersistentStorage::PersistentStorage()
    : _initialized(false)
{
}

bool PersistentStorage::begin() {
    _initialized = _prefs.begin(NAMESPACE, false); // false = read/write mode
    
    if (_initialized) {
        Serial.println("[NVS] Kalici depolama baslatildi");
    } else {
        Serial.println("[NVS] HATA: Kalici depolama baslatilamadi!");
    }
    
    return _initialized;
}

// ==================== Kulucka Durumu ====================

void PersistentStorage::saveIncubationState(uint8_t profileIndex, uint32_t startTimestamp, bool isRunning) {
    if (!_initialized) return;
    
    _prefs.putUChar(KEY_PROFILE, profileIndex);
    _prefs.putULong(KEY_START_TIME, startTimestamp);
    _prefs.putBool(KEY_RUNNING, isRunning);
    _prefs.putBool(KEY_VALID, true);
    
    Serial.print("[NVS] Durum kaydedildi - Profil:");
    Serial.print(profileIndex);
    Serial.print(" Tarih:");
    Serial.print(startTimestamp);
    Serial.print(" Aktif:");
    Serial.println(isRunning ? "Evet" : "Hayir");
}

IncubationState PersistentStorage::loadIncubationState() {
    IncubationState state;
    state.valid = false;
    
    if (!_initialized) return state;
    
    // Gecerli veri var mi kontrol et
    if (!_prefs.getBool(KEY_VALID, false)) {
        Serial.println("[NVS] Kayitli kulucka durumu yok");
        return state;
    }
    
    state.profileIndex = _prefs.getUChar(KEY_PROFILE, 0);
    state.startTimestamp = _prefs.getULong(KEY_START_TIME, 0);
    state.isRunning = _prefs.getBool(KEY_RUNNING, false);
    state.valid = true;
    
    Serial.print("[NVS] Durum yuklendi - Profil:");
    Serial.print(state.profileIndex);
    Serial.print(" Tarih:");
    Serial.print(state.startTimestamp);
    Serial.print(" Aktif:");
    Serial.println(state.isRunning ? "Evet" : "Hayir");
    
    return state;
}

void PersistentStorage::clearIncubationState() {
    if (!_initialized) return;
    
    _prefs.putBool(KEY_VALID, false);
    Serial.println("[NVS] Kulucka durumu temizlendi");
}

// ==================== PID Parametreleri ====================

void PersistentStorage::savePID(float kp, float ki, float kd) {
    if (!_initialized) return;
    
    _prefs.putFloat(KEY_KP, kp);
    _prefs.putFloat(KEY_KI, ki);
    _prefs.putFloat(KEY_KD, kd);
    
    Serial.print("[NVS] PID kaydedildi - Kp:");
    Serial.print(kp);
    Serial.print(" Ki:");
    Serial.print(ki);
    Serial.print(" Kd:");
    Serial.println(kd);
}

bool PersistentStorage::loadPID(float &kp, float &ki, float &kd) {
    if (!_initialized) return false;
    
    // Varsayilan degerler
    kp = _prefs.getFloat(KEY_KP, 20.0f);
    ki = _prefs.getFloat(KEY_KI, 0.8f);
    kd = _prefs.getFloat(KEY_KD, 5.0f);
    
    Serial.print("[NVS] PID yuklendi - Kp:");
    Serial.print(kp);
    Serial.print(" Ki:");
    Serial.print(ki);
    Serial.print(" Kd:");
    Serial.println(kd);
    
    return true;
}
