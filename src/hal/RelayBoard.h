#ifndef RELAY_BOARD_H
#define RELAY_BOARD_H

#include <Arduino.h>
#include <Wire.h>
#include "../config/Config.h"

// PCF8574 uzerinden 4'lu role modulu yonetimi (TCA9548A MUX CH1)
// Active LOW: LOW=Role ACIK, HIGH=Role KAPALI
class RelayBoard {
public:
    static RelayBoard& instance();

    bool begin();

    void setHeater(bool on);
    void setHumidifier(bool on);
    void setBuzzer(bool on);
    void setLED(bool on);

    bool isHeaterOn() const;
    bool isHumidifierOn() const;
    bool isBuzzerOn() const;
    bool isLEDOn() const;

private:
    RelayBoard();
    uint8_t _state;  // PCF8574 cikis durumu (Active LOW)
    bool    _initialized;

    void selectMuxChannel();
    void writeState();
    void setRelay(uint8_t bit, bool on);
    bool getRelay(uint8_t bit) const;
};

#endif // RELAY_BOARD_H
