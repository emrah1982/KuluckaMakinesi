#ifndef PHASE_MANAGER_H
#define PHASE_MANAGER_H

#include <Arduino.h>
#include "AnimalProfiles.h"

class PhaseManager {
public:
    PhaseManager();

    void setProfile(uint8_t profileIndex);
    // Override icin: pointer'la profil set + harici index
    // (DisplayManager/UI dogru hayvan ikonunu/indexini bilsin diye).
    // ANIMAL_PROFILE pointer'inin yasam suresi cagiranin sorumlulugundadir;
    // tipik kullanim: IncubationService static buffer tutar.
    void setProfile(const AnimalProfile* prof, uint8_t externalIdx);
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

    // Profesyonel profil (v2) parametreleri — faz bazli turning/cooling/spray
    uint16_t getTurningIntervalMin() const;    // 0 = turning yok
    uint8_t  getTurningDurationSec() const;    // 0 = aci ile hesapla
    uint8_t  getTurningAngleDeg() const;       // Cevirme acisi (derece)
    bool     isCoolingEnabled() const;
    uint8_t  getCoolingDurationMin() const;    // her seans kac dk
    uint8_t  getCoolingPerDay() const;         // gunde kac kere
    bool     isSprayingEnabled() const;
    uint8_t  getSprayingDurationSec() const;

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
