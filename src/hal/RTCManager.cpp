#include "RTCManager.h"

RTCManager::RTCManager()
    : _initialized(false)
{
}

// ==================== TCA9548A MUX Kanal Secimi ====================

void RTCManager::selectMuxChannel() const {
    Wire.beginTransmission(MUX_ADDR);
    Wire.write(1 << MUX_CH_RTC);  // Kanal 0 = 0x01, Kanal 1 = 0x02, ...
    Wire.endTransmission();
}

// ==================== Baslangic ====================

bool RTCManager::begin() {
    // I2C baslat
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(I2C_CLOCK_SPEED);

    // MUX'u kontrol et
    Wire.beginTransmission(MUX_ADDR);
    uint8_t muxErr = Wire.endTransmission();
    if (muxErr != 0) {
        Serial.print("[RTC] TCA9548A MUX HATA! (0x");
        Serial.print(MUX_ADDR, HEX);
        Serial.print(") err=");
        Serial.println(muxErr);
    } else {
        Serial.println("[RTC] TCA9548A MUX OK");
    }

    // RTC kanalini sec ve baslat
    selectMuxChannel();
    _initialized = _rtc.begin();
    if (_initialized) {
        _startDate = _rtc.now();
        Serial.println("[RTC] DS3231 OK (MUX CH" + String(MUX_CH_RTC) + ")");

        if (_rtc.lostPower()) {
            Serial.println("[RTC] UYARI: Pil bitmis, saat sifirlandi!");
            _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
    } else {
        Serial.println("[RTC] DS3231 HATA! (MUX CH" + String(MUX_CH_RTC) + ")");
    }
    return _initialized;
}

// ==================== Zaman Okuma ====================

DateTime RTCManager::now() const {
    if (!_initialized) {
        return DateTime((uint32_t)0);
    }
    selectMuxChannel();
    return _rtc.now();
}

int RTCManager::getElapsedDays() const {
    if (!_initialized) return 0;

    selectMuxChannel();
    DateTime current = _rtc.now();
    TimeSpan diff = current - _startDate;
    return diff.days() + 1;
}

void RTCManager::setStartDate(const DateTime &date) {
    _startDate = date;
    Serial.print("[RTC] Baslangic tarihi: ");
    Serial.println(date.timestamp());
}

DateTime RTCManager::getStartDate() const {
    return _startDate;
}

String RTCManager::getFormattedTime() const {
    DateTime dt = now();
    char buf[9];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", dt.hour(), dt.minute(), dt.second());
    return String(buf);
}

String RTCManager::getFormattedDate() const {
    DateTime dt = now();
    char buf[11];
    snprintf(buf, sizeof(buf), "%02d/%02d/%04d", dt.day(), dt.month(), dt.year());
    return String(buf);
}

float RTCManager::getModuleTemperature() {
    if (!_initialized) return 0.0f;
    selectMuxChannel();
    return _rtc.getTemperature();
}
