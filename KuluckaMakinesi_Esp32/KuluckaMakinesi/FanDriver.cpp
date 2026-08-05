#include "FanDriver.h"
#include "RelayBoard.h"   // L298N IN1, PCF8574 P7 uzerinden surulur

// ============================================================
// FanDriver — Gercek Dunya PWM Fan Surucusu
// ------------------------------------------------------------
// Fan bir L298N motor surucu uzerinden calisir:
//   ESP32 IO18 -> ENA  : 25 kHz PWM (hiz). Insan kulagi duymaz.
//   PCF8574 P7 -> IN1  : yon/enable mantik girisi
//   L298N IN2  -> GND  : tek yon calisma. BOSTA BIRAKILMAMALI, aksi
//                        halde donus yonu belirsiz olur.
// IN1 LOW iken motor donmez; PWM tek basina yeterli degildir.
// ============================================================

FanDriver::FanDriver()
    : _currentPWM(0)
    , _targetPWM(0)
    , _softEnabled(true)
    , _kickstartActive(false)
    , _kickstartEndMs(0)
    , _lastRampMs(0)
    , _in1State(true)   // begin()'de ilk writePWM(0) IN1'i kesin LOW yapsin
{
}

void FanDriver::begin() {
    ledcAttach(FAN_PIN, PWM_FREQ_FAN, PWM_RESOLUTION);
    writePWM(0);                  // Baslangicta KAPALI (kickstart de uygulanmaz)
    _currentPWM = 0;
    _targetPWM = 0;
    _kickstartActive = false;
    _lastRampMs = millis();
    Serial.println("[FAN] Baslatildi (25 kHz PWM, soft start aktif)");
}

void FanDriver::setPWM(uint8_t value) {
    // 0 -> tamamen kapat (FAN_MIN_PWM uygulanmaz)
    if (value == 0) {
        _targetPWM = 0;
        return;
    }
    // Cok dusuk degerler fanin donmesine yetmez -> kapat
    if (value < FAN_OFF_THRESHOLD) {
        _targetPWM = 0;
        return;
    }
    // Aktif aralikta minimum hizi zorla (donmesini garantile)
    if (value < FAN_MIN_PWM) value = FAN_MIN_PWM;

    // Soft mod kapaliyken anlik uygulanir
    if (!_softEnabled) {
        _targetPWM = value;
        _currentPWM = value;
        writePWM(value);
        return;
    }

    // KICKSTART: Fan tamamen duruyorsa (currentPWM == 0), fan rotorunun
    // donmesi icin yetersiz tork olusmasin diye kisa sure tam PWM uygula.
    if (_currentPWM == 0 && value > 0) {
        _kickstartActive = true;
        _kickstartEndMs = millis() + FAN_KICKSTART_MS;
        writePWM(255);              // tam guc kickstart icin
        _currentPWM = 255;
    }
    _targetPWM = value;
    _lastRampMs = millis();
}

void FanDriver::stop() {
    // Hizli kapatma (kickstart/ramp bypass)
    _targetPWM = 0;
    _currentPWM = 0;
    _kickstartActive = false;
    writePWM(0);
}

void FanDriver::update() {
    if (!_softEnabled) return;     // soft mod kapali ise update gereksiz

    unsigned long now = millis();

    // KICKSTART asamasi: bitene kadar bekle
    if (_kickstartActive) {
        if ((long)(now - _kickstartEndMs) >= 0) {
            // Kickstart bitti, hedef PWM'e atla (sonraki ramp'lar buradan basliyor)
            _kickstartActive = false;
            _currentPWM = _targetPWM;
            writePWM(_currentPWM);
            _lastRampMs = now;
        }
        return;                     // kickstart bitmediyse degisme yok
    }

    // RAMP: hedefe kademeli olarak yaklas
    if (_currentPWM == _targetPWM) return;
    if (now - _lastRampMs < FAN_RAMP_INTERVAL_MS) return;
    _lastRampMs = now;

    if (_currentPWM < _targetPWM) {
        // Yukselen ramp
        uint16_t next = (uint16_t)_currentPWM + FAN_RAMP_STEP;
        _currentPWM = (next > _targetPWM) ? _targetPWM : (uint8_t)next;
    } else {
        // Dusen ramp
        if (_currentPWM <= FAN_RAMP_STEP) {
            _currentPWM = _targetPWM;
        } else {
            uint16_t next = (uint16_t)_currentPWM - FAN_RAMP_STEP;
            _currentPWM = (next < _targetPWM) ? _targetPWM : (uint8_t)next;
        }
    }
    writePWM(_currentPWM);
}

uint8_t FanDriver::getCurrentPWM() const {
    return _currentPWM;
}

uint8_t FanDriver::getTargetPWM() const {
    return _targetPWM;
}

void FanDriver::setSoftBehavior(bool enabled) {
    _softEnabled = enabled;
    if (!enabled) {
        // Soft mod kapatildi -> hedef PWM'e an itibariyle atla
        _kickstartActive = false;
        _currentPWM = _targetPWM;
        writePWM(_currentPWM);
    }
}

void FanDriver::writePWM(uint8_t v) {
#if FAN_SPEED_CONTROL
    // IO18 -> ENA bagli: hiz PWM ile ayarlanir
    ledcWrite(FAN_PIN, v);
#else
    // IO18 -> ENA BAGLI DEGIL. ENA jumper'i takili oldugu icin surucu
    // surekli aktif; hiz P7 ile ayarlanamaz (I2C pini, 25 kHz uretilemez).
    // Pini yine de tanimli tutuyoruz ki bosta salinmasin, ama degeri
    // yalnizca ac/kapa anlamina gelir.
    ledcWrite(FAN_PIN, v > 0 ? 255 : 0);
#endif

    // L298N IN1 (PCF8574 P7). ENA'daki PWM tek basina yetmez: IN1 LOW iken
    // motor donmez. Fan gercekten dursun diye kapanista IN1 de LOW yapilir,
    // boylece durdurma tek bir sinyale bagli kalmaz.
    //
    // Yalnizca AC/KAPA gecisinde yaziyoruz: writePWM() ramp sirasinda cok
    // sik cagriliyor ve her cagrida I2C'ye yazmak MUX'u bosuna mesgul eder.
    bool wantOn = (v > 0);
    if (wantOn != _in1State) {
        RelayBoard::instance().setExtraPin(FAN_BIT_IN1, wantOn);
        _in1State = wantOn;
    }
}
