#include "SpeakerDriver.h"

// LEDC channel — PWM/diger surucularla cakismayacak yuksek index sec
#ifndef SPEAKER_LEDC_CHANNEL
#define SPEAKER_LEDC_CHANNEL  6
#endif

#ifndef SPEAKER_LEDC_RES_BITS
#define SPEAKER_LEDC_RES_BITS 8
#endif

SpeakerDriver::SpeakerDriver()
    : _mode(SPK_IDLE)
    , _stateStartMs(0)
    , _patternStep(0)
    , _alarmFreq(SPEAKER_ALARM_FREQ)
    , _toneActive(false)
    , _oneshotDurationMs(0)
{
}

void SpeakerDriver::begin() {
#if SPEAKER_ENABLED
    // E32R28T board: PAM8403 amplifikator enable pini (GPIO4).
    // LOW = enable (amplifikator acik), HIGH = disable (sessiz, dusuk guc).
    // Boot sirasinda HIGH set ediyoruz ki ses cikmasin; tone calarken LOW.
    pinMode(SPEAKER_AMP_EN_PIN, OUTPUT);
    digitalWrite(SPEAKER_AMP_EN_PIN, HIGH);   // amplifikator KAPALI baslat

    // ESP32 Arduino core 3.x: ledcAttach(pin, freq, res) tek satir
    ledcAttach(SPEAKER_PIN, 2000, SPEAKER_LEDC_RES_BITS);
    ledcWrite(SPEAKER_PIN, 0);   // sessiz duty
    Serial.print("[SPK] Hoparlor pin=");
    Serial.print(SPEAKER_PIN);
    Serial.print(" amp_en=");
    Serial.println(SPEAKER_AMP_EN_PIN);
#endif
}

void SpeakerDriver::toneOn(uint16_t freq) {
#if SPEAKER_ENABLED
    if (freq < 100) freq = 100;
    if (freq > 8000) freq = 8000;
    // Amplifikatoru ac (LOW = enable)
    digitalWrite(SPEAKER_AMP_EN_PIN, LOW);
    ledcWriteTone(SPEAKER_PIN, freq);
    // %50 duty (kare dalga) — 8 bit res icin 128
    ledcWrite(SPEAKER_PIN, 128);
    _toneActive = true;
#endif
}

void SpeakerDriver::toneOff() {
#if SPEAKER_ENABLED
    ledcWrite(SPEAKER_PIN, 0);
    // Amplifikatoru kapat (HIGH = disable) — guc tasarrufu + tik gurultusu yok
    digitalWrite(SPEAKER_AMP_EN_PIN, HIGH);
    _toneActive = false;
#endif
}

void SpeakerDriver::beepOnce(uint16_t freqHz, uint16_t durationMs) {
#if SPEAKER_ENABLED
    _mode = SPK_ONESHOT;
    _stateStartMs = millis();
    _oneshotDurationMs = durationMs;
    toneOn(freqHz);
#endif
}

void SpeakerDriver::startAlarmPattern() {
#if SPEAKER_ENABLED
    if (_mode == SPK_ALARM_PATTERN) return;
    _mode = SPK_ALARM_PATTERN;
    _patternStep = 0;
    _stateStartMs = millis();
    toneOn(_alarmFreq);
#endif
}

void SpeakerDriver::stop() {
    _mode = SPK_IDLE;
    toneOff();
}

bool SpeakerDriver::isPlaying() const {
    return _toneActive;
}

void SpeakerDriver::update() {
#if SPEAKER_ENABLED
    if (_mode == SPK_IDLE) return;
    unsigned long now = millis();
    unsigned long elapsed = now - _stateStartMs;

    if (_mode == SPK_ONESHOT) {
        if (elapsed >= _oneshotDurationMs) {
            toneOff();
            _mode = SPK_IDLE;
        }
        return;
    }

    // SPK_ALARM_PATTERN: 3 beep (200ms ON + 200ms OFF), sonra 1500ms ara, tekrar
    // Step: 0=on,1=off,2=on,3=off,4=on,5=off,6=longpause -> tekrar
    if (_mode == SPK_ALARM_PATTERN) {
        unsigned long stepDur;
        switch (_patternStep) {
            case 0: case 2: case 4: stepDur = 200; break;  // ON
            case 1: case 3:         stepDur = 200; break;  // OFF arasi
            case 5:                 stepDur = 200; break;  // son OFF
            case 6:                 stepDur = 1500; break; // uzun ara
            default:                stepDur = 200; break;
        }
        if (elapsed < stepDur) return;

        _patternStep = (_patternStep + 1) % 7;
        _stateStartMs = now;
        bool shouldBeOn = (_patternStep == 0) || (_patternStep == 2) || (_patternStep == 4);
        if (shouldBeOn) toneOn(_alarmFreq);
        else            toneOff();
    }
#endif
}
