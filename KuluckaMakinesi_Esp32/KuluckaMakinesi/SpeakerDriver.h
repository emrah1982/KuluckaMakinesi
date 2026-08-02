#ifndef SPEAKER_DRIVER_H
#define SPEAKER_DRIVER_H

#include <Arduino.h>
#include "Config.h"

// ============================================================
// SPEAKER DRIVER — PAM8403 amplifikator + 1W 8ohm hoparlor
// ============================================================
// ESP32-3248S032 (CYD) kartinda hoparlor cikisi: SPEAKER_PIN (Config.h)
// Sinyal: ledc PWM ile kare dalga (tone). DC component olmaz.
//
// Kullanim:
//   speaker.begin();
//   speaker.startAlarmPattern();   // periyodik beep (alarm icin)
//   speaker.beepOnce(2000, 100);   // tek seferlik (UI feedback)
//   speaker.update();              // loop'ta her cycle cagrilir
//   speaker.stop();                // tum sesi kes
// ============================================================

enum SpeakerMode {
    SPK_IDLE = 0,
    SPK_ONESHOT,        // beepOnce — duration_ms sonra durur
    SPK_ALARM_PATTERN   // periyodik: 200ms ON, 200ms OFF, 3 kere, 1500ms ara
};

class SpeakerDriver {
public:
    SpeakerDriver();
    void begin();
    void update();                          // Non-blocking state machine
    void beepOnce(uint16_t freqHz, uint16_t durationMs);
    void startAlarmPattern();               // Susturulana kadar tekrar
    void stop();                            // Tum sesi kes
    bool isPlaying() const;

private:
    SpeakerMode  _mode;
    unsigned long _stateStartMs;
    uint8_t       _patternStep;             // 0..7 (3 beep + ara icin)
    uint16_t      _alarmFreq;
    bool          _toneActive;
    uint16_t      _oneshotDurationMs;

    void toneOn(uint16_t freq);
    void toneOff();
};

#endif // SPEAKER_DRIVER_H
