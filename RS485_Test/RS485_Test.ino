/*
 * ============================================================
 *  RS485 MAX485 + SHT20 Modbus RTU - TEST SKETCH
 * ============================================================
 *  UART konektoru (IO1/IO3) uzerinden MAX485 haberlesme testi.
 *  Ayni pinleri Serial debug ve RS485 icin sirayla kullanir:
 *    1. Serial2 ac -> RS485 Modbus okuma yap -> Serial2 kapat
 *    2. Serial ac  -> Sonuclari yazdir      -> Serial kapat
 *    3. Tekrarla (5 sn aralikla)
 *
 *  Baglanti:
 *    MAX485 RO    -> ESP32 IO3 (UART RX konektoru)
 *    MAX485 DI    -> ESP32 IO1 (UART TX konektoru)
 *    MAX485 DE+RE -> ESP32 IO5 (MicroSD CS padine lehim)
 *    SHT20 Sari(A)  -> MAX485 A
 *    SHT20 Beyaz(B) -> MAX485 B
 *    SHT20 & MAX485 besleme: 5V + GND
 * ============================================================
 */

#include <Arduino.h>

#define RS485_RX_PIN    3       // MAX485 RO -> IO3
#define RS485_TX_PIN    1       // MAX485 DI -> IO1
#define RS485_DE_RE_PIN 5       // MAX485 DE+RE -> IO5

#define MODBUS_BAUD       9600
#define MODBUS_SLAVE_ADDR 0x01
#define MODBUS_REG_TEMP   0x0001
#define MODBUS_REG_HUM    0x0002
#define MODBUS_TIMEOUT    500   // ms

// ==================== Modbus CRC16 ====================
uint16_t modbusCRC16(const uint8_t *data, uint8_t len) {
    uint16_t crc = 0xFFFF;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) { crc >>= 1; crc ^= 0xA001; }
            else { crc >>= 1; }
        }
    }
    return crc;
}

// ==================== Modbus Read Register ====================
bool modbusReadRegister(uint8_t slaveAddr, uint16_t regAddr, uint8_t *respBuf, uint8_t *respLen) {
    uint8_t frame[8];
    frame[0] = slaveAddr;
    frame[1] = 0x03;
    frame[2] = (regAddr >> 8) & 0xFF;
    frame[3] = regAddr & 0xFF;
    frame[4] = 0x00;
    frame[5] = 0x01;
    uint16_t crc = modbusCRC16(frame, 6);
    frame[6] = crc & 0xFF;
    frame[7] = (crc >> 8) & 0xFF;

    // RX tamponunu temizle
    while (Serial2.available()) Serial2.read();

    // TX modu -> gonder
    digitalWrite(RS485_DE_RE_PIN, HIGH);
    Serial2.write(frame, 8);
    Serial2.flush();

    // RX modu
    digitalWrite(RS485_DE_RE_PIN, LOW);

    // Yanit bekle
    unsigned long start = millis();
    uint8_t idx = 0;
    while ((millis() - start) < MODBUS_TIMEOUT && idx < 7) {
        if (Serial2.available()) {
            respBuf[idx++] = Serial2.read();
        }
    }

    *respLen = idx;

    if (idx < 7) return false;
    if (respBuf[0] != slaveAddr || respBuf[1] != 0x03) return false;

    uint16_t recvCrc = respBuf[idx - 2] | ((uint16_t)respBuf[idx - 1] << 8);
    uint16_t calcCrc = modbusCRC16(respBuf, idx - 2);
    if (recvCrc != calcCrc) return false;

    return true;
}

// ==================== Sonuclari Serial'a Yaz ====================
// RS485 isini bitirdikten sonra Serial acilip sonuclar yazdirilir
void printResults(bool tempOK, bool humOK,
                  uint8_t *respTemp, uint8_t respTempLen,
                  uint8_t *respHum, uint8_t respHumLen,
                  float temperature, float humidity,
                  uint32_t testNum) {

    Serial.begin(115200);
    delay(50);  // USB baglanti stabilizasyonu

    Serial.println();
    Serial.println("========================================");
    Serial.print("  RS485 TEST #");
    Serial.println(testNum);
    Serial.println("========================================");

    // --- Sicaklik ---
    Serial.print("[TEMP] Durum: ");
    Serial.println(tempOK ? "OK" : "HATA!");

    Serial.print("[TEMP] TX Frame: ");
    uint8_t txFrame[8] = {0x01, 0x03, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00};
    uint16_t c = modbusCRC16(txFrame, 6);
    txFrame[6] = c & 0xFF; txFrame[7] = (c >> 8) & 0xFF;
    for (int i = 0; i < 8; i++) {
        if (txFrame[i] < 0x10) Serial.print("0");
        Serial.print(txFrame[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    Serial.print("[TEMP] RX Frame (");
    Serial.print(respTempLen);
    Serial.print(" byte): ");
    for (int i = 0; i < respTempLen; i++) {
        if (respTemp[i] < 0x10) Serial.print("0");
        Serial.print(respTemp[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    if (tempOK) {
        Serial.print("[TEMP] Deger: ");
        Serial.print(temperature, 1);
        Serial.println(" C");
    }

    Serial.println();

    // --- Nem ---
    Serial.print("[HUM]  Durum: ");
    Serial.println(humOK ? "OK" : "HATA!");

    Serial.print("[HUM]  TX Frame: ");
    uint8_t txFrame2[8] = {0x01, 0x03, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00};
    c = modbusCRC16(txFrame2, 6);
    txFrame2[6] = c & 0xFF; txFrame2[7] = (c >> 8) & 0xFF;
    for (int i = 0; i < 8; i++) {
        if (txFrame2[i] < 0x10) Serial.print("0");
        Serial.print(txFrame2[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    Serial.print("[HUM]  RX Frame (");
    Serial.print(respHumLen);
    Serial.print(" byte): ");
    for (int i = 0; i < respHumLen; i++) {
        if (respHum[i] < 0x10) Serial.print("0");
        Serial.print(respHum[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    if (humOK) {
        Serial.print("[HUM]  Deger: %");
        Serial.println(humidity, 1);
    }

    Serial.println("========================================");

    if (!tempOK || !humOK) {
        Serial.println("KONTROL EDIN:");
        Serial.println("  - MAX485 A/B <-> SHT20 A/B kablo baglantisi");
        Serial.println("  - MAX485 DE+RE -> IO5 lehim baglantisi");
        Serial.println("  - MAX485 VCC=5V, GND baglantisi");
        Serial.println("  - SHT20 VCC=5V, GND baglantisi");
    }

    Serial.println();
    Serial.flush();
    delay(50);
    Serial.end();  // IO1/IO3 serbest birak (bir sonraki RS485 icin)
}

// ==================== Setup ====================
void setup() {
    pinMode(RS485_DE_RE_PIN, OUTPUT);
    digitalWrite(RS485_DE_RE_PIN, LOW);

    // Ilk mesaj
    Serial.begin(115200);
    delay(100);
    Serial.println();
    Serial.println("============================================");
    Serial.println("  RS485 MAX485 + SHT20 TEST BASLADI");
    Serial.println("============================================");
    Serial.println("Pinler: TX=IO1, RX=IO3, DE/RE=IO5");
    Serial.println("Modbus: 9600 8N1, Slave=0x01");
    Serial.println("Her 5 saniyede bir okuma yapilacak...");
    Serial.println();
    Serial.flush();
    delay(50);
    Serial.end();

    delay(500);
}

// ==================== Loop ====================
uint32_t testNum = 0;

void loop() {
    testNum++;

    // --- 1. RS485 AC: Sicaklik oku ---
    Serial2.begin(MODBUS_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
    delay(50);

    uint8_t respTemp[9] = {0};
    uint8_t respTempLen = 0;
    bool tempOK = modbusReadRegister(MODBUS_SLAVE_ADDR, MODBUS_REG_TEMP, respTemp, &respTempLen);

    float temperature = 0.0f;
    if (tempOK) {
        int16_t rawT = ((int16_t)respTemp[3] << 8) | respTemp[4];
        temperature = rawT / 10.0f;
    }

    // --- 2. RS485: Nem oku ---
    delay(50);
    uint8_t respHum[9] = {0};
    uint8_t respHumLen = 0;
    bool humOK = modbusReadRegister(MODBUS_SLAVE_ADDR, MODBUS_REG_HUM, respHum, &respHumLen);

    float humidity = 0.0f;
    if (humOK) {
        int16_t rawH = ((int16_t)respHum[3] << 8) | respHum[4];
        humidity = rawH / 10.0f;
    }

    // --- 3. RS485 KAPAT ---
    Serial2.end();
    delay(50);

    // --- 4. SERIAL AC: Sonuclari yazdir ---
    printResults(tempOK, humOK, respTemp, respTempLen, respHum, respHumLen,
                 temperature, humidity, testNum);

    // --- 5. Bekle ---
    delay(5000);
}
