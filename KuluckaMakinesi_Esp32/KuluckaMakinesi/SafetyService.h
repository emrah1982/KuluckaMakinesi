#ifndef SAFETY_SERVICE_H
#define SAFETY_SERVICE_H

#include <Arduino.h>
#include "Config.h"
#include "HeaterDriver.h"
#include "HumidifierDriver.h"
#include "FanDriver.h"

enum SafetyState {
    SAFETY_OK,
    SAFETY_WARNING,
    SAFETY_SHUTDOWN
};

class SafetyService {
public:
    SafetyService(HeaterDriver &heater, HumidifierDriver &humidifier, FanDriver &fan);

    void begin();
    SafetyState check(float temperature, bool sensorOK, uint8_t sensorFailCount,
                      uint8_t sensor1FailCount = 0, uint8_t sensor2FailCount = 0);
    void emergencyShutdown(const String &reason);
    void reset();

    SafetyState getState() const;
    String getShutdownReason() const;
    bool isShutdown() const;

    // ---------- Termal kacis (yapisik role) tespiti ----------
    // emergencyShutdown() cikislari kapatir, ancak role kontagi yapismissa
    // isitici fiziksel olarak calismaya devam eder ve sicaklik dusmez.
    // Bu fonksiyon kapatma sonrasi sicakligi izler; SAFETY_VERIFY_DELAY_MS
    // sonunda beklenen dusus gerceklesmediyse termal kacis ilan eder.
    // Shutdown aktifken her dongude cagrilmalidir.
    void verifyShutdown(float temperature, bool sensorOK);

    bool   isRunaway() const;          // Kapatildi ama sicaklik dusmuyor
    String getRunawayReason() const;

    // I/O arizasi bildirimi: roleler kapatilamiyorsa ust katman bunu cagirir.
    // Sicakliktan bagimsiz olarak dogrudan kacis durumuna gecilir.
    void reportIOFailure(const String &reason);

private:
    HeaterDriver     &_heater;
    HumidifierDriver &_humidifier;
    FanDriver        &_fan;

    SafetyState _state;
    String      _shutdownReason;
    bool        _shutdown;

    // Alt sicaklik sureli izleme
    unsigned long _lowTempStartTime;
    bool          _lowTempActive;

    // Shutdown dogrulama / termal kacis
    unsigned long _shutdownTime;      // emergencyShutdown() zamani
    float         _shutdownTemp;      // Kapatma anindaki sicaklik
    bool          _runaway;           // Termal kacis tespit edildi
    String        _runawayReason;

    void enterRunaway(const String &reason);
};

#endif // SAFETY_SERVICE_H
