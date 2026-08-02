#ifndef CO2_SENSOR_H
#define CO2_SENSOR_H

#include <Arduino.h>
#include "Config.h"

// CO2 Sensor tipleri
enum CO2SensorType {
    CO2_SENSOR_NONE,      // Sensor yok veya tespit edilemedi
    CO2_SENSOR_SCD30,     // Sensirion SCD30 (I2C)
    CO2_SENSOR_MHZ19      // Winsen MH-Z19B (UART - I2C-UART bridge ile)
};

class CO2Sensor {
public:
    CO2Sensor();
    
    bool begin();                    // Sensoru baslat ve tipini tespit et
    bool read();                     // Yeni okuma yap
    
    uint16_t getCO2() const;         // CO2 degeri (ppm)
    float getTemperature() const;    // Sicaklik (SCD30 icin)
    float getHumidity() const;       // Nem (SCD30 icin)
    
    bool isReady() const;            // Sensor hazir mi
    bool isValid() const;            // Son okuma gecerli mi
    CO2SensorType getType() const;   // Sensor tipi
    const char* getTypeName() const; // Sensor tipi adi
    
    // Kalibrasyon
    void setAltitudeCompensation(uint16_t altitude);  // Rakım (metre)
    void forceRecalibration(uint16_t reference);      // Zorla kalibrasyon (ppm)
    void setAutoCalibration(bool enable);             // Otomatik kalibrasyon

private:
    CO2SensorType _type;
    uint16_t _co2;
    float _temperature;
    float _humidity;
    bool _ready;
    bool _valid;
    unsigned long _lastReadTime;
    
    // SCD30 I2C fonksiyonlari
    bool scd30Begin();
    bool scd30Read();
    bool scd30IsDataReady();
    bool scd30SendCommand(uint16_t cmd);
    bool scd30SendCommand(uint16_t cmd, uint16_t arg);
    bool scd30ReadRegister(uint16_t cmd, uint8_t* data, uint8_t len);
    uint8_t scd30CRC8(const uint8_t* data, uint8_t len);
};

#endif // CO2_SENSOR_H
