#include "RTCManager.h"
#include "I2CMux.h"

// ============================================================
// RTCManager — Gercek Dunya DS3231 RTC Surucusu
// ------------------------------------------------------------
// Bkz. RTCManager.h dokumani.
// ============================================================

RTCManager::RTCManager()
    : _initialized(false)
    , _useFallback(false)
    , _fallbackDays(0)
    , _fallbackStartMs(0)
    , _cachedTime((uint32_t)0)
    , _cacheReadMs(0)
    , _lastSuccessMs(0)
    , _status(RTC_STATE_UNKNOWN)
{
}

// ============ Grove 8 Kanal I2C MUX Kanal Secimi (CH0) ============
void RTCManager::selectMuxChannel() const {
    I2CMux::selectChannel(MUX_CH_RTC);
}

// ==================== Tarih Validasyonu ====================
bool RTCManager::isValidDate(const DateTime &dt) {
    uint16_t y = dt.year();
    if (y < RTC_VALID_YEAR_MIN || y > RTC_VALID_YEAR_MAX) return false;
    if (dt.month() < 1 || dt.month() > 12) return false;
    if (dt.day()   < 1 || dt.day()   > 31) return false;
    if (dt.hour()  > 23) return false;
    if (dt.minute() > 59) return false;
    if (dt.second() > 59) return false;
    return true;
}

// ==================== Retry Mekanizmali Okuma ====================
bool RTCManager::readRTCWithRetry(DateTime &out) const {
    if (!_initialized) return false;

    for (int attempt = 0; attempt < RTC_RETRY_COUNT; attempt++) {
        selectMuxChannel();
        DateTime dt = _rtc.now();
        // I2C hatasi durumunda DS3231 0xFF dolu byte donebilir -> year() taşar
        if (isValidDate(dt)) {
            out = dt;
            return true;
        }
        if (attempt < RTC_RETRY_COUNT - 1) {
            delay(RTC_RETRY_DELAY_MS);
        }
    }
    return false;
}

// ==================== Cache Tazeleme ====================
void RTCManager::refreshCache() const {
    DateTime fresh((uint32_t)0);
    if (readRTCWithRetry(fresh)) {
        _cachedTime    = fresh;
        _cacheReadMs   = millis();
        _lastSuccessMs = _cacheReadMs;
        // Status: zaten lostPower bayragi varsa BATTERY_LOW olarak isaretliyoruz (begin'de)
        if (_status != RTC_STATE_BATTERY_LOW) {
            _status = RTC_STATE_OK;
        }
    } else {
        // Okuma basarisiz — son cached zamani kullanmaya devam et
        // Status'u guncelle: uzun sure okuyamadıysak I2C_FAIL
        if (millis() - _lastSuccessMs > RTC_HEALTH_TIMEOUT_MS) {
            _status = RTC_STATE_I2C_FAIL;
        }
    }
}

// ==================== Baslangic ====================
bool RTCManager::begin() {
    I2CMux::begin();
    if (!I2CMux::isReady()) {
        Serial.println("[RTC] MUX HATA! (bulunamadi)");
        _initialized = false;
        _status = RTC_STATE_I2C_FAIL;
        return false;
    }

    // RTC kanalini sec ve baslat
    selectMuxChannel();
    _initialized = _rtc.begin();
    if (_initialized) {
        Serial.println("[RTC] DS3231 OK (MUX CH" + String(MUX_CH_RTC) + ")");

        // Pil durumunu kontrol et
        if (_rtc.lostPower()) {
            Serial.println("[RTC] UYARI: Pil bitmis, saat sifirlandi!");
            // Derleme zamanini fallback olarak yaz; sistem WiFi/NTP ile sonra duzeltilebilir
            _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
            _status = RTC_STATE_BATTERY_LOW;
        }

        // Ilk okumayi yap, validate et
        DateTime initial((uint32_t)0);
        if (readRTCWithRetry(initial)) {
            _cachedTime    = initial;
            _cacheReadMs   = millis();
            _lastSuccessMs = _cacheReadMs;
            // Eger lostPower olmadiysa OK
            if (_status != RTC_STATE_BATTERY_LOW) {
                _status = RTC_STATE_OK;
            }
            // setStartDate sonra cagrilana kadar baslangic = simdiki zaman
            _startDate = DateTime(initial.year(), initial.month(), initial.day(),
                                  initial.hour(), initial.minute(), initial.second());
        } else {
            // RTC yaniyor ama gecersiz veri donuyor
            Serial.println("[RTC] HATA: Okumalar gecersiz (year < 2024). Fallback'a gec.");
            _status = RTC_STATE_INVALID;
        }
    } else {
        Serial.println("[RTC] DS3231 HATA! (MUX CH" + String(MUX_CH_RTC) + ")");
        _status = RTC_STATE_I2C_FAIL;
    }
    return _initialized;
}

// ==================== Zaman Okuma (Cached) ====================
DateTime RTCManager::now() const {
    if (!_initialized || _useFallback) {
        // Fallback veya init basarisiz: 0 doner; cagiran tarafin baska kaynak kullanmasi gerekir
        if (_useFallback) {
            // Sahte millis tabanli "simdi" — sadece elapsed days hesabi icin
            unsigned long extraMs = millis() - _fallbackStartMs;
            uint32_t extraSec = (uint32_t)(extraMs / 1000UL);
            // Baslangic tarihi + gecen sure
            return _startDate + TimeSpan(extraSec);
        }
        return DateTime((uint32_t)0);
    }

    // Cache yeterince yeni mi? Degilse tazele
    if (millis() - _cacheReadMs >= RTC_CACHE_INTERVAL_MS) {
        refreshCache();
    }

    // Cache + millis offseti ile interpolate et (1 sn'lik blocklarda atlama olur ama yetiyor)
    unsigned long elapsedMs = millis() - _cacheReadMs;
    if (elapsedMs < RTC_CACHE_INTERVAL_MS) {
        return _cachedTime + TimeSpan((uint32_t)(elapsedMs / 1000UL));
    }
    return _cachedTime;
}

DateTime RTCManager::nowForced() const {
    if (!_initialized) return DateTime((uint32_t)0);
    refreshCache();
    return _cachedTime;
}

uint32_t RTCManager::getUnixTime() const {
    return (uint32_t)now().unixtime();
}

// ==================== Manuel Zaman Ayarlama (NTP/UI) ====================
bool RTCManager::setUnixTime(uint32_t unixSec) {
    if (unixSec == 0) return false;
    return setDateTime(DateTime(unixSec));
}

bool RTCManager::setDateTime(const DateTime &dt) {
    if (!_initialized) return false;
    if (!isValidDate(dt)) {
        Serial.println("[RTC] setDateTime: gecersiz tarih reddedildi");
        return false;
    }
    selectMuxChannel();
    _rtc.adjust(dt);
    // Cache'i tazele
    _cachedTime    = DateTime(dt.year(), dt.month(), dt.day(),
                              dt.hour(), dt.minute(), dt.second());
    _cacheReadMs   = millis();
    _lastSuccessMs = _cacheReadMs;
    _status = RTC_STATE_OK;   // basariyla set edildi -> BATTERY_LOW kalksin
    Serial.print("[RTC] Manuel zaman set edildi: ");
    Serial.println(dt.timestamp());
    return true;
}

// ==================== Gun Sayimi ====================
int RTCManager::getElapsedDays() const {
    // Fallback modu: RTC pili olmadan millis() tabanli gun sayimi
    if (_useFallback) {
        unsigned long extraMs = millis() - _fallbackStartMs;
        int extraDays = (int)(extraMs / 86400000UL);
        return (int)_fallbackDays + extraDays;
    }

    if (!_initialized) return 0;
    DateTime current = now();
    if (!isValidDate(current) || !isValidDate(_startDate)) return 0;
    TimeSpan diff = current - _startDate;
    return diff.days() + 1;
}

void RTCManager::setElapsedDaysFallback(uint16_t days) {
    _fallbackDays    = days;
    _fallbackStartMs = millis();
    _useFallback     = true;
    _status          = RTC_STATE_FALLBACK;
    Serial.printf("[RTC] Fallback modu: gun=%d (RTC pili olmus, millis tabanli)\n", days);
}

void RTCManager::setStartDate(const DateTime &date) {
    // DateTime'in user-provided copy ctor'i var; implicit copy assignment uyarisindan kacin
    _startDate = DateTime(date.year(), date.month(), date.day(),
                          date.hour(), date.minute(), date.second());
    Serial.print("[RTC] Baslangic tarihi: ");
    Serial.println(date.timestamp());
}

DateTime RTCManager::getStartDate() const {
    return _startDate;
}

// ==================== Format ====================
String RTCManager::getFormattedTime() const {
    DateTime dt = now();
    char buf[16];
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u",
             (unsigned)dt.hour(), (unsigned)dt.minute(), (unsigned)dt.second());
    return String(buf);
}

String RTCManager::getFormattedDate() const {
    DateTime dt = now();
    char buf[16];
    snprintf(buf, sizeof(buf), "%02u/%02u/%04u",
             (unsigned)dt.day(), (unsigned)dt.month(), (unsigned)dt.year());
    return String(buf);
}

// ==================== Modul Saglik ====================
float RTCManager::getModuleTemperature() {
    if (!_initialized) return 0.0f;
    selectMuxChannel();
    return _rtc.getTemperature();
}

const char* RTCManager::getStatusString() const {
    switch (_status) {
        case RTC_STATE_OK:           return "OK";
        case RTC_STATE_BATTERY_LOW:  return "PIL_ZAYIF";
        case RTC_STATE_INVALID:      return "GECERSIZ_TARIH";
        case RTC_STATE_I2C_FAIL:     return "I2C_HATA";
        case RTC_STATE_FALLBACK:     return "FALLBACK";
        case RTC_STATE_UNKNOWN:
        default:                     return "BILINMIYOR";
    }
}

unsigned long RTCManager::getMsSinceLastSync() const {
    if (_lastSuccessMs == 0) return 0xFFFFFFFFUL;   // hic sync olmadi
    return millis() - _lastSuccessMs;
}
