#include "I2CMux.h"

// ============================================================
// Grove - 8 Channel I2C Multiplexer (TCA9548A tabanli)
// Kanal haritasi Config.h icindeki MUX_CH_* tanimlarindadir.
// Kontrol: tek byte yazilir, her bit bir kanali acar (1<<ch).
//
// ADRES: TCA9548A'nin A0/A1/A2 pinleri adresi belirler. Hepsi GND ise
// taban adres 0x70'tir; A0=+1, A1=+2, A2=+4 ile 0x77'ye kadar cikar.
// Grove kartinda bu pinler varsayilan olarak GND'ye baglidir (uzerinde
// yazan deger: 0x70). Kart uzerindeki adres lehimleri degistirilirse
// veya farkli bir kart takilirsa asagidaki otomatik tarama devreye girer:
// once MUX_ADDR denenir, bulunamazsa 0x70-0x77 taranir. Bu yuzden
// Config.h'deki MUX_ADDR'i elle degistirmek gerekmez.
// ============================================================

namespace I2CMux {

static bool _inited = false;
static bool _ready = false;
static uint8_t _addr = MUX_ADDR;

// Bus saglik sayaclari
static uint8_t  _consecFail   = 0;      // Art arda basarisiz kanal secimi
static uint32_t _recoverCount = 0;      // Toplam bus kurtarma sayisi
static bool     _inRecovery   = false;  // Yeniden girisi engelle

static bool pingAddr(uint8_t a) {
    Wire.beginTransmission(a);
    return (Wire.endTransmission() == 0);
}

// Kontrol registerine yaz (kanal maskesi)
static bool writeMask(uint8_t a, uint8_t mask) {
    Wire.beginTransmission(a);
    Wire.write(mask);
    return (Wire.endTransmission() == 0);
}

// Kontrol registerini geri oku. TCA9548A/PCA9548A yazilan maskeyi aynen
// dondurur; bu ozellik adresteki cihazin gercekten MUX oldugunu dogrular.
static bool readMask(uint8_t a, uint8_t &mask) {
    if (Wire.requestFrom((int)a, (int)1) != 1) return false;
    mask = Wire.read();
    return true;
}

// Adresteki cihaz gercekten bir I2C multiplexer mi?
// 0x70-0x77 araliginda baska cihazlar da olabilir (ornegin BMP280/BME280
// 0x76-0x77 kullanir). Yazilan maskeyi geri veremeyen cihaz MUX degildir.
static bool verifyMux(uint8_t a) {
    uint8_t rb = 0xFF;

    // 1) Tum kanallar kapali -> geri okuma 0x00 olmali
    if (!writeMask(a, 0x00)) return false;
    delay(2);
    if (!readMask(a, rb) || rb != 0x00) return false;

    // 2) Sadece kanal 0 acik -> geri okuma 0x01 olmali
    if (!writeMask(a, 0x01)) return false;
    delay(2);
    if (!readMask(a, rb) || rb != 0x01) return false;

    // Kanallari tekrar kapat, temiz durumda birak
    writeMask(a, 0x00);
    delay(2);
    return true;
}

// ---------------------------------------------------------------------
//  Takilan I2C bus'i serbest birak (bit-bang kurtarma)
//
//  Bir slave transfer ortasinda resetlenirse SDA'yi LOW tutmaya devam
//  edebilir; bu durumda master hicbir sey yapamaz ve tum bus olur.
//  Cozum: SCL'e manuel darbe gonderip slave'in kalan bitleri bosaltmasini
//  saglamak, ardindan gecerli bir STOP kosulu uretmek.
// ---------------------------------------------------------------------
static void busUnstick() {
    Wire.end();
    delay(5);

    pinMode(I2C_SCL_PIN, OUTPUT_OPEN_DRAIN);
    pinMode(I2C_SDA_PIN, INPUT_PULLUP);
    digitalWrite(I2C_SCL_PIN, HIGH);
    delayMicroseconds(I2C_RECOVER_PULSE_US);

    // SDA serbest kalana kadar (max I2C_RECOVER_PULSES) saat darbesi gonder
    for (uint8_t i = 0; i < I2C_RECOVER_PULSES; i++) {
        if (digitalRead(I2C_SDA_PIN) == HIGH) break;   // slave birakti
        digitalWrite(I2C_SCL_PIN, LOW);
        delayMicroseconds(I2C_RECOVER_PULSE_US);
        digitalWrite(I2C_SCL_PIN, HIGH);
        delayMicroseconds(I2C_RECOVER_PULSE_US);
    }

    // STOP kosulu: SCL HIGH iken SDA LOW -> HIGH
    pinMode(I2C_SDA_PIN, OUTPUT_OPEN_DRAIN);
    digitalWrite(I2C_SDA_PIN, LOW);
    delayMicroseconds(I2C_RECOVER_PULSE_US);
    digitalWrite(I2C_SCL_PIN, HIGH);
    delayMicroseconds(I2C_RECOVER_PULSE_US);
    digitalWrite(I2C_SDA_PIN, HIGH);
    delayMicroseconds(I2C_RECOVER_PULSE_US);

    // Pinleri birak, Wire.begin() yeniden konfigure edecek
    pinMode(I2C_SDA_PIN, INPUT);
    pinMode(I2C_SCL_PIN, INPUT);
    delay(5);
}

// Wire'i baslat/yeniden baslat
static void wireStart() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(I2C_CLOCK_SPEED);
    Wire.setTimeOut(50);
    delay(20);
}

// MUX adresini bul ve gercekten MUX oldugunu dogrula. -1 = bulunamadi.
static int detectAddr() {
    // 1) Once beklenen adres (Config.h -> MUX_ADDR, varsayilan 0x70)
    if (pingAddr(MUX_ADDR) && verifyMux(MUX_ADDR)) return MUX_ADDR;

    // 2) Bulunamadiysa tum adres araligini tara (A0/A1/A2 lehimlenmis olabilir)
    for (uint8_t a = 0x70; a <= 0x77; a++) {
        if (a == MUX_ADDR) continue;   // yukarida denendi
        if (pingAddr(a) && verifyMux(a)) return a;
    }
    return -1;
}

void begin() {
    if (_inited) return;
    _inited = true;

    wireStart();

    int found = detectAddr();

    if (found >= 0) {
        _addr = (uint8_t)found;
        _ready = true;
        closeAll();
        if (_addr == MUX_ADDR) {
            Serial.printf("[MUX] Grove 8 kanal MUX bulundu: 0x%02X\n", _addr);
        } else {
            Serial.printf("[MUX] UYARI: MUX 0x%02X adresinde bulundu "
                          "(Config.h MUX_ADDR = 0x%02X). Adres lehimlerini "
                          "kontrol edin.\n", _addr, (uint8_t)MUX_ADDR);
        }
    } else {
        _ready = false;
        Serial.println("[MUX] HATA: 0x70-0x77 araliginda I2C multiplexer "
                       "bulunamadi (kablo/besleme kontrol edin)");
    }
}

bool isReady() {
    return _ready;
}

uint8_t addr() {
    return _addr;
}

bool ping() {
    return pingAddr(_addr);
}

void closeAll() {
    Wire.beginTransmission(_addr);
    Wire.write((uint8_t)0x00);
    Wire.endTransmission();
    delay(2);
}

// Tek bir kanal secim denemesi (kurtarma tetiklemeden)
static bool trySelect(uint8_t ch) {
    closeAll();

    Wire.beginTransmission(_addr);
    Wire.write((uint8_t)(1u << ch));
    uint8_t ec = Wire.endTransmission();
    delay(5);
    return (ec == 0);
}

bool selectChannel(uint8_t ch) {
    if (ch > 7) return false;

    // MUX hic bulunamadiysa bile kurtarmayi dene (gec baglanmis olabilir)
    if (!_ready) {
        if (_inRecovery) return false;
        if (!recover()) return false;
    }

    if (trySelect(ch)) {
        _consecFail = 0;
        return true;
    }

    // Basarisiz: sayaci artir, esige gelince bus'i kurtarip bir kez daha dene
    if (_consecFail < 255) _consecFail++;
    Serial.printf("[MUX] Kanal %u secilemedi (ardisik hata=%u)\n", ch, _consecFail);

    if (_consecFail >= MUX_FAIL_LIMIT && !_inRecovery) {
        if (recover() && trySelect(ch)) {
            _consecFail = 0;
            Serial.printf("[MUX] Kurtarma sonrasi kanal %u OK\n", ch);
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------
//  Bus kurtarma
// ---------------------------------------------------------------------
bool recover() {
    if (_inRecovery) return false;   // yeniden giris korumasi
    _inRecovery = true;
    _recoverCount++;

    Serial.printf("[MUX] Bus kurtarma basliyor (#%lu)...\n",
                  (unsigned long)_recoverCount);

    _ready = false;

    busUnstick();
    wireStart();

    int found = detectAddr();
    if (found >= 0) {
        _addr = (uint8_t)found;
        _ready = true;
        _consecFail = 0;
        closeAll();
        Serial.printf("[MUX] Kurtarma BASARILI, MUX 0x%02X\n", _addr);
    } else {
        Serial.println("[MUX] Kurtarma BASARISIZ - MUX hala yok");
    }

    _inRecovery = false;
    return _ready;
}

uint8_t getFailCount() {
    return _consecFail;
}

uint32_t getRecoverCount() {
    return _recoverCount;
}

bool isHealthy() {
    return _ready && (_consecFail == 0);
}

} // namespace I2CMux
