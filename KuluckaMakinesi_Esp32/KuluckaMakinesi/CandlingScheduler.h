#ifndef CANDLING_SCHEDULER_H
#define CANDLING_SCHEDULER_H

#include <Arduino.h>
#include "AnimalProfiles.h"

// ============================================================
// YUMURTA DOL KONTROLU (CANDLING) ZAMANLAYICI
// ------------------------------------------------------------
// Her tur icin otomatik kontrol gunleri hesaplar:
//   1. Kontrol = toplam surenin %25'i
//   2. Kontrol = toplam surenin %50'si
//   3. Kontrol = toplam surenin %75'i
//   Son Kontrol = Lockdown gunu (cevirme kapali faz baslangici)
//
// Lockdown gunu AnimalProfile'dan otomatik cikarilir:
// son fazin startDay'i (turningEnabled=false olan faz).
//
// Kullanim:
//   CandlingSchedule sched;
//   buildCandlingSchedule(&PROFILE_TAVUK, sched);
//   // sched.checkDays[0..3] ve sched.labels[0..3] hazir
//
//   if (isCandlingDay(sched, currentDay)) {
//       // alarm/bildirim goster
//   }
// ============================================================

#define CANDLING_MAX_CHECKS  4   // %25, %50, %75, lockdown

struct CandlingSchedule {
    const char* speciesName;     // "Tavuk" vb.
    uint8_t     totalDays;       // Toplam kulucka suresi
    uint8_t     lockdownDay;     // Cikim (lockdown) baslangic gunu
    uint8_t     checkCount;      // Kontrol sayisi (genelde 4)
    uint8_t     checkDays[CANDLING_MAX_CHECKS];   // Kontrol gunleri
    const char* labels[CANDLING_MAX_CHECKS];      // "1.Kontrol" vb.
    bool        triggered[CANDLING_MAX_CHECKS];    // Bugun tetiklendi mi
};

// Lockdown gununu profilden cikar: turningEnabled=false olan son fazin startDay'i
// Bulamazsa totalDays - 3 (varsayilan)
static uint8_t getLockdownDay(const AnimalProfile* prof) {
    if (!prof) return 0;
    // Son fazdan geriye dogru ara (turning kapali = cikim fazı)
    for (int i = prof->phaseCount - 1; i >= 0; i--) {
        if (!prof->phases[i].turningEnabled && prof->phases[i].startDay > 0) {
            return prof->phases[i].startDay;
        }
    }
    // Fallback: toplam - 3
    return (prof->totalDays > 3) ? (prof->totalDays - 3) : prof->totalDays;
}

// Candling programini hesapla
static void buildCandlingSchedule(const AnimalProfile* prof, CandlingSchedule& out) {
    if (!prof) return;

    out.speciesName = prof->name;
    out.totalDays   = prof->totalDays;
    out.lockdownDay = getLockdownDay(prof);
    out.checkCount  = 0;

    // %25, %50, %75 + lockdown
    const float percentages[] = { 0.25f, 0.50f, 0.75f };
    const char* pctLabels[]   = { "1.Kontrol (%25)", "2.Kontrol (%50)", "3.Kontrol (%75)" };

    for (int i = 0; i < 3; i++) {
        uint8_t day = max(1, (int)(prof->totalDays * percentages[i]));
        // Ayni gun tekrar etmesin (orn: 18 gunluk turda %25=4, %50=9, %75=14)
        bool duplicate = false;
        for (int j = 0; j < out.checkCount; j++) {
            if (out.checkDays[j] == day) { duplicate = true; break; }
        }
        if (!duplicate) {
            out.checkDays[out.checkCount]   = day;
            out.labels[out.checkCount]      = pctLabels[i];
            out.triggered[out.checkCount]   = false;
            out.checkCount++;
        }
    }

    // Son kontrol: lockdown gunu (onceki gunlerle cakismasin)
    bool ldDup = false;
    for (int j = 0; j < out.checkCount; j++) {
        if (out.checkDays[j] == out.lockdownDay) { ldDup = true; break; }
    }
    if (!ldDup && out.lockdownDay > 0) {
        out.checkDays[out.checkCount]   = out.lockdownDay;
        out.labels[out.checkCount]      = "Son Kontrol (Lockdown!)";
        out.triggered[out.checkCount]   = false;
        out.checkCount++;
    }
}

// Bugun kontrol gunu mu?
// Return: index (0..checkCount-1) veya -1
static int getCandlingCheckIndex(const CandlingSchedule& sched, int currentDay) {
    for (int i = 0; i < sched.checkCount; i++) {
        if (sched.checkDays[i] == currentDay) return i;
    }
    return -1;
}

// Bugun kontrol gunu mu? (basit bool)
static bool isCandlingDay(const CandlingSchedule& sched, int currentDay) {
    return getCandlingCheckIndex(sched, currentDay) >= 0;
}

// Kontrol etiketini al (orn: "1.Kontrol (%25)")
// currentDay=0 ise bos string doner
static const char* getCandlingLabel(const CandlingSchedule& sched, int currentDay) {
    int idx = getCandlingCheckIndex(sched, currentDay);
    if (idx >= 0 && idx < sched.checkCount) return sched.labels[idx];
    return "";
}

// Bir sonraki kontrol gununu dondur (currentDay'den sonraki en yakin)
// Yoksa 0 doner (tum kontroller gecti veya schedule bos)
static uint8_t nextCandlingDay(const CandlingSchedule& sched, int currentDay) {
    for (int i = 0; i < sched.checkCount; i++) {
        if (sched.checkDays[i] > currentDay) return sched.checkDays[i];
    }
    return 0;  // tum kontrol gunleri gecti
}

// Tum turler icin ornek programlari hesapla (debug/test icin)
// Serial'a yazar
static void printAllCandlingSchedules() {
    for (int p = 0; p < PROFILE_COUNT; p++) {
        CandlingSchedule sched;
        buildCandlingSchedule(ALL_PROFILES[p], sched);
        Serial.printf("[CANDLE] %s (%d gun, lockdown=%d): ",
                      sched.speciesName, sched.totalDays, sched.lockdownDay);
        for (int i = 0; i < sched.checkCount; i++) {
            Serial.printf("Gun%d=%s  ", sched.checkDays[i], sched.labels[i]);
        }
        Serial.println();
    }
}

#endif // CANDLING_SCHEDULER_H
