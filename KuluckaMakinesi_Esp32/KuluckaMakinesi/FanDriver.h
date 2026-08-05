#ifndef FAN_DRIVER_H
#define FAN_DRIVER_H

#include <Arduino.h>
#include "Config.h"

// ============================================================
// FAN PWM SURUCU — Gercek Dunya Davranisi
// ------------------------------------------------------------
// 4-pin PC fani veya 12V DC fan icin 25 kHz PWM cikisi (sessiz).
// Ozellikleri:
//   * Kickstart: Fan kapaliyken acilirken yetersiz tork yuzunden
//     donmemeyi onlemek icin kisa sure (~250 ms) tam PWM uygulanir.
//   * Soft ramp: Ani PWM atlamalarinda mekanik gerilim olusur,
//     hedef PWM ile mevcut PWM arasinda kademeli gecis yapilir.
//   * Off threshold: Cok dusuk PWM'de (< FAN_OFF_THRESHOLD) fan zaten
//     donemez, tamamen kapatilir (titreme/inilti onlenir).
//   * value == 0: Acik aciktan FAN'i tamamen kapatir, FAN_MIN_PWM
//     uygulanmaz (temizlik modu vb. icin).
//
// Kullanim:
//   FanDriver fan;
//   fan.begin();
//   fan.setPWM(150);   // hedef PWM
//   loop() { fan.update(); }   // ramp/kickstart mantigi
// ============================================================

class FanDriver {
public:
    FanDriver();

    void begin();

    // Hedef PWM ayarla (0 = kapali, 1-FAN_MIN_PWM-1 = kapali, FAN_MIN_PWM-255 = aktif).
    // Soft ramp ile kademeli olarak hedeflenir.
    void setPWM(uint8_t value);

    // Hizli kapatma (kickstart bypass, ramp bypass)
    void stop();

    // Loop'tan periyodik cagrilir; ramp/kickstart durumunu gunceller
    void update();

    uint8_t getCurrentPWM() const;       // anlik (gercek) PWM
    uint8_t getTargetPWM()  const;       // istenen hedef PWM

    // Soft start/ramp davranisini ac/kapa (testler veya temizlik modu icin)
    void setSoftBehavior(bool enabled);

private:
    uint8_t _currentPWM;        // Suanda yazilan PWM (0-255)
    uint8_t _targetPWM;         // Istenen hedef PWM (0-255)
    bool    _softEnabled;       // Soft ramp/kickstart aktif mi
    bool    _kickstartActive;   // Kickstart suresi devam ediyor mu
    unsigned long _kickstartEndMs;  // Kickstart bitis zamani
    unsigned long _lastRampMs;  // Son ramp adimi zamani

    // L298N IN1 (PCF8574 P7) son yazilan durum.
    // PWM her ramp adiminda degisir ama IN1 yalnizca ac/kapa gecisinde
    // yazilmalidir; her adimda I2C'ye yazmak bus'i gereksiz mesgul eder.
    bool _in1State;

    void writePWM(uint8_t v);   // PWM cikisi (ledcWrite) + L298N IN1 yonetimi
};

#endif // FAN_DRIVER_H
