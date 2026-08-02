#include "EggIRSensor.h"
#include "I2CMux.h"

// =====================================================================
//  MLX90614ESF-BCC - Yerel yumurta IR termometresi
//  Grove 8 kanal I2C MUX Kanal 4 (MUX_CH_EGG_IR) uzerinden okunur.
//
//  SMBus okuma dizisi (datasheet 8.4.3):
//    START -> [addr<<1 | W] -> [komut/RAM reg] ->
//    REPEATED START -> [addr<<1 | R] -> [LSB] [MSB] [PEC] -> STOP
//  PEC, yukaridaki tum byte'lar uzerinden CRC-8 (poly 0x07) ile hesaplanir.
// =====================================================================

EggIRSensor::EggIRSensor()
    : _objTemp(0.0f)
    , _ambTemp(0.0f)
    , _offset(EGG_IR_TEMP_OFFSET)
    , _ready(false)
    , _valid(false)
    , _failCount(0)
    , _lastReadMs(0)
{
}

// ---------------------------------------------------------------------
//  SMBus PEC - CRC-8, poly 0x07, init 0x00
// ---------------------------------------------------------------------
uint8_t EggIRSensor::computePEC(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0x00;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ MLX90614_PEC_POLY)
                               : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

float EggIRSensor::rawToCelsius(uint16_t raw) {
    return (float)raw * MLX90614_RAW_LSB - MLX90614_KELVIN_OFFSET;
}

// ---------------------------------------------------------------------
//  Tek bir RAM registerini oku (kanal secimi cagiran tarafta yapilir)
// ---------------------------------------------------------------------
bool EggIRSensor::readRaw(uint8_t reg, uint16_t &raw) {
    Wire.beginTransmission(MLX90614_ADDR);
    Wire.write(reg);
    // endTransmission(false) -> STOP gonderme, repeated start birak
    if (Wire.endTransmission(false) != 0) return false;

    if (Wire.requestFrom((int)MLX90614_ADDR, (int)3) != 3) return false;
    uint8_t lo  = Wire.read();
    uint8_t hi  = Wire.read();
    uint8_t pec = Wire.read();

    // PEC dogrulama: [W adres][komut][R adres][LSB][MSB]
    const uint8_t frame[5] = {
        (uint8_t)(MLX90614_ADDR << 1),
        reg,
        (uint8_t)((MLX90614_ADDR << 1) | 0x01),
        lo,
        hi
    };
    if (computePEC(frame, 5) != pec) return false;

    raw = ((uint16_t)hi << 8) | lo;
    return true;
}

// ---------------------------------------------------------------------
//  Baslatma
// ---------------------------------------------------------------------
bool EggIRSensor::begin() {
    I2CMux::begin();   // idempotent - baska modul cagirdiysa tekrar acmaz
    if (!I2CMux::isReady()) {
        Serial.println("[EGG-IR] MUX hazir degil, MLX90614 atlandi");
        _ready = false;
        return false;
    }

    if (!I2CMux::selectChannel(MUX_CH_EGG_IR)) {
        Serial.printf("[EGG-IR] MUX kanal %d secilemedi\n", MUX_CH_EGG_IR);
        _ready = false;
        return false;
    }
    delay(10);

    Wire.beginTransmission(MLX90614_ADDR);
    if (Wire.endTransmission() != 0) {
        Serial.printf("[EGG-IR] MLX90614 BULUNAMADI CH%d addr=0x%02X\n",
                      MUX_CH_EGG_IR, MLX90614_ADDR);
        _ready = false;
        return false;
    }

    _ready = true;
    if (forceRead()) {
        Serial.printf("[EGG-IR] MLX90614 OK CH%d yumurta=%.2fC ortam=%.2fC\n",
                      MUX_CH_EGG_IR, _objTemp, _ambTemp);
    } else {
        Serial.printf("[EGG-IR] MLX90614 bulundu ama ilk olcum basarisiz (CH%d)\n",
                      MUX_CH_EGG_IR);
    }
    return true;
}

// ---------------------------------------------------------------------
//  Okuma
// ---------------------------------------------------------------------
bool EggIRSensor::forceRead() {
    if (!_ready) return false;

    if (!I2CMux::selectChannel(MUX_CH_EGG_IR)) {
        _valid = false;
        return false;
    }
    delay(5);   // MUX gecis kararliligi (diger sensorlerle ayni yaklasim)

    _lastReadMs = millis();

    uint16_t rawObj = 0;
    uint16_t rawAmb = 0;
    bool okObj = readRaw(MLX90614_REG_TOBJ1, rawObj);
    bool okAmb = readRaw(MLX90614_REG_TA,    rawAmb);

    // Nesne okumasi zorunlu; hata bayragi set ise deger cop demektir
    if (!okObj || (rawObj & MLX90614_ERROR_FLAG)) {
        if (_failCount < 255) _failCount++;
        if (_failCount >= EGG_IR_FAIL_LIMIT) _valid = false;
        DEBUG_PRINTF("[EGG-IR] Okuma hatasi (fail=%u)\n", _failCount);
        return false;
    }

    float obj = rawToCelsius(rawObj) + _offset;

    // Kulucka ortami icin makul aralik disi -> gecersiz say
    if (obj < EGG_IR_VALID_TEMP_MIN || obj > EGG_IR_VALID_TEMP_MAX) {
        if (_failCount < 255) _failCount++;
        if (_failCount >= EGG_IR_FAIL_LIMIT) _valid = false;
        DEBUG_PRINTF("[EGG-IR] Aralik disi: %.2fC (fail=%u)\n", obj, _failCount);
        return false;
    }

    _objTemp   = obj;
    _failCount = 0;
    _valid     = true;

    // Ortam sicakligi bilgi amaclidir; okunamazsa son deger korunur
    if (okAmb && !(rawAmb & MLX90614_ERROR_FLAG)) {
        _ambTemp = rawToCelsius(rawAmb);
    }

    DEBUG_PRINTF("[EGG-IR] yumurta=%.2fC ortam=%.2fC\n", _objTemp, _ambTemp);
    return true;
}

bool EggIRSensor::update() {
    if (!_ready) return false;

    uint32_t now = millis();
    if (_lastReadMs != 0 && (now - _lastReadMs) < EGG_IR_READ_INTERVAL_MS) {
        return false;   // okuma araligi dolmadi
    }
    return forceRead();
}
