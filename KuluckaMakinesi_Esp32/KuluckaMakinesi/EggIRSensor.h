#ifndef EGG_IR_SENSOR_H
#define EGG_IR_SENSOR_H

#include <Arduino.h>
#include <Wire.h>
#include "Config.h"

// =====================================================================
//  EggIRSensor - Yerel kizilotesi yumurta sicakligi (MLX90614ESF-BCC)
//
//  Modul  : "Arduino Kizilotesi Termometre Modulu" (GY-906 tasima karti)
//  Baglanti: Grove 8 kanal I2C MUX - Kanal 4 (MUX_CH_EGG_IR), adres 0x5A
//  Protokol: SMBus (Wire ile repeated-start + PEC dogrulamasi).
//            Harici kutuphane gerekmez.
//
//  Bu sensor yumurta sicakliginin BIRINCIL kaynagidir. Gecersiz olursa
//  IncubationService otomatik olarak EggTempService (WiFi uzerinden ayri
//  ESP32) yedegine duser.
//
//  Kullanim:
//    EggIRSensor _eggIR;
//    _eggIR.begin();
//    ...
//    _eggIR.update();              // loop icinde, kendi araligiyla okur
//    if (_eggIR.isValid()) t = _eggIR.getObjectTemp();
// =====================================================================
class EggIRSensor {
public:
    EggIRSensor();

    // Sensoru baslat: MUX kanalini sec, adresi dogrula, ilk olcumu yap.
    // true = sensor bulundu ve cevap veriyor
    bool begin();

    // Loop'tan cagrilir. EGG_IR_READ_INTERVAL_MS dolmadiysa hicbir sey
    // yapmaz. Donen deger: bu cagrida gercek bir I2C okumasi yapildi mi.
    bool update();

    // Araligi beklemeden hemen oku (begin ve tanilama icin)
    bool forceRead();

    float    getObjectTemp()  const { return _objTemp; }   // Yumurta yuzeyi (°C)
    float    getAmbientTemp() const { return _ambTemp; }   // Sensor govde/ortam (°C)
    bool     isReady()        const { return _ready; }     // Donanim bulundu mu
    bool     isValid()        const { return _valid; }     // Son okuma gecerli mi
    uint32_t getLastReadTime() const { return _lastReadMs; }
    uint8_t  getFailCount()   const { return _failCount; }

    // Referans termometreye gore duzeltme (°C). Kalici degildir; NVS'e
    // yazmak isteyen ust katman kendi kaydeder.
    void  setOffset(float off) { _offset = off; }
    float getOffset() const    { return _offset; }

private:
    // MLX90614 RAM registerinden 16-bit ham deger oku (PEC dogrulamali)
    bool readRaw(uint8_t reg, uint16_t &raw);

    // SMBus PEC hesabi - CRC-8, poly 0x07, init 0x00
    static uint8_t computePEC(const uint8_t *data, uint8_t len);

    // Ham deger -> Celsius
    static float rawToCelsius(uint16_t raw);

    float    _objTemp;
    float    _ambTemp;
    float    _offset;
    bool     _ready;
    bool     _valid;
    uint8_t  _failCount;
    uint32_t _lastReadMs;
};

#endif // EGG_IR_SENSOR_H
