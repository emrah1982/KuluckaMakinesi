#ifndef RELAY_BOARD_H
#define RELAY_BOARD_H

#include <Arduino.h>
#include <Wire.h>
#include "Config.h"

// PCF8574 uzerinden 4'lu role modulu yonetimi (Grove 8 kanal I2C MUX CH7)
// Active LOW: LOW=Role ACIK, HIGH=Role KAPALI
// P0=Isitici, P1=Nemlendirici, P2=Turner Guc, P3=Turner Yon
class RelayBoard {
public:
    static RelayBoard& instance();

    bool begin();

    void setHeater(bool on);
    void setHumidifier(bool on);
    void setTurnerPower(bool on);
    void setTurnerDirection(bool reverse);  // false=Ileri, true=Geri

    bool isHeaterOn() const;
    bool isHumidifierOn() const;
    bool isTurnerPowerOn() const;
    bool isTurnerReverse() const;

    // ----- Generic ekstra pin API (P4..P7 icin) -----
    // Stepper surucu (A4988/DRV8825) icin: pin TTL seviyesi dogrudan yazilir.
    // setExtraPin(bit, true)  -> pin HIGH (~Vcc)
    // setExtraPin(bit, false) -> pin LOW  (GND)
    // bit aralik: 0..7. Bu cagri role bitlerini etkilememelidir;
    // role bitleri (0..3) icin role API'leri kullanilmalidir.
    void setExtraPin(uint8_t bit, bool high);
    bool getExtraPin(uint8_t bit) const;

    // Stepper STEP icin minik latency: tek I2C transaction iki bit set/clear.
    // Daha hizli pulse uretimi gerekirse pulseExtraPin() kullanilir.
    void pulseExtraPin(uint8_t bit, uint16_t pulseWidthUs);

    // ----- I/O saglik takibi -----
    // KRITIK: I2C yazmasi basarisiz olursa PCF8574 son cikis durumunu korur.
    // Isitici ACIK iken bu olursa isitici ACIK KALIR. Bu yuzden her yazmanin
    // sonucu izlenir; ust katman (IncubationService) saglik bozulunca alarm
    // uretir ve guvenlik servisini tetikler.
    bool     isHealthy() const;            // Son yazmalar basarili mi
    bool     lastWriteOK() const;          // Yalnizca son yazma
    uint8_t  getWriteFailCount() const;    // Art arda hata sayisi
    uint32_t getTotalWriteFails() const;   // Acilistan beri toplam hata

    // Tum cikislari kapatmayi ZORLA dener: hata halinde I2C bus'i kurtarip
    // tekrar dener. Acil kapatma yolunda kullanilir. Basarili ise true.
    bool forceAllOff();

private:
    RelayBoard();
    uint8_t _state;  // PCF8574 cikis durumu (Active LOW)
    bool    _initialized;

    // Saglik sayaclari
    bool     _lastWriteOK;
    uint8_t  _consecWriteFail;
    uint32_t _totalWriteFail;

    void selectMuxChannel();
    bool writeState();               // true = I2C yazmasi ACK aldi
    bool writeStateOnce();           // tek deneme (kurtarma tetiklemez)
    void setRelay(uint8_t bit, bool on);
    bool getRelay(uint8_t bit) const;
};

#endif // RELAY_BOARD_H
