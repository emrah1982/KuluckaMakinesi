#include "PersistentStorage.h"
 #include "Config.h"

// NVS namespace ve key tanimlari
const char* PersistentStorage::NAMESPACE   = "kulucka";
const char* PersistentStorage::KEY_PROFILE    = "profile";
const char* PersistentStorage::KEY_START_TIME = "startTime";
const char* PersistentStorage::KEY_RUNNING    = "running";
const char* PersistentStorage::KEY_VALID      = "valid";
const char* PersistentStorage::KEY_KP           = "kp";
const char* PersistentStorage::KEY_KI           = "ki";
const char* PersistentStorage::KEY_KD           = "kd";
const char* PersistentStorage::KEY_EGG_COUNT    = "eggCount";
const char* PersistentStorage::KEY_ELAPSED_DAYS = "elapsedD";
const char* PersistentStorage::KEY_LAST_SAVE_TS    = "lastSaveTs";
const char* PersistentStorage::KEY_IS_PAUSED       = "isPaused";
const char* PersistentStorage::KEY_LAST_KNOWN_TIME = "lkTime";

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

void PersistentStorage::saveIncubationState(uint8_t profileIndex, uint32_t startTimestamp,
                                             bool isRunning, bool isPaused,
                                             uint16_t elapsedDays, uint32_t lastSaveTs) {
    if (!_initialized) return;

    _prefs.putUChar(KEY_PROFILE,      profileIndex);
    _prefs.putULong(KEY_START_TIME,   startTimestamp);
    _prefs.putBool(KEY_RUNNING,       isRunning);
    _prefs.putBool(KEY_IS_PAUSED,     isPaused);
    _prefs.putUShort(KEY_ELAPSED_DAYS, elapsedDays);
    _prefs.putULong(KEY_LAST_SAVE_TS, lastSaveTs);
    _prefs.putBool(KEY_VALID,         true);

    Serial.printf("[NVS] Durum kaydedildi - Profil:%d Gun:%d Aktif:%s Paused:%s\n",
                  profileIndex, elapsedDays,
                  isRunning ? "E" : "H", isPaused ? "E" : "H");
}

void PersistentStorage::updateIncubationProgress(uint16_t elapsedDays, uint32_t lastSaveTs, bool isPaused) {
    if (!_initialized) return;
    _prefs.putUShort(KEY_ELAPSED_DAYS, elapsedDays);
    _prefs.putULong(KEY_LAST_SAVE_TS,  lastSaveTs);
    _prefs.putBool(KEY_IS_PAUSED,      isPaused);
    Serial.printf("[NVS] Ilerleme guncellendi - Gun:%d Paused:%s\n",
                  elapsedDays, isPaused ? "E" : "H");
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
    
    state.profileIndex      = _prefs.getUChar(KEY_PROFILE,       0);
    state.startTimestamp    = _prefs.getULong(KEY_START_TIME,    0);
    state.isRunning         = _prefs.getBool(KEY_RUNNING,        false);
    state.isPaused          = _prefs.getBool(KEY_IS_PAUSED,      false);
    state.elapsedDays       = _prefs.getUShort(KEY_ELAPSED_DAYS, 0);
    state.lastSaveTimestamp = _prefs.getULong(KEY_LAST_SAVE_TS,  0);
    state.valid = true;

    DEBUG_PRINTF("[NVS] Durum yuklendi - Profil:%d Start:%lu Gun:%d Paused:%s\n",
                  state.profileIndex, state.startTimestamp,
                  state.elapsedDays, state.isPaused ? "E" : "H");
    
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

    kp = _prefs.getFloat(KEY_KP, PID_DEFAULT_KP);
    ki = _prefs.getFloat(KEY_KI, PID_DEFAULT_KI);
    kd = _prefs.getFloat(KEY_KD, PID_DEFAULT_KD);

    // Mantikli aralik kontrolu - bozuk NVS korunmasi
    bool valid = (kp >= 0.1f && kp <= 200.0f &&
                  ki >= 0.0f && ki <= 50.0f &&
                  kd >= 0.0f && kd <= 100.0f);
    if (!valid) {
        Serial.printf("[NVS] PID degerleri aralik disi (Kp=%.2f Ki=%.2f Kd=%.2f) -> varsayilana donuluyor\n", kp, ki, kd);
        kp = PID_DEFAULT_KP;
        ki = PID_DEFAULT_KI;
        kd = PID_DEFAULT_KD;
        return false;
    }

    Serial.printf("[NVS] PID yuklendi - Kp:%.2f Ki:%.2f Kd:%.2f\n", kp, ki, kd);
    return true;
}

// ==================== Planlanan Baslangic Tarihi ====================

void PersistentStorage::savePlannedStart(uint32_t ts) {
    if (!_initialized) return;
    _prefs.putULong("plannedStart", ts);
    Serial.print("[NVS] Planlanan baslangic tarihi kaydedildi: ");
    Serial.println(ts);
}

uint32_t PersistentStorage::loadPlannedStart() {
    if (!_initialized) return 0;
    return _prefs.getULong("plannedStart", 0);
}

// ==================== Sensor Kalibrasyon ====================

void PersistentStorage::saveCalibration(float tOff1, float hOff1, float tOff2, float hOff2) {
    if (!_initialized) return;

    _prefs.putFloat("calT1", tOff1);
    _prefs.putFloat("calH1", hOff1);
    _prefs.putFloat("calT2", tOff2);
    _prefs.putFloat("calH2", hOff2);

    Serial.printf("[NVS] Kalibrasyon kaydedildi T1=%+.2f H1=%+.2f T2=%+.2f H2=%+.2f\n",
                  tOff1, hOff1, tOff2, hOff2);
}

bool PersistentStorage::loadCalibration(float &tOff1, float &hOff1, float &tOff2, float &hOff2) {
    if (!_initialized) return false;

    // NVS'de kalibrasyon var mi kontrol (varsayilan 0.0 = kalibrasyon yok)
    tOff1 = _prefs.getFloat("calT1", 0.0f);
    hOff1 = _prefs.getFloat("calH1", 0.0f);
    tOff2 = _prefs.getFloat("calT2", 0.0f);
    hOff2 = _prefs.getFloat("calH2", 0.0f);

    // Mantikli aralik kontrolu (max +-5°C / +-10%)
    bool valid = (fabs(tOff1) <= 5.0f && fabs(hOff1) <= 10.0f &&
                  fabs(tOff2) <= 5.0f && fabs(hOff2) <= 10.0f);
    if (!valid) {
        tOff1 = hOff1 = tOff2 = hOff2 = 0.0f;
        Serial.println("[NVS] Kalibrasyon degerleri aralik disi, sifirlandi");
        return false;
    }

    Serial.printf("[NVS] Kalibrasyon yuklendi T1=%+.2f H1=%+.2f T2=%+.2f H2=%+.2f\n",
                  tOff1, hOff1, tOff2, hOff2);
    return true;
}

void PersistentStorage::saveEggCount(uint16_t eggCount) {
    if (!_initialized) return;
    _prefs.putUShort(KEY_EGG_COUNT, eggCount);
}

uint16_t PersistentStorage::loadEggCount(uint16_t defaultEggCount) {
    if (!_initialized) return defaultEggCount;
    return _prefs.getUShort(KEY_EGG_COUNT, defaultEggCount);
}

// ---------- Wall Clock (gercek dunya zamani) ----------
// RTC pili bittiyse bile son bilinen zamani NVS'de saklarız. Cihaz sonra
// boot oldugunda bu zaman + millis offset ile yaklasik dogru zaman uretir.
// 1700000000UL = 2023-Kasim civari. Bu altindaki degerler Unix'in 1970'lerine
// gider — kabul etmeyiz (gecerli kuluckalar 2024+ olmalı).
void PersistentStorage::saveLastKnownTime(uint32_t unixSec) {
    if (!_initialized) return;
    if (unixSec < 1700000000UL) return;   // gecersiz, kaydetme
    _prefs.putULong(KEY_LAST_KNOWN_TIME, unixSec);
}

uint32_t PersistentStorage::loadLastKnownTime() {
    if (!_initialized) return 0;
    return _prefs.getULong(KEY_LAST_KNOWN_TIME, 0);
}
