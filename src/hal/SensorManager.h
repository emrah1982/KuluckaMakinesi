#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include "../config/Config.h"

struct SensorData {
    float temperature;
    float humidity;
    bool  valid;
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
    bool isAnyValid() const;
    uint8_t getFailCount() const;

private:
    SensorData _data1;

    float _filteredTemp;
    float _filteredHum;
    float _rawTemp;
    float _rawHum;

    bool  _sensorOK;
    uint8_t _failCount;

    // Modbus RTU
    bool    modbusReadRegister(uint8_t slaveAddr, uint16_t regAddr, uint16_t regCount, uint8_t *respBuf, uint8_t respLen);
    uint16_t modbusCRC16(const uint8_t *data, uint8_t len);
    void    rs485TxMode();
    void    rs485RxMode();

    // Filtre
    float applyEMA(float newVal, float oldVal, float alpha);
    bool  isSpikeTemp(float newVal, float oldVal);
    bool  isSpikeHum(float newVal, float oldVal);
};

#endif // SENSOR_MANAGER_H
