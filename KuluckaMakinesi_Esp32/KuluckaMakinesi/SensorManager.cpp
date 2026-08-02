#include "SensorManager.h"
#include <Wire.h>
#include "I2CMux.h"

// ============================================================
// SensorManager - Karma Dual Sensor Fuzyon
// SHT40 (MUX CH1, Birincil) + SHT30 (MUX CH2, Ikincil)
// SHT40: Sensirion_I2C_SHT4x kutuphanesi
// SHT30: Wire protokolu ile direkt I2C
// ============================================================

SensorManager::SensorManager()
    : _filteredTemp(0.0f)
    , _filteredHum(0.0f)
    , _rawTemp(0.0f)
    , _rawHum(0.0f)
    , _sensorOK(false)
    , _sensor1OK(false)
    , _sensor2OK(false)
    , _sensor1Present(false)
    , _sensor2Present(false)
    , _failCount(0)
    , _failCount1(0)
    , _failCount2(0)
    , _divergeCount(0)
    , _fusionStatus(FUSION_NONE)
    , _lastValidReadTime(0)
    , _failoverBlendCounter(0)
{
    _data1.valid = false;
    _data1.temperature = 0.0f;
    _data1.humidity = 0.0f;
    _data2.valid = false;
    _data2.temperature = 0.0f;
    _data2.humidity = 0.0f;
    memset(&_cal, 0, sizeof(_cal));
}

// ====================================================================
// =====================  YARDIMCI FONKSİYONLAR  ======================
// ====================================================================

void SensorManager::selectMuxChannel() {
    I2CMux::selectChannel(MUX_CH_SENSOR);   // CH1 - SHT40
}

void SensorManager::selectMuxChannel2() {
    I2CMux::selectChannel(MUX_CH_SENSOR_2); // CH2 - SHT30
}

// ====================================================================
// =====================  SHT40 (Birincil, CH1) - Wire Direkt  ========
// SHT40 protokolü: tek byte komut gönder, 6 byte oku (T MSB, T LSB, CRC, H MSB, H LSB, CRC)
// ====================================================================

uint8_t SensorManager::sht40CRC8(const uint8_t *data, uint8_t len) {
    uint8_t crc = SHT40_CRC_INIT;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ SHT40_CRC_POLY) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

bool SensorManager::sht40Read(float &temp, float &hum) {
    I2CMux::selectChannel(MUX_CH_SENSOR);  // CH1
    delay(5);  // MUX gecis kararliligi

    // High precision measurement komutu (0xFD) gönder
    Wire.beginTransmission(SHT40_ADDR);
    Wire.write(SHT40_CMD_MEAS_HI);
    if (Wire.endTransmission() != 0) return false;

    delay(SHT40_MEAS_DELAY_HI_MS);

    // 6 byte oku: T_MSB, T_LSB, T_CRC, H_MSB, H_LSB, H_CRC
    uint8_t buf[6];
    uint8_t got = Wire.requestFrom((int)SHT40_ADDR, (int)6);
    if (got != 6) return false;
    for (uint8_t i = 0; i < 6; i++) buf[i] = Wire.read();

    // CRC dogrulama
    if (sht40CRC8(&buf[0], 2) != buf[2]) return false;
    if (sht40CRC8(&buf[3], 2) != buf[5]) return false;

    // Donusum formulu (SHT40 datasheet)
    uint16_t rawT = ((uint16_t)buf[0] << 8) | buf[1];
    uint16_t rawH = ((uint16_t)buf[3] << 8) | buf[4];
    temp = -45.0f + 175.0f * ((float)rawT / 65535.0f);
    hum  = -6.0f  + 125.0f * ((float)rawH / 65535.0f);
    // Nem sinirla (0-100)
    if (hum < 0.0f)   hum = 0.0f;
    if (hum > 100.0f) hum = 100.0f;
    return true;
}

// ====================================================================
// =====================  SHT30 (Ikincil, CH2)  =======================
// Wire ile direkt I2C (clock stretching ENABLED)
// ====================================================================

uint8_t SensorManager::sht30CRC8(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0xFF;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

bool SensorManager::sht30ReadOnChannel(uint8_t muxCh, float &temp, float &hum) {
    I2CMux::selectChannel(muxCh);
    delay(5);  // MUX gecis kararliligi

    Wire.beginTransmission(SHT30_ADDR);
    Wire.write((uint8_t)(SHT30_CMD_MEAS_HI >> 8));
    Wire.write((uint8_t)(SHT30_CMD_MEAS_HI & 0xFF));
    if (Wire.endTransmission() != 0) return false;

    delay(SHT30_MEAS_DELAY_MS);

    uint8_t buf[6];
    uint8_t got = Wire.requestFrom((int)SHT30_ADDR, (int)6);
    if (got != 6) return false;
    for (uint8_t i = 0; i < 6; i++) buf[i] = Wire.read();

    if (sht30CRC8(&buf[0], 2) != buf[2]) return false;
    if (sht30CRC8(&buf[3], 2) != buf[5]) return false;

    uint16_t rawT = ((uint16_t)buf[0] << 8) | buf[1];
    uint16_t rawH = ((uint16_t)buf[3] << 8) | buf[4];
    temp = -45.0f + 175.0f * ((float)rawT / 65535.0f);
    hum  = 100.0f * ((float)rawH / 65535.0f);
    return true;
}

// ====================================================================
// =====================  BAŞLATMA  ===================================
// ====================================================================

bool SensorManager::begin() {
    I2CMux::begin();
    if (!I2CMux::isReady()) {
        Serial.println("[SENSOR] MUX HATA! (bulunamadi)");
        _sensorOK = false;
        return false;
    }

    // --- SHT40 #1 (MUX CH1) - Wire direkt ---
    I2CMux::selectChannel(MUX_CH_SENSOR);
    delay(10);

    // Adres kontrolu
    Wire.beginTransmission(SHT40_ADDR);
    uint8_t err1 = Wire.endTransmission();
    _sensor1Present = (err1 == 0);   // takili mi (arizali olmasindan ayri bilgi)
    if (err1 == 0) {
        float t1, h1;
        _sensor1OK = sht40Read(t1, h1);
        if (_sensor1OK) {
            _data1 = {t1, h1, true};
            Serial.printf("[SHT40] OK  CH%d T=%.2fC H=%.2f%%\n", MUX_CH_SENSOR, t1, h1);
        } else {
            Serial.printf("[SHT40] Ilk olcum basarisiz (CH%d)\n", MUX_CH_SENSOR);
        }
    } else {
        Serial.printf("[SHT40] BULUNAMADI CH%d addr=0x%02X err=%u\n",
                      MUX_CH_SENSOR, SHT40_ADDR, err1);
        _sensor1OK = false;
    }

    // --- SHT30 #2 (MUX CH2) - Wire protokolu ---
    I2CMux::selectChannel(MUX_CH_SENSOR_2);
    delay(10);
    Wire.beginTransmission(SHT30_ADDR);
    uint8_t err2 = Wire.endTransmission();
    _sensor2Present = (err2 == 0);   // takili mi (arizali olmasindan ayri bilgi)
    if (err2 == 0) {
        float t2, h2;
        _sensor2OK = sht30ReadOnChannel(MUX_CH_SENSOR_2, t2, h2);
        if (_sensor2OK) {
            _data2 = {t2, h2, true};
            Serial.printf("[SHT30] OK  CH%d T=%.2fC H=%.2f%%\n", MUX_CH_SENSOR_2, t2, h2);
        } else {
            Serial.printf("[SHT30] Ilk olcum basarisiz (CH%d)\n", MUX_CH_SENSOR_2);
        }
    } else {
        Serial.printf("[SHT30] BULUNAMADI CH%d addr=0x%02X err=%u\n",
                      MUX_CH_SENSOR_2, SHT30_ADDR, err2);
        _sensor2OK = false;
    }

    _sensorOK = _sensor1OK || _sensor2OK;

    if (_sensor1OK && _sensor2OK)
        Serial.println("[FUZYON] SHT40+SHT30 cift sensor aktif -> fuzyon modu");
    else if (_sensor1OK)
        Serial.println("[FUZYON] Sadece SHT40 aktif (tek sensor)");
    else if (_sensor2OK)
        Serial.println("[FUZYON] Sadece SHT30 aktif (tek sensor)");
    else
        Serial.println("[FUZYON] UYARI: Hicbir sensor bulunamadi!");

    return _sensorOK;
}

// ====================================================================
// =====================  FUZYON  =====================================
// ====================================================================

void SensorManager::fusionCombine() {
    bool v1 = _data1.valid;
    bool v2 = _data2.valid;

    if (v1 && v2) {
        float dT = fabs(_data1.temperature - _data2.temperature);
        float dH = fabs(_data1.humidity - _data2.humidity);

        if (dT <= FUSION_TEMP_TOLERANCE && dH <= FUSION_HUM_TOLERANCE) {
            // Uyumlu -> ortalama al
            _rawTemp = (_data1.temperature + _data2.temperature) / 2.0f;
            _rawHum  = (_data1.humidity + _data2.humidity) / 2.0f;
            _fusionStatus = FUSION_BOTH_OK;
            _divergeCount = 0;
        } else {
            // Uyumsuz -> onceki filtreye yakin olani tercih et
            _divergeCount++;
            bool prefer1 = true;
            if (_filteredTemp > 0.0f) {
                float d1 = fabs(_data1.temperature - _filteredTemp);
                float d2 = fabs(_data2.temperature - _filteredTemp);
                prefer1 = (d1 <= d2);
            }
            // Sticky penalty: hata sayisi fazla olan sensoru disfavour et
            if (_failCount1 >= _failCount2 + 2)      prefer1 = false;
            else if (_failCount2 >= _failCount1 + 2) prefer1 = true;

            _rawTemp = prefer1 ? _data1.temperature : _data2.temperature;
            _rawHum  = prefer1 ? _data1.humidity    : _data2.humidity;
            _fusionStatus = FUSION_DIVERGED;

            if (_divergeCount == FUSION_DIVERGE_COUNT) {
                Serial.printf("[FUZYON] UYARI: Sensorler uyumsuz! dT=%.2f dH=%.2f\n", dT, dH);
                Serial.printf("[FUZYON]   SHT40: T=%.2f H=%.2f  |  SHT30: T=%.2f H=%.2f\n",
                              _data1.temperature, _data1.humidity,
                              _data2.temperature, _data2.humidity);
            }
        }
    } else if (v1) {
        _rawTemp = _data1.temperature;
        _rawHum  = _data1.humidity;
        _fusionStatus = FUSION_ONLY_S1;
        _divergeCount = 0;
        if (_failCount2 == 1)
            Serial.println("[FUZYON] UYARI: SHT30 ariza! Sadece SHT40 kullaniliyor.");
    } else if (v2) {
        _rawTemp = _data2.temperature;
        _rawHum  = _data2.humidity;
        _fusionStatus = FUSION_ONLY_S2;
        _divergeCount = 0;
        if (_failCount1 == 1)
            Serial.println("[FUZYON] UYARI: SHT40 ariza! Sadece SHT30 kullaniliyor.");
    } else {
        _fusionStatus = FUSION_NONE;
        _divergeCount = 0;
    }
}

// ====================================================================
// =====================  ANA OKUMA  ==================================
// ====================================================================

bool SensorManager::readAll() {
    // --- SHT40 oku (CH1, Sensirion kutuphanesi) + kalibrasyon ---
    float t1, h1;
    bool ok1 = sht40Read(t1, h1);
    if (ok1) {
        t1 += _cal.tempOffset1;
        h1 += _cal.humOffset1;
        _data1.temperature = t1;
        _data1.humidity    = h1;
        _data1.valid = (t1 >= SENSOR_VALID_TEMP_MIN && t1 <= SENSOR_VALID_TEMP_MAX &&
                        h1 >= SENSOR_VALID_HUM_MIN  && h1 <= SENSOR_VALID_HUM_MAX);
    } else {
        _data1.valid = false;
    }

    // --- SHT30 oku (CH2, Wire protokolu) + kalibrasyon ---
    float t2, h2;
    bool ok2 = sht30ReadOnChannel(MUX_CH_SENSOR_2, t2, h2);
    if (ok2) {
        t2 += _cal.tempOffset2;
        h2 += _cal.humOffset2;
        _data2.temperature = t2;
        _data2.humidity    = h2;
        _data2.valid = (t2 >= SENSOR_VALID_TEMP_MIN && t2 <= SENSOR_VALID_TEMP_MAX &&
                        h2 >= SENSOR_VALID_HUM_MIN  && h2 <= SENSOR_VALID_HUM_MAX);
    } else {
        _data2.valid = false;
    }

    // Per-sensor hata sayaci
    _sensor1OK = _data1.valid;
    _sensor2OK = _data2.valid;
    _failCount1 = _data1.valid ? 0 : (_failCount1 + 1);
    _failCount2 = _data2.valid ? 0 : (_failCount2 + 1);

    _sensorOK = _data1.valid || _data2.valid;
    if (!_sensorOK) {
        _failCount++;
        _fusionStatus = FUSION_NONE;
        return false;
    }
    _failCount = 0;
    _lastValidReadTime = millis();

    FusionStatus prevStatus = _fusionStatus;
    fusionCombine();

    // --- Failover yumusatma ---
    bool fusionChanged = (prevStatus != _fusionStatus) &&
                         (prevStatus != FUSION_NONE) &&
                         (_fusionStatus != FUSION_NONE);
    if (fusionChanged) {
        _failoverBlendCounter = FAILOVER_BLEND_COUNT;
        Serial.printf("[SENSOR] Failover gecisi -> %s (yumusatma %d okuma)\n",
                       getFusionStatusStr(), FAILOVER_BLEND_COUNT);
    }

    // Spike + EMA filtre
    float alpha = EMA_ALPHA;
    if (_failoverBlendCounter > 0) {
        alpha = EMA_ALPHA * 0.3f;
        _failoverBlendCounter--;
    }

    float tempToFilter = _rawTemp;
    float humToFilter  = _rawHum;
    if (_filteredTemp > 0.0f) {
        if (isSpikeTemp(_rawTemp, _filteredTemp)) tempToFilter = _filteredTemp;
        if (isSpikeHum(_rawHum, _filteredHum))    humToFilter  = _filteredHum;
    }
    if (_filteredTemp == 0.0f) {
        _filteredTemp = tempToFilter;
        _filteredHum  = humToFilter;
    } else {
        _filteredTemp = applyEMA(tempToFilter, _filteredTemp, alpha);
        _filteredHum  = applyEMA(humToFilter,  _filteredHum,  alpha);
    }
    return true;
}

// ==================== Getter'lar ====================

float   SensorManager::getTemperature()    const { return _filteredTemp; }
float   SensorManager::getHumidity()       const { return _filteredHum;  }
float   SensorManager::getRawTemperature() const { return _rawTemp; }
float   SensorManager::getRawHumidity()    const { return _rawHum;  }
bool    SensorManager::isSensor1OK()       const { return _sensor1OK; }
bool    SensorManager::isSensor2OK()       const { return _sensor2OK; }
bool    SensorManager::isAnyValid()        const { return _data1.valid || _data2.valid; }
uint8_t SensorManager::getFailCount()      const { return _failCount; }
uint8_t SensorManager::getFailCount1()     const { return _failCount1; }
uint8_t SensorManager::getFailCount2()     const { return _failCount2; }
uint8_t SensorManager::getDivergeCount()   const { return _divergeCount; }

bool SensorManager::isStale() const {
    if (_lastValidReadTime == 0) return true;
    return (millis() - _lastValidReadTime) > SENSOR_STALE_TIMEOUT_MS;
}

// ==================== Kalibrasyon ====================

void SensorManager::setCalibration(const SensorCalibration &cal) {
    _cal = cal;
    Serial.printf("[SENSOR] Kalibrasyon: SHT40 T=%+.2f H=%+.2f | SHT30 T=%+.2f H=%+.2f\n",
                  _cal.tempOffset1, _cal.humOffset1, _cal.tempOffset2, _cal.humOffset2);
}

SensorCalibration SensorManager::getCalibration() const {
    return _cal;
}

FusionStatus SensorManager::getFusionStatus() const { return _fusionStatus; }

const char* SensorManager::getFusionStatusStr() const {
    switch (_fusionStatus) {
        case FUSION_BOTH_OK:  return "CIFT OK";
        case FUSION_DIVERGED: return "UYUMSUZ";
        case FUSION_ONLY_S1:  return "SADECE SHT40";
        case FUSION_ONLY_S2:  return "SADECE SHT30";
        case FUSION_NONE:     return "SENSOR YOK";
        default:              return "?";
    }
}

float SensorManager::getSensor1Temp() const { return _data1.temperature; }
float SensorManager::getSensor1Hum()  const { return _data1.humidity; }
float SensorManager::getSensor2Temp() const { return _data2.temperature; }
float SensorManager::getSensor2Hum()  const { return _data2.humidity; }

// ==================== Filtreler ====================

float SensorManager::applyEMA(float newVal, float oldVal, float alpha) {
    return alpha * newVal + (1.0f - alpha) * oldVal;
}
bool SensorManager::isSpikeTemp(float newVal, float oldVal) {
    return (fabs(newVal - oldVal) > SPIKE_THRESHOLD);
}
bool SensorManager::isSpikeHum(float newVal, float oldVal) {
    return (fabs(newVal - oldVal) > (SPIKE_THRESHOLD * 5.0f));
}
