#include "SensorManager.h"

// ============================================================
// RS485 SHT20 Modbus RTU - MAX485 uzerinden
// Baglanti:
//   MAX485 RO  -> ESP32 GPIO16 (RX2)
//   MAX485 DI  -> ESP32 GPIO17 (TX2)
//   MAX485 DE+RE -> ESP32 GPIO4
//   SHT20 Sari (A) -> MAX485 A
//   SHT20 Beyaz(B) -> MAX485 B
// Protokol: Modbus RTU, 9600 8N1, Slave=0x01
// Register: 0x0001=Sicaklik(x10), 0x0002=Nem(x10)
// ============================================================

SensorManager::SensorManager()
    : _filteredTemp(0.0f)
    , _filteredHum(0.0f)
    , _rawTemp(0.0f)
    , _rawHum(0.0f)
    , _sensorOK(false)
    , _failCount(0)
{
    _data1.valid = false;
    _data1.temperature = 0.0f;
    _data1.humidity = 0.0f;
}

bool SensorManager::begin() {
    // RS485 yonlendirme pini
    pinMode(RS485_DE_RE_PIN, OUTPUT);
    rs485RxMode();

    // Serial2 baslat (UART2 - RS485 icin)
    Serial2.begin(MODBUS_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);

    // Modbus haberlesme testi - sicaklik register oku
    delay(100);
    uint8_t resp[9];
    _sensorOK = modbusReadRegister(MODBUS_SLAVE_ADDR, MODBUS_REG_TEMP, 1, resp, 7);

    if (_sensorOK) {
        Serial.println("[SENSOR] RS485 SHT20 (Modbus RTU) OK");
    } else {
        Serial.println("[SENSOR] RS485 SHT20 HATA! Baglanti/adres kontrol edin");
    }

    return _sensorOK;
}

// ==================== RS485 Yonlendirme ====================

void SensorManager::rs485TxMode() {
    digitalWrite(RS485_DE_RE_PIN, HIGH);  // DE=1, RE=1 -> TX modu
}

void SensorManager::rs485RxMode() {
    digitalWrite(RS485_DE_RE_PIN, LOW);   // DE=0, RE=0 -> RX modu
}

// ==================== Modbus CRC16 ====================

uint16_t SensorManager::modbusCRC16(const uint8_t *data, uint8_t len) {
    uint16_t crc = 0xFFFF;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

// ==================== Modbus Read Holding Registers (0x03) ====================

bool SensorManager::modbusReadRegister(uint8_t slaveAddr, uint16_t regAddr, uint16_t regCount, uint8_t *respBuf, uint8_t respLen) {
    // Istek frame'i olustur: [Addr][0x03][RegHi][RegLo][CountHi][CountLo][CRC_Lo][CRC_Hi]
    uint8_t frame[8];
    frame[0] = slaveAddr;
    frame[1] = 0x03;  // Function code: Read Holding Registers
    frame[2] = (regAddr >> 8) & 0xFF;
    frame[3] = regAddr & 0xFF;
    frame[4] = (regCount >> 8) & 0xFF;
    frame[5] = regCount & 0xFF;
    uint16_t crc = modbusCRC16(frame, 6);
    frame[6] = crc & 0xFF;        // CRC Lo
    frame[7] = (crc >> 8) & 0xFF; // CRC Hi

    // RX tamponunu temizle
    while (Serial2.available()) Serial2.read();

    // TX moduna gec ve gonder
    rs485TxMode();
    Serial2.write(frame, 8);
    Serial2.flush();  // TX tamamlanana kadar bekle

    // RX moduna gec
    rs485RxMode();

    // Yanit bekle
    unsigned long start = millis();
    uint8_t idx = 0;
    while ((millis() - start) < MODBUS_TIMEOUT && idx < respLen) {
        if (Serial2.available()) {
            respBuf[idx++] = Serial2.read();
        }
    }

    // Minimum yanit uzunlugu kontrol (addr + func + byteCount + data + crc)
    if (idx < respLen) {
        return false;
    }

    // Slave adres ve function code kontrol
    if (respBuf[0] != slaveAddr || respBuf[1] != 0x03) {
        return false;
    }

    // CRC kontrol
    uint16_t recvCrc = respBuf[idx - 2] | ((uint16_t)respBuf[idx - 1] << 8);
    uint16_t calcCrc = modbusCRC16(respBuf, idx - 2);
    if (recvCrc != calcCrc) {
        return false;
    }

    return true;
}

// ==================== Sensor Okuma ====================

bool SensorManager::readAll() {
    // Sicaklik oku (register 0x0001, 1 register, yanit 7 byte)
    uint8_t respTemp[7];
    bool tempOK = modbusReadRegister(MODBUS_SLAVE_ADDR, MODBUS_REG_TEMP, 1, respTemp, 7);

    // Nem oku (register 0x0002, 1 register, yanit 7 byte)
    delay(50);  // Slave'e nefes aldirma suresi
    uint8_t respHum[7];
    bool humOK = modbusReadRegister(MODBUS_SLAVE_ADDR, MODBUS_REG_HUM, 1, respHum, 7);

    if (tempOK && humOK) {
        // Veri byte'lari: respBuf[3]=Hi, respBuf[4]=Lo
        int16_t rawT = ((int16_t)respTemp[3] << 8) | respTemp[4];
        int16_t rawH = ((int16_t)respHum[3] << 8) | respHum[4];

        _data1.temperature = rawT / 10.0f;
        _data1.humidity    = rawH / 10.0f;
        _data1.valid       = (_data1.temperature > -40.0f && _data1.temperature < 80.0f &&
                              _data1.humidity >= 0.0f && _data1.humidity <= 100.0f);
    } else {
        _data1.valid = false;
    }

    _sensorOK = _data1.valid;

    if (!_data1.valid) {
        _failCount++;
        if (_failCount <= 3 || (_failCount % 10) == 0) {
            Serial.print("[SENSOR] RS485 SHT20 okuma hatasi! (");
            Serial.print(_failCount);
            Serial.println(")");
        }
        return false;
    }

    _failCount = 0;

    // Ham degerler
    _rawTemp = _data1.temperature;
    _rawHum  = _data1.humidity;

    // Spike filtresi (ilk okumada atla)
    float tempToFilter = _rawTemp;
    float humToFilter  = _rawHum;

    if (_filteredTemp > 0.0f) {
        if (isSpikeTemp(_rawTemp, _filteredTemp)) {
            tempToFilter = _filteredTemp;
        }
        if (isSpikeHum(_rawHum, _filteredHum)) {
            humToFilter = _filteredHum;
        }
    }

    // EMA filtresi
    if (_filteredTemp == 0.0f) {
        _filteredTemp = tempToFilter;
        _filteredHum  = humToFilter;
    } else {
        _filteredTemp = applyEMA(tempToFilter, _filteredTemp, EMA_ALPHA);
        _filteredHum  = applyEMA(humToFilter, _filteredHum, EMA_ALPHA);
    }

    return true;
}

// ==================== Getter'lar ====================

float SensorManager::getTemperature() const {
    return _filteredTemp;
}

float SensorManager::getHumidity() const {
    return _filteredHum;
}

float SensorManager::getRawTemperature() const {
    return _rawTemp;
}

float SensorManager::getRawHumidity() const {
    return _rawHum;
}

bool SensorManager::isSensor1OK() const {
    return _sensorOK && _data1.valid;
}

bool SensorManager::isSensor2OK() const {
    // Tek sensor - sensor1 ile ayni durumu dondur (API uyumlulugu)
    return _sensorOK && _data1.valid;
}

bool SensorManager::isAnyValid() const {
    return _data1.valid;
}

uint8_t SensorManager::getFailCount() const {
    return _failCount;
}

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
