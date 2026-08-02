#ifndef TURNER_DRIVER_H
#define TURNER_DRIVER_H

#include <Arduino.h>
#include "../config/Config.h"

// Yumurta cevirme motoru durumu
enum TurnerState {
    TURNER_IDLE,        // Bekleme
    TURNER_FORWARD,     // Ileri yon cevirme
    TURNER_PAUSE,       // Yon degistirme arasi
    TURNER_REVERSE,     // Geri yon cevirme
    TURNER_DISABLED     // Cikim fazinda devre disi
};

// L298N ile yumurta cevirme motoru kontrolu
// Cikim (lockdown) fazinda otomatik durur
class TurnerDriver {
public:
    TurnerDriver();

    void begin();
    void update(bool turningEnabled);  // Her loop'ta cagrilmali
    void stop();
    void forceStop();                  // Acil durdurma

    bool isRunning() const;
    bool isDisabled() const;
    TurnerState getState() const;
    unsigned long getLastTurnTime() const;
    uint16_t getTurnCount() const;

private:
    TurnerState _state;
    unsigned long _lastTurnTime;
    unsigned long _stateStartTime;
    uint16_t _turnCount;
    bool _wasEnabled;

    void setMotor(bool forward);
    void stopMotor();
};

#endif // TURNER_DRIVER_H
