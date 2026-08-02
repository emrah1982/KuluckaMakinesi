#include "HumidityController.h"

HumidityController::HumidityController(HumidifierDriver &driver)
    : _driver(driver)
    , _lowThreshold(50.0f)
    , _highThreshold(60.0f)
    , _humidifying(false)
{
}

void HumidityController::setThresholds(float low, float high) {
    _lowThreshold = low;
    _highThreshold = high;
}

void HumidityController::update(float currentHumidity) {
    // Nem düşükse nemlendiriciyi aç
    if (currentHumidity < _lowThreshold && !_humidifying) {
        _driver.turnOn();
        _humidifying = true;
    }

    // Nem yüksekse nemlendiriciyi kapat (gecikmeli)
    if (currentHumidity > _highThreshold && _humidifying) {
        _driver.turnOff();
        _humidifying = false;
    }

    // Gecikmeli kapatma güncelle
    _driver.update();
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
