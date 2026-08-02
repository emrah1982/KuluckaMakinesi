#include <Arduino.h>
#include <RTClib.h>
#include "pca9548a.h"
#include <DHT.h>
// -------- DHT22 (Yedek Sicaklik/Nem Sensoru) --------
#define DHTPIN 12         // DHT22 veri pini (GPIO12)
#define DHTTYPE DHT22     // DHT22 tipi
DHT dht(DHTPIN, DHTTYPE);


#define I2C_SDA   32
#define I2C_SCL   25
#define I2C_FREQ  100000

// -------- RS485 / Modbus RTU (SHT20 Sensor) --------
#define RS485_RX_PIN      27      // MAX485 RO -> ESP32 RX2
#define RS485_TX_PIN      16      // MAX485 DI -> ESP32 TX2
#define RS485_DE_RE_PIN   18      // MAX485 DE+RE -> ESP32 IO18 (KULLANILABİLİR)
#define MODBUS_BAUD       9600
#define MODBUS_SLAVE_ADDR 0x01
#define MODBUS_REG_TEMP   0x0001
#define MODBUS_REG_HUM    0x0002
#define MODBUS_TIMEOUT    500     // ms

// -------- Veri Kontrol LED'i --------
#define DATA_STATUS_LED_PIN  2   // ESP32 dahili LED (istege bagli degistirilebilir)

// -------- SABIT KANALLAR (KuluckaMakinesi/Config.h ile ayni olmali) --------
#define CH_RTC     0     // DS3231
#define CH_SHT30   1     // SHT40 (birincil) -> Kanal 1
#define CH_SHT30_2 2     // SHT30 (ikincil)  -> Kanal 2
#define CH_CO2     3     // SCD30            -> Kanal 3
#define CH_EGG_IR  4     // MLX90614         -> Kanal 4
#define CH_PCF     7     // PCF8574 role     -> Kanal 7

// -------- SHT30 Dual + Fuzyon --------
#define SHT30_ADDR 0x44
#define FUSION_TEMP_TOL 1.5f  // C - sensor arasi max fark
#define FUSION_HUM_TOL  5.0f  // % - sensor arasi max fark
#define FUSION_DIVERGE_LIMIT 3

bool sht30_1_Ok = false;
bool sht30_2_Ok = false;
static uint8_t sht30FailCount1 = 0;
static uint8_t sht30FailCount2 = 0;
static uint8_t divergeCount = 0;
float fusionTemp = 0, fusionHum = 0;
const char* fusionStatusStr = "BASLATILMADI";

static const uint8_t ADDR_DS3231 = 0x68;
static const uint8_t ADDR_SHT31_1 = 0x44;
static const uint8_t ADDR_SHT31_2 = 0x45;

PCA9548A mux;
RTC_DS3231 rtc;

uint8_t muxAddr = PCA9548A_BASE_ADDR;

int8_t rtcCh = -1;
int8_t shtCh = -1;
uint8_t shtAddr = 0;

// -------- PCF8574 Role Kontrol --------
int8_t pcfCh = -1;          // MUX kanali (-1=bulunamadi, -2=dogrudan busta)
uint8_t pcfAddress = 0;     // Otomatik bulunan adres (0x20-0x27 veya 0x38-0x3F)
uint8_t relayState = 0xFF;  // Tum roleler KAPALI (Active LOW)
bool RELAY_ACTIVE_LOW = true;

// -------- SHT20 (RS485) Durum --------
bool sht20Ok = false;

// ==================== YARDIMCI FONKSIYONLAR ====================

static const char* i2cErrStr(uint8_t ec) {
  switch (ec) {
    case 0: return "OK";
    case 1: return "data too long";
    case 2: return "NACK on address";
    case 3: return "NACK on data";
    case 4: return "other error";
    case 5: return "timeout";
    default: return "unknown";
  }
}

static uint8_t i2cEndTx(uint8_t addr) {
  Wire.beginTransmission(addr);
  return (uint8_t)Wire.endTransmission();
}

static bool i2cPing(uint8_t addr) {
  return (i2cEndTx(addr) == 0);
}

static void i2cRecover() {
  Wire.end();
  delay(10);
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(I2C_FREQ);
  Wire.setTimeOut(50);
  delay(20);
}

static bool selectMux(uint8_t ch) {
  mux.closeAll();
  delay(2);
  bool ok = mux.selectChannel(ch);
  delay(5);
  return ok;
}

static int findMuxAddr() {
  for (uint8_t a = 0x70; a <= 0x77; a++) {
    if (i2cPing(a)) return a;
  }
  return -1;
}

static bool muxSelectVerify(uint8_t ch, const char* tag) {
  if (ch > 7) {
    Serial.printf("[MUX] %s select CH%u gecersiz\n", tag ? tag : "", ch);
    return false;
  }

  uint8_t exp = (uint8_t)(1u << ch);
  for (uint8_t attempt = 1; attempt <= 3; attempt++) {
    // reg=0xFF ise MUX kararsiz, once sifirla
    if (attempt > 1) {
      mux.closeAll();
      delay(10);
    }
    bool ok = mux.selectChannel(ch);
    delay(5);
    uint8_t reg = mux.readChannels();
    if (ok && reg == exp) return true;
    Serial.printf("[MUX] %s CH%u select=%s reg=0x%02X exp=0x%02X (try %u)\n",
                  tag ? tag : "", ch, ok ? "OK" : "HATA", reg, exp, attempt);
    delay(10);
  }
  return false;
}

// ==================== PCF8574 ROLE FONKSIYONLARI ====================

// PCF8574'e yaz (MUX kanal secimi dahil)
static void writePCF(uint8_t data) {
  if (pcfAddress == 0) return;

  if (pcfCh >= 0) {
    if (!selectMux((uint8_t)pcfCh)) {
      Serial.println("[PCF8574] HATA: MUX kanal secilemedi");
      return;
    }
  }

  Wire.beginTransmission(pcfAddress);
  Wire.write(data);
  uint8_t ec = Wire.endTransmission();
  delay(2);
  if (ec != 0) {
    Serial.printf("[PCF8574] YAZMA HATASI ec=%u(%s)\n", ec, i2cErrStr(ec));
  }
}

// Tek role kontrol (ch: 0-7, state: true=ACIK, false=KAPALI)
static void setRelay(uint8_t ch, bool state) {
  if (ch > 7) return;
  if (RELAY_ACTIVE_LOW) {
    if (state) relayState &= ~(1 << ch);
    else       relayState |= (1 << ch);
  } else {
    if (state) relayState |= (1 << ch);
    else       relayState &= ~(1 << ch);
  }
  writePCF(relayState);
}

// Tum roleleri ac/kapat
static void allRelays(bool state) {
  for (int i = 0; i < 8; i++) {
    setRelay(i, state);
  }
}

// ==================== I2C TARAMA ====================

// Belirli bir MUX kanalinda PCF8574 ara (0x20-0x27 ve 0x38-0x3F)
static uint8_t findPCF8574onChannel() {
  for (uint8_t addr = 0x20; addr <= 0x27; addr++) {
    if (i2cPing(addr)) return addr;
  }
  for (uint8_t addr = 0x38; addr <= 0x3F; addr++) {
    if (i2cPing(addr)) return addr;
  }
  return 0;
}

static uint8_t findPCFOnFixedChannel() {
  if (!selectMux(CH_PCF)) return 0;
  return findPCF8574onChannel();
}

static bool checkRTCOnFixedChannel() {
  if (!selectMux(CH_RTC)) return false;
  return i2cPing(ADDR_DS3231);
}

static void scanChannel(uint8_t ch) {
  if (!muxSelectVerify(ch, "scan")) {
    Serial.printf("[I2C] CH%u scan atlandi (MUX select sorunu)\n", ch);
    return;
  }

  bool any = false;
  Serial.printf("[I2C] CH%u scan: ", ch);
  for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
    // MUX adreslerini atla - yazinca MUX registeri bozuluyor!
    if (addr >= 0x70 && addr <= 0x77) continue;
    if (i2cPing(addr)) {
      any = true;
      Serial.printf("0x%02X ", addr);
    }
  }
  if (!any) Serial.print("(bos)");
  Serial.println();

  if (rtcCh < 0) {
    uint8_t ec = i2cEndTx(ADDR_DS3231);
    if (ec == 0) {
      rtcCh = (int8_t)ch;
      Serial.printf("[RTC] DS3231 goruldu: CH%u addr=0x%02X\n", ch, ADDR_DS3231);
    }
  }
  if (shtCh < 0) {
    uint8_t ec1 = i2cEndTx(ADDR_SHT31_1);
    uint8_t ec2 = i2cEndTx(ADDR_SHT31_2);
    if (ec1 == 0) {
      shtCh = (int8_t)ch; shtAddr = ADDR_SHT31_1;
      Serial.printf("[SHT31] goruldu: CH%u addr=0x%02X\n", ch, shtAddr);
    } else if (ec2 == 0) {
      shtCh = (int8_t)ch; shtAddr = ADDR_SHT31_2;
      Serial.printf("[SHT31] goruldu: CH%u addr=0x%02X\n", ch, shtAddr);
    }
  }
  // PCF8574 sadece dogrudan busta bulunamadiysa kanal taramasinda ara
  // (dogrudan bustaysa pcfCh == -2, tekrar aramaya gerek yok)
  if (pcfCh == -1) {
    uint8_t found = findPCF8574onChannel();
    if (found != 0) {
      pcfCh = (int8_t)ch;
      pcfAddress = found;
      Serial.printf("[PCF8574] goruldu: CH%u addr=0x%02X\n", ch, pcfAddress);
    }
  }
}

static void __attribute__((unused)) scanAllChannels() {
  rtcCh = -1;
  shtCh = -1;
  shtAddr = 0;
  pcfCh = -1;
  pcfAddress = 0;

  // ONCE: MUX kapali iken dogrudan busta PCF8574 ara
  mux.closeAll();
  delay(10);
  uint8_t directPcf = findPCF8574onChannel();
  if (directPcf != 0) {
    pcfCh = -2;
    pcfAddress = directPcf;
    Serial.printf("[PCF8574] goruldu: dogrudan I2C bus addr=0x%02X\n", pcfAddress);
  }

  // SONRA: MUX kanallarini tara (PCF8574 zaten bulunduysa kanallarda aramaz)
  for (uint8_t ch = 0; ch < 8; ch++) {
    scanChannel(ch);
  }
  mux.closeAll();
  delay(5);

  Serial.println("====== TARAMA SONUCLARI ======");
  Serial.printf("[I2C] DS3231:  %s", (rtcCh >= 0) ? "bulundu" : "BULUNAMADI");
  if (rtcCh >= 0) Serial.printf(" (CH%d, addr=0x%02X)", rtcCh, ADDR_DS3231);
  Serial.println();

  Serial.printf("[I2C] SHT31:   %s", (shtCh >= 0) ? "bulundu" : "BULUNAMADI");
  if (shtCh >= 0) Serial.printf(" (CH%d, addr=0x%02X)", shtCh, shtAddr);
  Serial.println();

  Serial.printf("[I2C] PCF8574: %s", (pcfAddress != 0) ? "bulundu" : "BULUNAMADI");
  if (pcfCh == -2) Serial.printf(" (dogrudan bus, addr=0x%02X)", pcfAddress);
  else if (pcfCh >= 0) Serial.printf(" (CH%d, addr=0x%02X)", pcfCh, pcfAddress);
  Serial.println();
  Serial.println("==============================");
}

// ==================== SHT31 ====================

static uint8_t sht31Crc8(const uint8_t *data, uint8_t len) {
  uint8_t crc = 0xFF;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t b = 0; b < 8; b++) {
      if (crc & 0x80) crc = (crc << 1) ^ 0x31;
      else crc <<= 1;
    }
  }
  return crc;
}

static bool __attribute__((unused)) readSHT31(float &tC, float &rh) {
  if (shtCh < 0 || shtAddr == 0) return false;
  if (!muxSelectVerify((uint8_t)shtCh, "sht")) return false;

  Wire.beginTransmission(shtAddr);
  Wire.write(0x24);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) return false;

  delay(20);
  if (Wire.requestFrom((int)shtAddr, 6) != 6) return false;

  uint8_t b[6];
  for (int i = 0; i < 6; i++) b[i] = Wire.read();

  if (sht31Crc8(&b[0], 2) != b[2]) return false;
  if (sht31Crc8(&b[3], 2) != b[5]) return false;

  uint16_t rawT = ((uint16_t)b[0] << 8) | b[1];
  uint16_t rawH = ((uint16_t)b[3] << 8) | b[4];
  tC = -45.0f + 175.0f * ((float)rawT / 65535.0f);
  rh = 100.0f * ((float)rawH / 65535.0f);
  return true;
}

// ==================== SHT30 DUAL + FUZYON ====================

static bool checkSHT30(uint8_t ch) {
  if (!selectMux(ch)) return false;
  return i2cPing(SHT30_ADDR);
}

static bool readSHT30OnChannel(uint8_t ch, float &tC, float &rh) {
  if (!selectMux(ch)) return false;

  Wire.beginTransmission(SHT30_ADDR);
  Wire.write(0x2C);
  Wire.write(0x06);
  if (Wire.endTransmission() != 0) return false;

  delay(20);
  if (Wire.requestFrom((int)SHT30_ADDR, 6) != 6) return false;

  uint8_t b[6];
  for (int i = 0; i < 6; i++) b[i] = Wire.read();

  if (sht31Crc8(&b[0], 2) != b[2]) return false;
  if (sht31Crc8(&b[3], 2) != b[5]) return false;

  uint16_t rawT = ((uint16_t)b[0] << 8) | b[1];
  uint16_t rawH = ((uint16_t)b[3] << 8) | b[4];
  tC = -45.0f + 175.0f * ((float)rawT / 65535.0f);
  rh = 100.0f * ((float)rawH / 65535.0f);

  if (tC < -40.0f || tC > 80.0f || rh < 0.0f || rh > 100.0f) return false;
  return true;
}

static void sensorFusion(bool ok1, float t1, float h1,
                          bool ok2, float t2, float h2) {
  if (ok1 && ok2) {
    float avg_t = (t1 + t2) / 2.0f;
    float avg_h = (h1 + h2) / 2.0f;
    float dT = fabs(t1 - t2);
    float dH = fabs(h1 - h2);

    if (dT <= FUSION_TEMP_TOL && dH <= FUSION_HUM_TOL) {
      // Uyumlu -> ortalama
      fusionTemp = avg_t;
      fusionHum  = avg_h;
      fusionStatusStr = "CIFT OK";
      divergeCount = 0;
    } else {
      // Uyumsuz -> onceki degere yakin olani sec
      divergeCount++;
      if (fusionTemp > 0.0f) {
        float d1 = fabs(t1 - fusionTemp);
        float d2 = fabs(t2 - fusionTemp);
        fusionTemp = (d1 <= d2) ? t1 : t2;
        fusionHum  = (d1 <= d2) ? h1 : h2;
      } else {
        fusionTemp = avg_t;
        fusionHum  = avg_h;
      }
      fusionStatusStr = "UYUMSUZ";
      // Sadece esige ulastiginda bir kez uyar (spam yapma)
      if (divergeCount == FUSION_DIVERGE_LIMIT) {
        Serial.printf("[FUZYON] UYARI: Sensorler uyumsuz! dT=%.2f dH=%.2f\n", dT, dH);
        Serial.printf("[FUZYON]   S1: T=%.2f H=%.2f  |  S2: T=%.2f H=%.2f\n", t1, h1, t2, h2);
      }
    }
  } else if (ok1) {
    fusionTemp = t1;
    fusionHum  = h1;
    fusionStatusStr = "SADECE S1";
    divergeCount = 0;
    if (sht30FailCount2 == 1)
      Serial.println("[FUZYON] UYARI: SHT30#2 ariza! Sadece #1 kullaniliyor.");
  } else if (ok2) {
    fusionTemp = t2;
    fusionHum  = h2;
    fusionStatusStr = "SADECE S2";
    divergeCount = 0;
    if (sht30FailCount1 == 1)
      Serial.println("[FUZYON] UYARI: SHT30#1 ariza! Sadece #2 kullaniliyor.");
  } else {
    fusionStatusStr = "SENSOR YOK";
    divergeCount = 0;
  }
}

// ==================== RS485 / SHT20 (Modbus RTU) ====================

static void rs485TxMode() { digitalWrite(RS485_DE_RE_PIN, HIGH); }
static void rs485RxMode() { digitalWrite(RS485_DE_RE_PIN, LOW);  }

static uint16_t modbusCRC16(const uint8_t *data, uint8_t len) {
  uint16_t crc = 0xFFFF;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x0001) { crc >>= 1; crc ^= 0xA001; }
      else              { crc >>= 1; }
    }
  }
  return crc;
}

static bool modbusReadRegister(uint8_t slaveAddr, uint16_t regAddr,
                               uint16_t regCount, uint8_t *respBuf, uint8_t respLen) {
  uint8_t frame[8];
  frame[0] = slaveAddr;
  frame[1] = 0x03;
  frame[2] = (regAddr >> 8) & 0xFF;
  frame[3] = regAddr & 0xFF;
  frame[4] = (regCount >> 8) & 0xFF;
  frame[5] = regCount & 0xFF;
  uint16_t crc = modbusCRC16(frame, 6);
  frame[6] = crc & 0xFF;
  frame[7] = (crc >> 8) & 0xFF;

  while (Serial2.available()) Serial2.read();  // Buffer temizle

  rs485TxMode();
  Serial2.write(frame, 8);
  Serial2.flush();
  rs485RxMode();

  unsigned long start = millis();
  uint8_t idx = 0;
  while ((millis() - start) < MODBUS_TIMEOUT && idx < respLen) {
    if (Serial2.available()) respBuf[idx++] = Serial2.read();
  }

  Serial.printf("[RS485] reg=0x%04X want=%u got=%u bytes: ",
                regAddr, (unsigned)respLen, (unsigned)idx);
  for (uint8_t i = 0; i < idx; i++) Serial.printf("%02X ", respBuf[i]);
  Serial.println();

  if (idx < respLen) { Serial.println("[RS485] HATA: timeout/eksik yanit"); return false; }
  if (respBuf[0] != slaveAddr || respBuf[1] != 0x03) {
    Serial.println("[RS485] HATA: header uyumsuz"); return false;
  }
  uint16_t recvCrc = respBuf[idx - 2] | ((uint16_t)respBuf[idx - 1] << 8);
  uint16_t calcCrc = modbusCRC16(respBuf, idx - 2);
  if (recvCrc != calcCrc) { Serial.printf("[RS485] HATA: CRC uyumsuz recv=0x%04X calc=0x%04X\n", recvCrc, calcCrc); return false; }
  return true;
}

static bool sht20Begin() {
  pinMode(RS485_DE_RE_PIN, OUTPUT);
  rs485RxMode();
  Serial2.begin(MODBUS_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
  delay(100);

  // Test okumasi - sicaklik register'ina oku
  uint8_t resp[7];
  bool ok = modbusReadRegister(MODBUS_SLAVE_ADDR, MODBUS_REG_TEMP, 1, resp, 7);
  return ok;
}

static bool readSHT20(float &tC, float &rh) {
  uint8_t respTemp[7];
  bool tempOK = modbusReadRegister(MODBUS_SLAVE_ADDR, MODBUS_REG_TEMP, 1, respTemp, 7);
  delay(50);
  uint8_t respHum[7];
  bool humOK = modbusReadRegister(MODBUS_SLAVE_ADDR, MODBUS_REG_HUM, 1, respHum, 7);

  if (tempOK && humOK) {
    int16_t rawT = ((int16_t)respTemp[3] << 8) | respTemp[4];
    int16_t rawH = ((int16_t)respHum[3] << 8) | respHum[4];
    tC = rawT / 10.0f;
    rh  = rawH / 10.0f;
    // Akla yatkinlik kontrolu
    if (tC > -40.0f && tC < 80.0f && rh >= 0.0f && rh <= 100.0f) return true;
    Serial.printf("[SHT20] Deger aralik disi: T=%.1f RH=%.1f\n", tC, rh);
  }
  return false;
}

// ==================== SETUP ====================

void setup() {


    // DHT22 başlat
    dht.begin();
  

  Serial.begin(115200);
  delay(200);

  // Pin testi: Sonuçlar Serial Monitör'de görünür
  testAllPins();

  pinMode(DATA_STATUS_LED_PIN, OUTPUT); // Veri kontrol LED'i
  digitalWrite(DATA_STATUS_LED_PIN, LOW); // Baslangicta kapali

  Serial.println("\n[BOOT] deneme.ino calisiyor");
  Serial.println("=== SISTEM BASLATILIYOR ===");

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(I2C_FREQ);
  Wire.setTimeOut(50);
  Serial.printf("[I2C] Wire.begin OK (SDA=%d SCL=%d F=%lu)\n", I2C_SDA, I2C_SCL, (unsigned long)I2C_FREQ);

  // ---- MUX oncesi dogrudan bus taramasi ----
  Serial.println("====== DOGRUDAN I2C BUS TARAMASI (MUX oncesi) ======");
  for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
    uint8_t ec = i2cEndTx(addr);
    if (ec == 0) {
      Serial.printf("  0x%02X -> OK", addr);
      if (addr == PCA9548A_BASE_ADDR) Serial.print(" (PCA9548A MUX)");
      if (addr == ADDR_DS3231)        Serial.print(" (DS3231 RTC)");
      if (addr == ADDR_SHT31_1)       Serial.print(" (SHT31 @0x44)");
      if (addr == ADDR_SHT31_2)       Serial.print(" (SHT31 @0x45)");
      if (addr >= 0x20 && addr <= 0x27) Serial.printf(" (PCF8574 A2A1A0=%d%d%d)", (addr>>2)&1, (addr>>1)&1, addr&1);
      if (addr >= 0x38 && addr <= 0x3F) Serial.printf(" (PCF8574A A2A1A0=%d%d%d)", (addr>>2)&1, (addr>>1)&1, addr&1);
      Serial.println();
    }
  }
  Serial.println("=====================================================");

  mux.begin();
  {
    int found = findMuxAddr();
    if (found >= 0) {
      muxAddr = (uint8_t)found;
      mux.setAddress(muxAddr);
      Serial.printf("[MUX] PCA9548A bulundu addr=0x%02X\n", muxAddr);
      uint8_t ec = i2cEndTx(muxAddr);
      Serial.printf("[MUX] PCA9548A ping addr=0x%02X -> ec=%u(%s)\n", muxAddr, ec, i2cErrStr(ec));
    } else {
      Serial.println("[MUX] HATA: PCA9548A bulunamadi (0x70-0x77)");
    }
  }

  // ---- RTC (SABIT KANAL) ----
  if (checkRTCOnFixedChannel()) {
    rtcCh = CH_RTC;
    if (rtc.begin()) {
      Serial.println("[RTC] DS3231 OK");
      if (rtc.lostPower()) {
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        Serial.println("[RTC] UYARI: lostPower -> derleme zamani ayarlandi");
      }
    } else {
      Serial.println("[RTC] HATA: rtc.begin basarisiz");
      rtcCh = -1;
    }
  } else {
    Serial.println("[RTC] DS3231 BULUNAMADI (CH_RTC)");
  }

  // ---- PCF8574 (SABIT KANAL) ----
  pcfAddress = findPCFOnFixedChannel();
  if (pcfAddress == 0) {
    Serial.println("[PCF8574] BULUNAMADI (CH_PCF)");
    pcfCh = -1;
  } else {
    pcfCh = CH_PCF;
    Serial.printf("[PCF8574] Bulundu: addr=0x%02X (CH_PCF)\n", pcfAddress);
    relayState = 0xFF;
    writePCF(relayState);
    Serial.println("\nSERI KOMUTLAR:");
    Serial.println("  1-8 : Role toggle");
    Serial.println("  a   : Hepsi AC");
    Serial.println("  k   : Hepsi KAPAT");
  }

  // ---- SHT40 #1 (MUX CH1) ----
  if (checkSHT30(CH_SHT30)) {
    sht30_1_Ok = true;
    float t = 0, h = 0;
    if (readSHT30OnChannel(CH_SHT30, t, h))
      Serial.printf("[SHT30#1] OK CH%d T=%.2fC H=%.2f%%\n", CH_SHT30, t, h);
    else
      Serial.printf("[SHT30#1] Ilk olcum basarisiz (CH%d)\n", CH_SHT30);
  } else {
    sht30_1_Ok = false;
    Serial.printf("[SHT30#1] BULUNAMADI (CH%d)\n", CH_SHT30);
  }

  // ---- SHT30 #2 (MUX CH2) ----
  if (checkSHT30(CH_SHT30_2)) {
    sht30_2_Ok = true;
    float t = 0, h = 0;
    if (readSHT30OnChannel(CH_SHT30_2, t, h))
      Serial.printf("[SHT30#2] OK CH%d T=%.2fC H=%.2f%%\n", CH_SHT30_2, t, h);
    else
      Serial.printf("[SHT30#2] Ilk olcum basarisiz (CH%d)\n", CH_SHT30_2);
  } else {
    sht30_2_Ok = false;
    Serial.printf("[SHT30#2] BULUNAMADI (CH%d)\n", CH_SHT30_2);
  }

  // Fuzyon durumu
  if (sht30_1_Ok && sht30_2_Ok)
    Serial.println("[FUZYON] Cift sensor aktif -> fuzyon modu");
  else if (sht30_1_Ok)
    Serial.println("[FUZYON] Sadece SHT30#1 aktif");
  else if (sht30_2_Ok)
    Serial.println("[FUZYON] Sadece SHT30#2 aktif");
  else
    Serial.println("[FUZYON] UYARI: Hicbir SHT30 bulunamadi!");

  digitalWrite(DATA_STATUS_LED_PIN, (sht30_1_Ok || sht30_2_Ok) ? HIGH : LOW);

  // ---- SHT20 (RS485/Modbus - Yedek) ----
  Serial.println("[SHT20] RS485 baslatiliyor...");
  Serial.printf("[SHT20] RX=%d TX=%d DE/RE=%d BAUD=%d\n",
                RS485_RX_PIN, RS485_TX_PIN, RS485_DE_RE_PIN, MODBUS_BAUD);
  sht20Ok = sht20Begin();
  if (sht20Ok) {
    Serial.println("[SHT20] OK - sensor yanit verdi (yedek)");
  } else {
    Serial.println("[SHT20] BULUNAMADI - kablolari kontrol et (RX/TX/DE-RE/A+B)");
  }

  Serial.println("=== SISTEM HAZIR ===\n");
}

// ==================== LOOP ====================

void loop() {
  static unsigned long lastMs = 0;

  // ---- Seri komut: Role kontrolu ----
  if (Serial.available()) {
    char cmd = Serial.read();

    if (cmd >= '1' && cmd <= '8' && pcfAddress != 0) {
      int ch = cmd - '1';
      bool currentOn = RELAY_ACTIVE_LOW ? !(relayState & (1 << ch)) : !!(relayState & (1 << ch));
      setRelay(ch, !currentOn);
      Serial.printf("[ROLE] Role %d -> %s\n", ch, (!currentOn) ? "ACIK" : "KAPALI");
    }
    else if (cmd == 'a' && pcfAddress != 0) {
      allRelays(true);
      Serial.println("[ROLE] TUM ROLELER ACIK");
    }
    else if (cmd == 'k' && pcfAddress != 0) {
      allRelays(false);
      Serial.println("[ROLE] TUM ROLELER KAPALI");
    }
  }

  // ---- 3 saniyede bir sensor oku ----
  if (millis() - lastMs < 3000) return;
  lastMs = millis();

  static uint8_t rtcFailCount = 0;
  static uint8_t pcfFailCount = 0;
  static uint8_t sht20FailCount = 0;

  // ==================== DUAL SHT30 + FUZYON ====================
  float t1 = 0, h1 = 0, t2 = 0, h2 = 0;
  bool ok1 = false, ok2 = false;

  if (sht30_1_Ok || sht30FailCount1 < 10) {
    ok1 = readSHT30OnChannel(CH_SHT30, t1, h1);
    sht30_1_Ok = ok1;
    sht30FailCount1 = ok1 ? 0 : (sht30FailCount1 + 1);
    if (ok1)
      Serial.printf("[SHT30#1] T=%.2fC H=%.2f%%\n", t1, h1);
    else if (sht30FailCount1 == 3)
      Serial.println("[SHT30#1] 3+ ardisik hata! Kontrol et.");
  }

  if (sht30_2_Ok || sht30FailCount2 < 10) {
    ok2 = readSHT30OnChannel(CH_SHT30_2, t2, h2);
    sht30_2_Ok = ok2;
    sht30FailCount2 = ok2 ? 0 : (sht30FailCount2 + 1);
    if (ok2)
      Serial.printf("[SHT30#2] T=%.2fC H=%.2f%%\n", t2, h2);
    else if (sht30FailCount2 == 3)
      Serial.println("[SHT30#2] 3+ ardisik hata! Kontrol et.");
  }

  sensorFusion(ok1, t1, h1, ok2, t2, h2);

  if (ok1 || ok2) {
    Serial.printf("[FUZYON] T=%.2fC H=%.2f%%  [%s]\n", fusionTemp, fusionHum, fusionStatusStr);
    digitalWrite(DATA_STATUS_LED_PIN, HIGH);
  } else {
    Serial.printf("[FUZYON] Veri yok! [%s]\n", fusionStatusStr);
    digitalWrite(DATA_STATUS_LED_PIN, LOW);
  }

  // DHT22 (yedek - sadece bilgi amacli)
  float dhtT = dht.readTemperature();
  float dhtH = dht.readHumidity();
  if (!isnan(dhtT) && !isnan(dhtH)) {
    Serial.printf("[DHT22] T=%.1fC RH=%.1f%%  (yedek)\n", dhtT, dhtH);
  }

  // RTC oku
  if (rtcCh >= 0) {
    if (selectMux((uint8_t)rtcCh) && i2cPing(ADDR_DS3231)) {
      DateTime now = rtc.now();
      Serial.printf("[RTC] %02d/%02d/%04d %02d:%02d:%02d\n",
                    now.day(), now.month(), now.year(),
                    now.hour(), now.minute(), now.second());
      rtcFailCount = 0;
    } else {
      rtcFailCount++;
      Serial.printf("[RTC] okuma basarisiz (%u/3)\n", rtcFailCount);
      if (rtcFailCount >= 3) {
        Serial.println("[I2C] I2C recover deneniyor...");
        i2cRecover();
        rtcFailCount = 0;
      }
    }
  }

  // PCF8574 durum oku
  mux.closeAll();
  delay(3);
  if (pcfAddress != 0) {
    if (pcfCh >= 0) selectMux((uint8_t)pcfCh);
    if (i2cPing(pcfAddress)) {
      Wire.requestFrom((int)pcfAddress, 1);
      uint8_t portVal = Wire.available() ? Wire.read() : 0xFF;
      Serial.printf("[PCF8574] port=0x%02X  Isitici=%s  Nem=%s  M.Guc=%s  M.Yon=%s\n",
                    portVal,
                    ((portVal >> 0) & 1) ? "OFF" : "ON",
                    ((portVal >> 1) & 1) ? "OFF" : "ON",
                    ((portVal >> 2) & 1) ? "OFF" : "ON",
                    ((portVal >> 3) & 1) ? "OFF" : "ON");
      pcfFailCount = 0;
    } else {
      pcfFailCount++;
      Serial.printf("[PCF8574] ping basarisiz (%u/3)\n", pcfFailCount);
    }
  }

  mux.closeAll();
  delay(3);

  // SHT20 (RS485, yedek) - I2C'den tamamen bagimsiz
  if (sht20Ok) {
    float t = 0, h = 0;
    if (readSHT20(t, h)) {
      Serial.printf("[SHT20] T=%.1fC RH=%.1f%%  (yedek)\n", t, h);
      sht20FailCount = 0;
    } else {
      sht20FailCount++;
      Serial.printf("[SHT20] okuma basarisiz (%u)\n", sht20FailCount);
    }
  }

  Serial.println("---");
}


// ==================== PIN TEST FONKSIYONU ====================
// Bu fonksiyon, kullanılabilir pinleri test etmek için eklenmiştir.
// setup() içinde testAllPins() fonksiyonunu çağırırsan, sonuçları Serial monitörde görebilirsin.

int testPins[] = {2,4,5,12,13,14,15,16,17,18,19,21,22,23,25,26,27,32,33};

void testAllPins() {
  Serial.println("\n--- PIN TEST BASLIYOR ---");
  for (int i = 0; i < sizeof(testPins)/sizeof(testPins[0]); i++) {
    int pin = testPins[i];

    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
    delay(10);

    pinMode(pin, INPUT_PULLDOWN);
    int val = digitalRead(pin);

    Serial.print("Pin ");
    Serial.print(pin);

    if (val == HIGH) {
      Serial.println(" ❌ ÇAKIŞMA / KULLANILIYOR");
    } else {
      Serial.println(" ✅ BOŞ / KULLANILABİLİR");
    }
  }
  Serial.println("--- PIN TEST BITTI ---\n");
}
