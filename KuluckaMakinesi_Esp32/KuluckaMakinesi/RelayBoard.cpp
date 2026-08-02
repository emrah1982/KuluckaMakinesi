#include "RelayBoard.h"
#include "I2CMux.h"

// ============================================================
// PCF8574 4'lu Role Modulu - Grove 8 kanal I2C MUX Kanal 7 uzerinden
// Baglanti:
//   MUX CH7 -> PCF8574 (0x20)
//   PCF8574 P0 -> Role 1 (Isitici)
//   PCF8574 P1 -> Role 2 (Nemlendirici)
//   PCF8574 P2 -> Role 3 (Turner Motor Guc)
//   PCF8574 P3 -> Role 4 (Turner Motor Yon)
// Active LOW: LOW=Role ACIK, HIGH=Role KAPALI
// ============================================================

RelayBoard::RelayBoard()
    : _state(RELAY_ALL_OFF)
    , _initialized(false)
    , _lastWriteOK(false)
    , _consecWriteFail(0)
    , _totalWriteFail(0)
{
}

RelayBoard& RelayBoard::instance() {
    static RelayBoard inst;
    return inst;
}

void RelayBoard::selectMuxChannel() {
    I2CMux::selectChannel(MUX_CH_RELAY);
}

// Tek yazma denemesi. MUX kanali secilemezse veya PCF8574 ACK vermezse false.
bool RelayBoard::writeStateOnce() {
    if (!I2CMux::selectChannel(MUX_CH_RELAY)) return false;

    Wire.beginTransmission(PCF8574_ADDR);
    Wire.write(_state);
    return (Wire.endTransmission() == 0);
}

// ---------------------------------------------------------------------
//  Role durumunu yaz + sonucu dogrula
//
//  KRITIK: Bu fonksiyon eskiden endTransmission() donusunu yok sayiyordu.
//  I2C hattinda bir sorun oldugunda "isiticiyi kapat" komutu sessizce
//  kayboluyordu ve PCF8574 son cikis durumunu (isitici ACIK) koruyordu.
//  Artik hata tespit ediliyor, bir kez bus kurtarma ile tekrar deneniyor
//  ve kalici hata ust katmana saglik bayragi ile bildiriliyor.
// ---------------------------------------------------------------------
bool RelayBoard::writeState() {
    if (!_initialized) {
        _lastWriteOK = false;
        return false;
    }

    if (writeStateOnce()) {
        _lastWriteOK = true;
        _consecWriteFail = 0;
        return true;
    }

    _lastWriteOK = false;
    if (_consecWriteFail < 255) _consecWriteFail++;
    _totalWriteFail++;

    // ONEMLI: Bus kurtarmayi ILK hatada tetiklemiyoruz.
    // Isitici rolesi her dongude yazilir. PCF8574 hic TAKILI DEGILSE her
    // yazma basarisiz olur; ilk hatada kurtarma cagirmak, saglam bir bus'i
    // her dongude yikip yeniden kuran sonsuz bir kurtarma dongusu yaratir
    // (sahada tam olarak bu yasandi). Kurtarma ancak ardisik hata esigi
    // asilinca denenir; I2CMux tarafinda ayrica cooldown var.
    bool retried = false;
    if (_consecWriteFail == RELAY_WRITE_FAIL_LIMIT) {
        Serial.println("[RELAY] Ardisik yazma hatasi - bus kurtarma deneniyor");
        if (I2CMux::recover() && writeStateOnce()) {
            _lastWriteOK = true;
            _consecWriteFail = 0;
            Serial.println("[RELAY] Kurtarma sonrasi yazma OK");
            return true;
        }
        retried = true;
    }

    // Log spam'i onle: her dongude degil, esikte ve her 50 hatada bir yaz
    if (retried || (_consecWriteFail % 50) == 0) {
        Serial.printf("[RELAY] YAZMA BASARISIZ! ardisik=%u toplam=%lu "
                      "(role karti takili mi? roleler son durumda kalir)\n",
                      _consecWriteFail, (unsigned long)_totalWriteFail);
    }
    return false;
}

// ---------------------------------------------------------------------
//  Acil kapatma: tum cikislari kapatmak icin israrla dene
// ---------------------------------------------------------------------
bool RelayBoard::forceAllOff() {
    _state = RELAY_ALL_OFF;

    // _initialized false olsa bile denemeye deger: bus sonradan duzelmis olabilir
    for (uint8_t attempt = 1; attempt <= 3; attempt++) {
        if (writeStateOnce()) {
            _lastWriteOK = true;
            _consecWriteFail = 0;
            _initialized = true;
            Serial.printf("[RELAY] Tum roleler KAPATILDI (deneme %u)\n", attempt);
            return true;
        }
        Serial.printf("[RELAY] Kapatma denemesi %u basarisiz, kurtariliyor...\n",
                      attempt);
        I2CMux::recover();
        delay(20);
    }

    _lastWriteOK = false;
    if (_consecWriteFail < 255) _consecWriteFail++;
    _totalWriteFail++;
    Serial.println("[RELAY] KRITIK: Roleler KAPATILAMADI! Fiziksel mudahale gerekli.");
    return false;
}

bool RelayBoard::isHealthy() const {
    return _initialized && _lastWriteOK && (_consecWriteFail == 0);
}

bool RelayBoard::lastWriteOK() const {
    return _lastWriteOK;
}

uint8_t RelayBoard::getWriteFailCount() const {
    return _consecWriteFail;
}

uint32_t RelayBoard::getTotalWriteFails() const {
    return _totalWriteFail;
}

bool RelayBoard::begin() {
    I2CMux::begin();
    if (!I2CMux::isReady()) {
        Serial.println("[RELAY] MUX HATA! (bulunamadi)");
        _initialized = false;
        return false;
    }

    // MUX kanal sec ve PCF8574 kontrol et
    selectMuxChannel();
    Wire.beginTransmission(PCF8574_ADDR);
    uint8_t err = Wire.endTransmission();

    if (err != 0) {
        Serial.print("[RELAY] PCF8574 HATA! (0x");
        Serial.print(PCF8574_ADDR, HEX);
        Serial.print(") err=");
        Serial.println(err);
        _initialized = false;
        return false;
    }

    _initialized = true;
    _state = RELAY_ALL_OFF;
    if (!writeState()) {
        Serial.println("[RELAY] HATA: Baslangic durumu yazilamadi!");
        return false;
    }

    Serial.println("[RELAY] PCF8574 OK (MUX CH" + String(MUX_CH_RELAY) + ")");
    return true;
}

void RelayBoard::setRelay(uint8_t bit, bool on) {
    if (on) {
        _state &= ~(1 << bit);   // Active LOW: LOW = ACIK
    } else {
        _state |= (1 << bit);    // Active LOW: HIGH = KAPALI
    }
    writeState();
}

bool RelayBoard::getRelay(uint8_t bit) const {
    return !(_state & (1 << bit));  // Active LOW: LOW = true (acik)
}

void RelayBoard::setHeater(bool on)            { setRelay(RELAY_BIT_HEATER, on); }
void RelayBoard::setHumidifier(bool on)        { setRelay(RELAY_BIT_HUMIDIFIER, on); }
void RelayBoard::setTurnerPower(bool on)       { setRelay(RELAY_BIT_TURNER_POWER, on); }
void RelayBoard::setTurnerDirection(bool rev)  { setRelay(RELAY_BIT_TURNER_DIR, rev); }

bool RelayBoard::isHeaterOn() const       { return getRelay(RELAY_BIT_HEATER); }
bool RelayBoard::isHumidifierOn() const   { return getRelay(RELAY_BIT_HUMIDIFIER); }
bool RelayBoard::isTurnerPowerOn() const  { return getRelay(RELAY_BIT_TURNER_POWER); }
bool RelayBoard::isTurnerReverse() const  { return getRelay(RELAY_BIT_TURNER_DIR); }

// ---- Generic ekstra pin (P4..P7) — stepper surucu icin ----
// PCF8574 acik-drenajli quasi-bidirectional. Bit=1 -> ~3.3V (zayif pull-up),
// Bit=0 -> GND. Stepper TTL girisleri dogrudan bu seviyeyi okur.
void RelayBoard::setExtraPin(uint8_t bit, bool high) {
    if (bit > 7) return;
    if (high) {
        _state |= (1 << bit);
    } else {
        _state &= ~(1 << bit);
    }
    writeState();
}

bool RelayBoard::getExtraPin(uint8_t bit) const {
    if (bit > 7) return false;
    return (_state & (1 << bit)) != 0;
}

void RelayBoard::pulseExtraPin(uint8_t bit, uint16_t pulseWidthUs) {
    setExtraPin(bit, true);
    if (pulseWidthUs > 0) delayMicroseconds(pulseWidthUs);
    setExtraPin(bit, false);
}
