#include "RelayBoard.h"

// ============================================================
// PCF8574 4'lu Role Modulu - TCA9548A MUX Kanal 1 uzerinden
// Baglanti:
//   TCA9548A CH1 -> PCF8574 (0x20)
//   PCF8574 P0 -> Role 1 (Isitici)
//   PCF8574 P1 -> Role 2 (Nemlendirici)
//   PCF8574 P2 -> Role 3 (Alarm Buzzer)
//   PCF8574 P3 -> Role 4 (Durum LED)
// Active LOW: LOW=Role ACIK, HIGH=Role KAPALI
// ============================================================

RelayBoard::RelayBoard()
    : _state(RELAY_ALL_OFF)
    , _initialized(false)
{
}

RelayBoard& RelayBoard::instance() {
    static RelayBoard inst;
    return inst;
}

void RelayBoard::selectMuxChannel() {
    Wire.beginTransmission(MUX_ADDR);
    Wire.write(1 << MUX_CH_RELAY);
    Wire.endTransmission();
}

void RelayBoard::writeState() {
    if (!_initialized) return;
    selectMuxChannel();
    Wire.beginTransmission(PCF8574_ADDR);
    Wire.write(_state);
    Wire.endTransmission();
}

bool RelayBoard::begin() {
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
    writeState();

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

void RelayBoard::setHeater(bool on)     { setRelay(RELAY_BIT_HEATER, on); }
void RelayBoard::setHumidifier(bool on) { setRelay(RELAY_BIT_HUMIDIFIER, on); }
void RelayBoard::setBuzzer(bool on)     { setRelay(RELAY_BIT_BUZZER, on); }
void RelayBoard::setLED(bool on)        { setRelay(RELAY_BIT_LED, on); }

bool RelayBoard::isHeaterOn() const     { return getRelay(RELAY_BIT_HEATER); }
bool RelayBoard::isHumidifierOn() const { return getRelay(RELAY_BIT_HUMIDIFIER); }
bool RelayBoard::isBuzzerOn() const     { return getRelay(RELAY_BIT_BUZZER); }
bool RelayBoard::isLEDOn() const        { return getRelay(RELAY_BIT_LED); }
