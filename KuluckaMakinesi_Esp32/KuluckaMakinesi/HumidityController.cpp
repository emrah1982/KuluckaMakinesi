#include "HumidityController.h"
#include "Config.h"

HumidityController::HumidityController(HumidifierDriver &driver)
    : _driver(driver)
    , _lowThreshold(50.0f)
    , _highThreshold(60.0f)
    , _humidifying(false)
    , _humOnStartMs(0)
    , _humAtStart(0.0f)
    , _ineffective(false)
{
}

void HumidityController::setThresholds(float low, float high) {
    // 0-100% araligina sabitle
    low  = constrain(low,  0.0f, 100.0f);
    high = constrain(high, 0.0f, 100.0f);

    // low > high ise takas et
    if (low > high) {
        float tmp = low; low = high; high = tmp;
    }

    // Minimum aralik: en az 2% fark olmali (rapid cycling onlenir)
    const float MIN_GAP = 2.0f;
    if (high - low < MIN_GAP) {
        float mid = (low + high) * 0.5f;
        low  = constrain(mid - MIN_GAP * 0.5f, 0.0f, 100.0f);
        high = constrain(mid + MIN_GAP * 0.5f, 0.0f, 100.0f);
    }

    _lowThreshold  = low;
    _highThreshold = high;
}

void HumidityController::update(float currentHumidity) {
    // Nem düşükse nemlendiriciyi aç
    if (currentHumidity < _lowThreshold && !_humidifying) {
        _driver.turnOn();
        _humidifying = true;

        // Etkinlik denetimi icin referans al: bu andan itibaren nemin
        // yukselmesi beklenir.
        _humOnStartMs = millis();
        _humAtStart   = currentHumidity;
        _ineffective  = false;
    }

    // Nem yüksekse nemlendiriciyi kapat (gecikmeli)
    if (currentHumidity > _highThreshold && _humidifying) {
        _driver.turnOff();
        _humidifying = false;

        // Hedefe ulasildi: nemlendirici calisiyor demektir
        _humOnStartMs = 0;
        _ineffective  = false;
    }

    // ---- ETKINLIK DENETIMI ----
    // Nemlendirici kesintisiz acik kalip nem beklenen kadar yukselmediyse
    // ariza vardir. En sik sebep su deposunun bosalmasidir: cihaz calisir,
    // role tikirdar, ama buhar uretilmez. Bang-bang kontrol bunu tek basina
    // fark edemez - komutu verip ise yaradigini varsayar.
    if (_humidifying && _humOnStartMs != 0) {
        if ((millis() - _humOnStartMs) >= HUM_EFFECT_TIMEOUT_MS) {
            float rise = currentHumidity - _humAtStart;
            _ineffective = (rise < HUM_EFFECT_MIN_RISE);
            // Pencereyi kaydir: ariza surse de her periyotta yeniden olculur,
            // duzelirse bayrak kendiliginden kalkar.
            _humOnStartMs = millis();
            _humAtStart   = currentHumidity;
        }
    }

    // Gecikmeli kapatma güncelle
    _driver.update();
}

bool HumidityController::isIneffective() const {
    return _ineffective;
}

float HumidityController::getLowThreshold() const {
    return _lowThreshold;
}

float HumidityController::getHighThreshold() const {
    return _highThreshold;
}

bool HumidityController::isHumidifying() const {
    return _humidifying;
}
