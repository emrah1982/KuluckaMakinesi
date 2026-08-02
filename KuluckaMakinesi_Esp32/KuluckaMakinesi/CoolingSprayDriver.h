#ifndef COOLING_SPRAY_DRIVER_H
#define COOLING_SPRAY_DRIVER_H

#include <Arduino.h>

// ============================================================
// CoolingSprayDriver — Profesyonel kuluckalarda gunluk sogutma
// ve su puskurtme (ozellikle ordek/kaz/devekusu).
//
// Mimari (Secenek B — mevcut donanimla simule):
//   COOLING   -> IncubationService heater'i 0'a ceker + fan PWM=255
//   SPRAYING  -> IncubationService humidifier'i zorla ON tutar
// Bu driver saat bazli zamanlama + state machine yonetir,
// donanim override kararlarini flag'ler uzerinden disariya verir.
//
// Slot planlamasi:
//   perDay=1 -> saat 12:00'de tek seans
//   perDay=2 -> 08:00 ve 20:00'de iki seans
// Slot baslatma penceresi 5 dakika (gerisi loop zamanlamasina guvenilir).
//
// Bir sogutma seansi: [COOLING durationMin dk] -> (sprayEnabled ise)
// [SPRAYING durationSec sn] -> IDLE. Sprey cooling hemen bitince gelir.
// ============================================================

enum CoolingSprayMode {
    CS_IDLE,          // aktif islem yok
    CS_COOLING,       // sogutma sureci (heater=0, fan=full)
    CS_SPRAYING       // sogutma sonrasi su puskurtme (humidifier ON)
};

class CoolingSprayDriver {
public:
    CoolingSprayDriver();

    void begin();

    // Her loop'tan cagrilir.
    // rtcHour/rtcMin: su anki saat:dk (RTC'den)
    // rtcDay: bugunku takvim gunu (1-31) — slot takibi icin
    // coolingEnabled: faz cooling istiyor mu
    // coolingDurationMin: her seans kac dk
    // coolingPerDay: gunde kac kere (1 veya 2)
    // sprayEnabled: sogutma sonu sprey var mi
    // sprayDurationSec: sprey kac sn
    void update(uint8_t rtcHour, uint8_t rtcMin, uint8_t rtcDay,
                bool coolingEnabled, uint8_t coolingDurationMin, uint8_t coolingPerDay,
                bool sprayEnabled, uint8_t sprayDurationSec);

    // Mevcut mod
    CoolingSprayMode getMode() const;

    // Override flag'leri — IncubationService bunlara bakip donanim kontrol eder
    bool shouldOverrideHeater() const;     // COOLING'de heater=0
    bool shouldOverrideFan() const;        // COOLING'de fan=full
    bool shouldOverrideHumidifier() const; // SPRAYING'de humidifier=ON

    // Durum bilgisi
    uint16_t getSecondsInState() const;    // Su anki durumda kac sn gecti
    uint16_t getCoolingRemainSec() const;  // COOLING'den cikis icin kalan sn
    uint8_t  getLastSlotDone() const;      // Bugun tamamlanan son slot (0/1/255=yok)

    // Manuel durdurma (acil/pause/stop)
    void cancel();

private:
    CoolingSprayMode _mode;
    unsigned long    _stateStartMs;

    uint8_t          _lastSlotDay;    // slotlarin resetlenecegi gun
    uint8_t          _lastSlotDone;   // bugun son tamamlanan slot (255=hicbir)

    // Cache'li parametreler (tek tetiklemede sabit kalmalari icin)
    uint8_t          _activeCoolingMin;
    uint8_t          _activeSprayEnabled;
    uint8_t          _activeSpraySec;

    // Slot penceresi kontrolu (perDay'e gore)
    bool inSlotWindow(uint8_t rtcHour, uint8_t rtcMin, uint8_t perDay, uint8_t &slotOut) const;
};

#endif // COOLING_SPRAY_DRIVER_H
