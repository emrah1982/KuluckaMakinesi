#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include "RTClib.h"
#include "Config.h"

// ============================================================
// RTCManager — Gercek Dunya DS3231 RTC Surucusu
// ------------------------------------------------------------
// Ozellikleri:
//   * Cached read: now() her cagrildiginda I2C trafigi yapmaz;
//     son okunan zaman + millis offset ile hesaplar (varsayilan
//     ~1 sn'de bir gerçek I2C okuma).
//   * Validity check: Okunan tarihin makul oldugunu kontrol eder
//     (year 2024-2099 araligi); gecersiz ise STATE_INVALID.
//   * Retry mechanism: I2C hatasinda otomatik 3x tekrar.
//   * Pil ariza tespiti: lostPower() + tarih validasyonu.
//   * Manuel set: setUnixTime() veya setDateTime() ile NTP/UI'dan
//     zaman ayarlama.
//   * Health status: STATE_OK / STATE_BATTERY_LOW / STATE_NO_BATTERY /
//     STATE_INVALID / STATE_I2C_FAIL / STATE_FALLBACK
//   * Fallback mode: RTC tamamen yoksa millis() tabanli gun sayimi.
// ============================================================

enum RTCStatus {
    RTC_STATE_UNKNOWN      = 0,
    RTC_STATE_OK           = 1,   // Calisiyor, zaman gecerli
    RTC_STATE_BATTERY_LOW  = 2,   // Pil zayif (lostPower=true ama zaman set edildi)
    RTC_STATE_INVALID      = 3,   // Zaman makul degil (year < 2024 vb)
    RTC_STATE_I2C_FAIL     = 4,   // I2C iletisim hatasi
    RTC_STATE_FALLBACK     = 5    // RTC yok, millis tabanli sayim
};

class RTCManager {
public:
    RTCManager();

    // ------------------ Lifecycle ------------------
    bool begin();

    // ------------------ Zaman okuma ------------------
    // Cached okuma: cok hizli, I2C trafigi yapmaz (1 sn'de bir tazelenir)
    DateTime now() const;
    // Forced okuma: I2C okumayi zorlar (cache bypass)
    DateTime nowForced() const;
    // Unix timestamp (saniye)
    uint32_t getUnixTime() const;

    // ------------------ Gun sayimi ------------------
    int  getElapsedDays() const;
    void setStartDate(const DateTime &date);
    DateTime getStartDate() const;

    // ------------------ Manuel zaman ayarlama ------------------
    // NTP/UI'dan zaman ayarlama. Validate eder, kayit eder, cache'i tazeler.
    bool setUnixTime(uint32_t unixSec);
    bool setDateTime(const DateTime &dt);

    // ------------------ Fallback (RTC pili olmus) ------------------
    void setElapsedDaysFallback(uint16_t days);

    // ------------------ Format ------------------
    String getFormattedTime() const;
    String getFormattedDate() const;

    // ------------------ Modul saglik ------------------
    float    getModuleTemperature();
    RTCStatus getStatus() const { return _status; }
    const char* getStatusString() const;
    bool      isHealthy() const { return _status == RTC_STATE_OK || _status == RTC_STATE_BATTERY_LOW; }
    // Son basarili I2C okumasindan bu yana ms (saglik gozetimi)
    unsigned long getMsSinceLastSync() const;

    // ------------------ Tarih validasyonu ------------------
    // Class-static helper: bir DateTime'in makul olup olmadigini dondurur
    static bool isValidDate(const DateTime &dt);

private:
    mutable RTC_DS3231 _rtc;
    DateTime   _startDate;
    bool       _initialized;
    bool       _useFallback;
    uint16_t   _fallbackDays;
    unsigned long _fallbackStartMs;

    // Cached okuma mekanizmasi
    mutable DateTime      _cachedTime;
    mutable unsigned long _cacheReadMs;     // millis() at cache time
    mutable unsigned long _lastSuccessMs;   // son basarili okuma zamani
    mutable RTCStatus     _status;

    void selectMuxChannel() const;
    bool readRTCWithRetry(DateTime &out) const;
    void refreshCache() const;
};

#endif // RTC_MANAGER_H
