#include "PhaseManager.h"

PhaseManager::PhaseManager()
    : _profile(nullptr)
    , _currentPhase(nullptr)
    , _profileIndex(0)
    , _currentDay(1)
    , _complete(false)
{
}

void PhaseManager::setProfile(uint8_t profileIndex) {
    if (profileIndex >= PROFILE_COUNT) {
        profileIndex = 0;
    }
    _profileIndex = profileIndex;
    _profile = ALL_PROFILES[profileIndex];
    _currentDay = 1;
    _complete = false;

    Serial.print("[PHASE] Profil secildi: ");
    Serial.print(_profile->name);
    Serial.print(" (");
    Serial.print(_profile->totalDays);
    Serial.println(" gun)");

    update(1);
}

void PhaseManager::update(int currentDay) {
    _currentDay = currentDay;

    if (_profile == nullptr) return;

    if (currentDay > _profile->totalDays) {
        _complete = true;
        Serial.println("[PHASE] KULUCKA TAMAMLANDI!");
        return;
    }

    const IncubationPhase* phase = findPhase(currentDay);
    if (phase != _currentPhase && phase != nullptr) {
        _currentPhase = phase;
        Serial.print("[PHASE] Evre degisti: ");
        Serial.print(phase->phaseName);
        Serial.print(" (Gun ");
        Serial.print(phase->startDay);
        Serial.print("-");
        Serial.print(phase->endDay);
        Serial.print(") Temp=");
        Serial.print(phase->temperature);
        Serial.print(" Nem=");
        Serial.print(phase->humidityLow);
        Serial.print("-");
        Serial.println(phase->humidityHigh);
    }
}

const IncubationPhase* PhaseManager::findPhase(int day) const {
    if (_profile == nullptr) return nullptr;

    for (uint8_t i = 0; i < _profile->phaseCount; i++) {
        const IncubationPhase &p = _profile->phases[i];
        if (day >= p.startDay && day <= p.endDay) {
            return &p;
        }
    }
    // Son evreyi döndür
    if (_profile->phaseCount > 0) {
        return &_profile->phases[_profile->phaseCount - 1];
    }
    return nullptr;
}

const AnimalProfile* PhaseManager::getCurrentProfile() const {
    return _profile;
}

const IncubationPhase* PhaseManager::getCurrentPhase() const {
    return _currentPhase;
}

uint8_t PhaseManager::getProfileIndex() const {
    return _profileIndex;
}

int PhaseManager::getCurrentDay() const {
    return _currentDay;
}

int PhaseManager::getTotalDays() const {
    if (_profile) return _profile->totalDays;
    return 0;
}

float PhaseManager::getTargetTemperature() const {
    if (_currentPhase) return _currentPhase->temperature;
    return 37.5f;
}

float PhaseManager::getHumidityLow() const {
    if (_currentPhase) return _currentPhase->humidityLow;
    return 50.0f;
}

float PhaseManager::getHumidityHigh() const {
    if (_currentPhase) return _currentPhase->humidityHigh;
    return 60.0f;
}

bool PhaseManager::isTurningEnabled() const {
    if (_currentPhase) return _currentPhase->turningEnabled;
    return false;
}

bool PhaseManager::isComplete() const {
    return _complete;
}

const char* PhaseManager::getPhaseName() const {
    if (_currentPhase) return _currentPhase->phaseName;
    return "Bilinmiyor";
}

int PhaseManager::getPhaseEndDay() const {
    if (_currentPhase) return _currentPhase->endDay;
    return 0;
}

int PhaseManager::getPhaseRemainingDays() const {
    if (_currentPhase) {
        int rem = _currentPhase->endDay - _currentDay;
        return (rem > 0) ? rem : 0;
    }
    return 0;
}

int PhaseManager::getRemainingDays() const {
    if (_profile) {
        int rem = _profile->totalDays - _currentDay;
        return (rem > 0) ? rem : 0;
    }
    return 0;
}
