#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include "Config.h"

struct SensorData {
    float temperature;
    float humidity;
    bool  valid;
};

enum FusionStatus {
    FUSION_BOTH_OK   = 0,  // Iki sensor de calisiyor, degerler uyumlu
    FUSION_DIVERGED  = 1,  // Iki sensor calisiyor ama degerler uyumsuz
    FUSION_ONLY_S1   = 2,  // Sadece sensor 1 calisiyor (SHT40)
    FUSION_ONLY_S2   = 3,  // Sadece sensor 2 calisiyor (SHT30)
    FUSION_NONE      = 4   // Hicbir sensor calismiyor
};

// Kalibrasyon yapisi
struct SensorCalibration {
    float tempOffset1;  // Sensor 1 (SHT40) sicaklik offset (°C)
    float humOffset1;   // Sensor 1 (SHT40) nem offset (%)
    float tempOffset2;  // Sensor 2 (SHT30) sicaklik offset (°C)
    float humOffset2;   // Sensor 2 (SHT30) nem offset (%)
};

class SensorManager {
public:
    SensorManager();

    bool begin();
    bool readAll();

    float getTemperature() const;
    float getHumidity() const;
    float getRawTemperature() const;
    float getRawHumidity() const;

    bool isSensor1OK() const;
    bool isSensor2OK() const;

    // Sensor acilista FIZIKSEL OLARAK BULUNDU mu?
    // "Takili degil" ile "takili ama arizalandi" ayrimi icin gerekli.
    // Ikisini ayirt etmeyen bir gosterim hic takilmamis sensor icin surekli
    // kirmizi HATA yazar; boylece gercek bir ariza da fark edilmez hale gelir.
    bool isSensor1Present() const { return _sensor1Present; }
    bool isSensor2Present() const { return _sensor2Present; }

    bool isAnyValid() const;
    bool isStale() const;
    uint8_t getFailCount() const;
    uint8_t getFailCount1() const;
    uint8_t getFailCount2() const;
    uint8_t getDivergeCount() const;

    FusionStatus getFusionStatus() const;
    const char*  getFusionStatusStr() const;
    float getSensor1Temp() const;
    float getSensor1Hum() const;
    float getSensor2Temp() const;
    float getSensor2Hum() const;

    // Kalibrasyon
    void setCalibration(const SensorCalibration &cal);
    SensorCalibration getCalibration() const;

private:
    SensorData _data1;          // SHT40 (MUX CH1) - Birincil
    SensorData _data2;          // SHT30 (MUX CH2) - Ikincil

    float _filteredTemp;
    float _filteredHum;
    float _rawTemp;
    float _rawHum;

    bool  _sensorOK;
    bool  _sensor1OK;
    bool  _sensor2OK;
    bool  _sensor1Present;   // begin()'de adres ACK verdi mi (donanim takili mi)
    bool  _sensor2Present;
    uint8_t _failCount;
    uint8_t _failCount1;
    uint8_t _failCount2;
    uint8_t _divergeCount;
    FusionStatus _fusionStatus;

    SensorCalibration _cal;

    unsigned long _lastValidReadTime;
    uint8_t _failoverBlendCounter;

    // ---------- SHT40 (Birincil, MUX CH1) - Wire ile direkt ----------
    bool    sht40Read(float &temp, float &hum);
    uint8_t sht40CRC8(const uint8_t *data, uint8_t len);

    // ---------- SHT30 (Ikincil, MUX CH2) ----------
    bool    sht30ReadOnChannel(uint8_t muxCh, float &temp, float &hum);
    uint8_t sht30CRC8(const uint8_t *data, uint8_t len);

    // ---------- Ortak ----------
    void selectMuxChannel();    // CH1 - SHT40
    void selectMuxChannel2();   // CH2 - SHT30
    void fusionCombine();

    // Filtre
    float applyEMA(float newVal, float oldVal, float alpha);
    bool  isSpikeTemp(float newVal, float oldVal);
    bool  isSpikeHum(float newVal, float oldVal);
};

#endif // SENSOR_MANAGER_H
