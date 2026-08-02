#ifndef PHASE_MANAGER_H
#define PHASE_MANAGER_H

#include <Arduino.h>
#include "../config/AnimalProfiles.h"

class PhaseManager {
public:
    PhaseManager();

    void setProfile(uint8_t profileIndex);
    void update(int currentDay);

    const AnimalProfile* getCurrentProfile() const;
    const IncubationPhase* getCurrentPhase() const;
    uint8_t getProfileIndex() const;
    int     getCurrentDay() const;
    int     getTotalDays() const;
    float   getTargetTemperature() const;
    float   getHumidityLow() const;
    float   getHumidityHigh() const;
    bool    isTurningEnabled() const;
    bool    isComplete() const;
    const char* getPhaseName() const;
    int     getPhaseEndDay() const;
    int     getPhaseRemainingDays() const;
    int     getRemainingDays() const;

private:
    const AnimalProfile *_profile;
    const IncubationPhase *_currentPhase;
    uint8_t _profileIndex;
    int     _currentDay;
    bool    _complete;

    const IncubationPhase* findPhase(int day) const;
};

#endif // PHASE_MANAGER_H
