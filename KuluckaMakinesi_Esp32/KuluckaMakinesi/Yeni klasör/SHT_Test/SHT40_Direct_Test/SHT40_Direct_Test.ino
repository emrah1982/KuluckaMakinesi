// ============================================================
// SHT40 DIREKT TEST (MUX olmadan)
// SHT40'i ESP32'ye direkt bag:
//   SHT40 SDA -> GPIO32
//   SHT40 SCL -> GPIO25
//   SHT40 VCC -> 3.3V
//   SHT40 GND -> GND
// Baud rate: 115200
// ============================================================

#include <Wire.h>

#define I2C_SDA_PIN  32
#define I2C_SCL_PIN  25
#define SHT40_ADDR   0x44
#define SHT40_ADDR2  0x45   // ADDR pini VCC'ye bagliysa

uint8_t crc8(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0xFF;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++)
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
    }
    return crc;
}

void scanI2C() {
    Serial.println("I2C Tarama...");
    int found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("  Cihaz: 0x%02X\n", addr);
            found++;
        }
    }
    if (found == 0) Serial.println("  HICBIR CIHAZ BULUNAMADI!");
}

bool readSHT40(uint8_t addr) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err != 0) {
        Serial.printf("[SHT40] 0x%02X: BULUNAMADI (err=%u)\n", addr, err);
        return false;
    }
    Serial.printf("[SHT40] 0x%02X: BULUNDU!\n", addr);

    Wire.beginTransmission(addr);
    Wire.write(0xFD);  // High precision
    if (Wire.endTransmission() != 0) {
        Serial.println("[SHT40] Komut hatasi!");
        return false;
    }
    delay(10);

    uint8_t got = Wire.requestFrom((int)addr, 6);
    if (got != 6) {
        Serial.printf("[SHT40] Veri hatasi: %u byte geldi\n", got);
        return false;
    }

    uint8_t buf[6];
    for (int i = 0; i < 6; i++) buf[i] = Wire.read();

    if (crc8(&buf[0], 2) != buf[2] || crc8(&buf[3], 2) != buf[5]) {
        Serial.println("[SHT40] CRC HATASI!");
        return false;
    }

    uint16_t rawT = ((uint16_t)buf[0] << 8) | buf[1];
    uint16_t rawH = ((uint16_t)buf[3] << 8) | buf[4];
    float temp = -45.0f + 175.0f * ((float)rawT / 65535.0f);
    float hum  = -6.0f  + 125.0f * ((float)rawH / 65535.0f);
    if (hum < 0) hum = 0;
    if (hum > 100) hum = 100;

    Serial.printf("[SHT40] Sicaklik: %.2f C  Nem: %.2f %%\n", temp, hum);
    return true;
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n=============================");
    Serial.println(" SHT40 DIREKT TEST (MUX YOK)");
    Serial.println("=============================\n");

    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(100000);
    delay(100);

    scanI2C();
}

void loop() {
    Serial.println("\n--- Okuma ---");
    // Her iki olasi adresi dene
    if (!readSHT40(0x44)) {
        Serial.println("0x44 basarisiz, 0x45 deneniyor...");
        readSHT40(0x45);
    }
    delay(3000);
}
