#ifndef TURNER_DRIVER_H
#define TURNER_DRIVER_H

#include <Arduino.h>
#include "Config.h"
#include "RelayBoard.h"

// Yumurta cevirme motoru durumu
// Gercek kuluçka: yumurta bir yone cevrilir, o acida 1-2 saat bekler,
// sonra ters yone cevrilir ve tekrar bekler.
enum TurnerState {
    TURNER_IDLE,        // Motor kapali, bir sonraki cevirme zamanini bekliyor
    TURNER_FORWARD,     // Motor ileri yon calisiyor (aciya getiriyor)
    TURNER_HOLD_FWD,    // Ileri acida bekleme (pozisyon tutma)
    TURNER_PAUSE,       // Yon degistirme arasi bekleme
    TURNER_REVERSE,     // Motor geri yon calisiyor (ters aciya getiriyor)
    TURNER_HOLD_REV,    // Geri acida bekleme (pozisyon tutma)
    TURNER_DISABLED     // Cikim fazinda devre disi
};

// PCF8574 role ile yumurta cevirme motoru kontrolu
// Role 3 (P2): Motor guc ON/OFF
// Role 4 (P3): Motor yon (OFF=Ileri, ON=Geri)
// Cikim (lockdown) fazinda otomatik durur
class TurnerDriver {
public:
    TurnerDriver();

    void begin();
    // Her loop'ta cagrilmali. intervalMin=0 veya enabled=false -> turning kapali.
    // durationSec=0 ise aciya gore hesaplanir: angleDeg / TURNER_DEG_PER_SEC.
    // angleDeg=0 ise TURNER_DEFAULT_ANGLE_DEG (Config.h) kullanilir.
    // Her iki sifir kalirsa son care olarak TURNER_DURATION_MS kullanilir.
    void update(bool turningEnabled,
                uint16_t intervalMin = 0,
                uint8_t  durationSec = 0,
                uint8_t  angleDeg    = 0);
    void stop();
    void forceStop();                  // Acil durdurma

    bool isRunning() const;
    bool isDisabled() const;
    TurnerState getState() const;
    unsigned long getLastTurnTime() const;
    uint16_t getTurnCount() const;

    // Pozisyonda bekleme süresi (ms) ayarla/oku
    void setHoldDuration(unsigned long holdMs);
    unsigned long getHoldDuration() const;

private:
    TurnerState _state;
    unsigned long _lastTurnTime;
    unsigned long _stateStartTime;
    uint16_t _turnCount;
    bool _wasEnabled;

    unsigned long _holdDurationMs; // Pozisyonda bekleme süresi (ms)

    void setMotor(bool forward);
    void stopMotor();
};

#endif // TURNER_DRIVER_H
