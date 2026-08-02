// pca9548a.h
#pragma once
#include <Wire.h>

#define PCA9548A_BASE_ADDR  0x70   // A2=0,A1=0,A0=0
#define PCA9548A_CH_NONE    0x00   // Tüm kanalları kapat

class PCA9548A {
public:
    PCA9548A(uint8_t addr = PCA9548A_BASE_ADDR, TwoWire* wire = &Wire)
        : _addr(addr), _wire(wire) {}

    void setAddress(uint8_t addr) {
        _addr = addr;
    }

    void begin() {
        closeAll();
    }

    bool selectChannel(uint8_t ch) {
        if (ch > 7) return false;
        return writeRegister(1 << ch);
    }

    void closeAll() {
        writeRegister(PCA9548A_CH_NONE);
    }

    bool selectChannels(uint8_t mask) {
        return writeRegister(mask);
    }

    uint8_t readChannels() {
        _wire->requestFrom(_addr, (uint8_t)1);
        if (_wire->available()) return _wire->read();
        return 0xFF;
    }

private:
    uint8_t  _addr;
    TwoWire* _wire;

    bool writeRegister(uint8_t val) {
        _wire->beginTransmission(_addr);
        _wire->write(val);
        return (_wire->endTransmission() == 0);
    }
};
