#include "CO2Sensor.h"
#include "I2CMux.h"
#include <Wire.h>

// SCD30 I2C adresi ve komutlari
#define SCD30_ADDR              0x61
#define SCD30_CMD_START_CONT    0x0010  // Surekli olcum baslat
#define SCD30_CMD_STOP_CONT     0x0104  // Surekli olcum durdur
#define SCD30_CMD_SET_INTERVAL  0x4600  // Olcum araligi (saniye)
#define SCD30_CMD_DATA_READY    0x0202  // Veri hazir mi
#define SCD30_CMD_READ_MEAS     0x0300  // Olcum oku
#define SCD30_CMD_AUTO_CAL      0x5306  // Otomatik kalibrasyon
#define SCD30_CMD_FORCE_CAL     0x5204  // Zorla kalibrasyon
#define SCD30_CMD_ALTITUDE      0x5102  // Rakım kompanzasyonu
#define SCD30_CMD_FIRMWARE      0xD100  // Firmware versiyonu

#define SCD30_MEAS_INTERVAL     2       // Olcum araligi (saniye)
#define CO2_READ_INTERVAL_MS    2000    // Okuma araligi (ms)

CO2Sensor::CO2Sensor()
    : _type(CO2_SENSOR_NONE)
    , _co2(0)
    , _temperature(0)
    , _humidity(0)
    , _ready(false)
    , _valid(false)
    , _lastReadTime(0)
{
}

bool CO2Sensor::begin() {
    _type = CO2_SENSOR_NONE;
    _ready = false;
    
    // MUX kanal 5'i sec (CO2 sensoru icin)
    if (!I2CMux::selectChannel(MUX_CH_CO2)) {
        DEBUG_PRINTLN("[CO2] MUX kanal secimi basarisiz");
        return false;
    }
    
    // SCD30 dene
    if (scd30Begin()) {
        _type = CO2_SENSOR_SCD30;
        _ready = true;
        DEBUG_PRINTLN("[CO2] SCD30 bulundu");
        return true;
    }
    
    DEBUG_PRINTLN("[CO2] Sensor bulunamadi");
    I2CMux::closeAll();
    return false;
}

bool CO2Sensor::scd30Begin() {
    // I2C adresi kontrol
    Wire.beginTransmission(SCD30_ADDR);
    if (Wire.endTransmission() != 0) {
        return false;
    }
    
    // Firmware versiyonu oku (sensor dogrulama)
    uint8_t fw[3];
    if (!scd30ReadRegister(SCD30_CMD_FIRMWARE, fw, 3)) {
        return false;
    }
    DEBUG_PRINTF("[CO2] SCD30 FW: %d.%d\n", fw[0], fw[1]);
    
    // Surekli olcum baslat (2 saniye aralikla)
    if (!scd30SendCommand(SCD30_CMD_SET_INTERVAL, SCD30_MEAS_INTERVAL)) {
        return false;
    }
    delay(10);
    
    // Basinc kompanzasyonu ile baslat (0 = devre disi)
    if (!scd30SendCommand(SCD30_CMD_START_CONT, 0)) {
        return false;
    }
    
    return true;
}

bool CO2Sensor::read() {
    if (!_ready || _type == CO2_SENSOR_NONE) {
        return false;
    }
    
    // Okuma araligi kontrolu
    if (millis() - _lastReadTime < CO2_READ_INTERVAL_MS) {
        return _valid;
    }
    
    // MUX kanal sec
    if (!I2CMux::selectChannel(MUX_CH_CO2)) {
        _valid = false;
        return false;
    }
    
    bool result = false;
    
    if (_type == CO2_SENSOR_SCD30) {
        result = scd30Read();
    }
    
    I2CMux::closeAll();
    _lastReadTime = millis();
    
    return result;
}

bool CO2Sensor::scd30Read() {
    // Veri hazir mi kontrol
    if (!scd30IsDataReady()) {
        return _valid;  // Onceki deger gecerli kalsin
    }
    
    // Olcum oku (18 byte: CO2, Temp, Hum - her biri 4 byte float + 2 byte CRC)
    uint8_t data[18];
    if (!scd30ReadRegister(SCD30_CMD_READ_MEAS, data, 18)) {
        _valid = false;
        return false;
    }
    
    // CRC kontrol ve float donusum
    // CO2 (byte 0-5)
    if (scd30CRC8(&data[0], 2) != data[2] || scd30CRC8(&data[3], 2) != data[5]) {
        _valid = false;
        return false;
    }
    uint32_t co2Raw = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                      ((uint32_t)data[3] << 8)  | (uint32_t)data[4];
    float co2Float;
    memcpy(&co2Float, &co2Raw, 4);
    _co2 = (uint16_t)co2Float;
    
    // Sicaklik (byte 6-11)
    if (scd30CRC8(&data[6], 2) != data[8] || scd30CRC8(&data[9], 2) != data[11]) {
        _valid = false;
        return false;
    }
    uint32_t tempRaw = ((uint32_t)data[6] << 24) | ((uint32_t)data[7] << 16) |
                       ((uint32_t)data[9] << 8)  | (uint32_t)data[10];
    memcpy(&_temperature, &tempRaw, 4);
    
    // Nem (byte 12-17)
    if (scd30CRC8(&data[12], 2) != data[14] || scd30CRC8(&data[15], 2) != data[17]) {
        _valid = false;
        return false;
    }
    uint32_t humRaw = ((uint32_t)data[12] << 24) | ((uint32_t)data[13] << 16) |
                      ((uint32_t)data[15] << 8)  | (uint32_t)data[16];
    memcpy(&_humidity, &humRaw, 4);
    
    // Gecerlilik kontrolu
    _valid = (_co2 >= 400 && _co2 <= 10000 &&
              _temperature >= -10 && _temperature <= 60 &&
              _humidity >= 0 && _humidity <= 100);
    
    return _valid;
}

bool CO2Sensor::scd30IsDataReady() {
    uint8_t data[3];
    if (!scd30ReadRegister(SCD30_CMD_DATA_READY, data, 3)) {
        return false;
    }
    return (data[1] == 1);  // data[0]=MSB, data[1]=LSB, data[2]=CRC
}

bool CO2Sensor::scd30SendCommand(uint16_t cmd) {
    Wire.beginTransmission(SCD30_ADDR);
    Wire.write((uint8_t)(cmd >> 8));
    Wire.write((uint8_t)(cmd & 0xFF));
    return (Wire.endTransmission() == 0);
}

bool CO2Sensor::scd30SendCommand(uint16_t cmd, uint16_t arg) {
    uint8_t data[2] = { (uint8_t)(arg >> 8), (uint8_t)(arg & 0xFF) };
    uint8_t crc = scd30CRC8(data, 2);
    
    Wire.beginTransmission(SCD30_ADDR);
    Wire.write((uint8_t)(cmd >> 8));
    Wire.write((uint8_t)(cmd & 0xFF));
    Wire.write(data[0]);
    Wire.write(data[1]);
    Wire.write(crc);
    return (Wire.endTransmission() == 0);
}

bool CO2Sensor::scd30ReadRegister(uint16_t cmd, uint8_t* data, uint8_t len) {
    Wire.beginTransmission(SCD30_ADDR);
    Wire.write((uint8_t)(cmd >> 8));
    Wire.write((uint8_t)(cmd & 0xFF));
    if (Wire.endTransmission() != 0) {
        return false;
    }
    
    delay(3);  // SCD30 komut isleme suresi
    
    Wire.requestFrom((uint8_t)SCD30_ADDR, len);
    if (Wire.available() < len) {
        return false;
    }
    
    for (uint8_t i = 0; i < len; i++) {
        data[i] = Wire.read();
    }
    
    return true;
}

uint8_t CO2Sensor::scd30CRC8(const uint8_t* data, uint8_t len) {
    uint8_t crc = 0xFF;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

uint16_t CO2Sensor::getCO2() const {
    return _co2;
}

float CO2Sensor::getTemperature() const {
    return _temperature;
}

float CO2Sensor::getHumidity() const {
    return _humidity;
}

bool CO2Sensor::isReady() const {
    return _ready;
}

bool CO2Sensor::isValid() const {
    return _valid;
}

CO2SensorType CO2Sensor::getType() const {
    return _type;
}

const char* CO2Sensor::getTypeName() const {
    switch (_type) {
        case CO2_SENSOR_SCD30: return "SCD30";
        case CO2_SENSOR_MHZ19: return "MH-Z19";
        default: return "Yok";
    }
}

void CO2Sensor::setAltitudeCompensation(uint16_t altitude) {
    if (_type != CO2_SENSOR_SCD30 || !_ready) return;
    
    I2CMux::selectChannel(MUX_CH_CO2);
    scd30SendCommand(SCD30_CMD_ALTITUDE, altitude);
    I2CMux::closeAll();
    DEBUG_PRINTF("[CO2] Rakim: %d m\n", altitude);
}

void CO2Sensor::forceRecalibration(uint16_t reference) {
    if (_type != CO2_SENSOR_SCD30 || !_ready) return;
    
    I2CMux::selectChannel(MUX_CH_CO2);
    scd30SendCommand(SCD30_CMD_FORCE_CAL, reference);
    I2CMux::closeAll();
    DEBUG_PRINTF("[CO2] Kalibrasyon: %d ppm\n", reference);
}

void CO2Sensor::setAutoCalibration(bool enable) {
    if (_type != CO2_SENSOR_SCD30 || !_ready) return;
    
    I2CMux::selectChannel(MUX_CH_CO2);
    scd30SendCommand(SCD30_CMD_AUTO_CAL, enable ? 1 : 0);
    I2CMux::closeAll();
    DEBUG_PRINTF("[CO2] Oto-kal: %s\n", enable ? "ACIK" : "KAPALI");
}
