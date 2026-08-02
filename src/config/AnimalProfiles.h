#ifndef ANIMAL_PROFILES_H
#define ANIMAL_PROFILES_H

#include <Arduino.h>

// ============================================================
// HAYVAN KULUÇKA PROFİLLERİ (4 EVRELİ PROFESYONEL)
// Kaynak: Hayvan.pdf referans dokümanı + Pas Reform / Petersime / Brinsea
// ============================================================
// Evre sistemi:
//   Evre 1 - Ön Isıtma:     Yumurtalar makineye alışır, stabilizasyon
//   Evre 2 - Erken Gelişim: Organ gelişimi, yüksek sıcaklık
//   Evre 3 - Geç Gelişim:   Büyüme, hava kesesi oluşumu
//   Evre 4 - Çıkım/ Lockdown: Çevirme durur, nem yükselir, sıcaklık düşer
// ============================================================

struct IncubationPhase {
    uint8_t  startDay;      // Evrenin başlangıç günü
    uint8_t  endDay;        // Evrenin bitiş günü
    float    temperature;   // Hedef sıcaklık (°C)
    float    humidityLow;   // Nem alt eşik (%)
    float    humidityHigh;  // Nem üst eşik (%)
    bool     turningEnabled;// Yumurta çevirme aktif mi
    const char* phaseName;  // Evre adı
};

struct AnimalProfile {
    const char*      name;          // Hayvan adı
    const char*      nameEN;        // İngilizce adı
    uint8_t          totalDays;     // Toplam kuluçka süresi
    uint8_t          phaseCount;    // Evre sayısı
    IncubationPhase  phases[4];     // Maksimum 4 evre
};

// -------------------- PROFİL TANIMLARI --------------------

// 0 - TAVUK (Chicken) - 21 gün, 4 evre
// Kaynak: Pas Reform / Jamesway / Petersime profesyonel protokolleri
const AnimalProfile PROFILE_TAVUK = {
    "Tavuk", "Chicken", 21, 4,
    {
        { 1,   3, 37.8f, 60.0f, 65.0f, true,  "On Isitma"     },
        { 4,  10, 37.6f, 50.0f, 55.0f, true,  "Erken Gelisim" },
        { 11, 18, 37.5f, 50.0f, 55.0f, true,  "Gec Gelisim"   },
        { 19, 21, 37.2f, 65.0f, 75.0f, false, "Cikim"         }
    }
};

// 1 - BILDIRCIN (Quail) - 18 gün, 4 evre
// Kaynak: GQF / Brinsea tavsiyeleri
const AnimalProfile PROFILE_BILDIRCIN = {
    "Bildircin", "Quail", 18, 4,
    {
        { 1,   2, 37.8f, 60.0f, 65.0f, true,  "On Isitma"     },
        { 3,   8, 37.6f, 50.0f, 55.0f, true,  "Erken Gelisim" },
        { 9,  14, 37.5f, 50.0f, 55.0f, true,  "Gec Gelisim"   },
        { 15, 18, 37.2f, 65.0f, 75.0f, false, "Cikim"         }
    }
};

// 2 - KAZ (Goose) - 30 gün, 4 evre
// Kaynak: Brinsea / Masalles tavsiyeleri
// Not: 15. gunden itibaren gunluk 15dk sogutma + su puskurtme onerilir
const AnimalProfile PROFILE_KAZ = {
    "Kaz", "Goose", 30, 4,
    {
        { 1,   4, 37.8f, 62.0f, 68.0f, true,  "On Isitma"     },
        { 5,  12, 37.6f, 55.0f, 60.0f, true,  "Erken Gelisim" },
        { 13, 25, 37.5f, 58.0f, 65.0f, true,  "Gec Gelisim"   },
        { 26, 30, 37.2f, 78.0f, 85.0f, false, "Cikim"         }
    }
};

// 3 - ÖRDEK (Duck) - 28 gün, 4 evre
// Kaynak: Cherry Valley / Brinsea tavsiyeleri
// Not: 14. gunden itibaren gunluk sogutma + su puskurtme onerilir
const AnimalProfile PROFILE_ORDEK = {
    "Ordek", "Duck", 28, 4,
    {
        { 1,   3, 37.8f, 62.0f, 68.0f, true,  "On Isitma"     },
        { 4,  10, 37.6f, 55.0f, 60.0f, true,  "Erken Gelisim" },
        { 11, 25, 37.5f, 58.0f, 65.0f, true,  "Gec Gelisim"   },
        { 26, 28, 37.2f, 75.0f, 85.0f, false, "Cikim"         }
    }
};

// 4 - HİNDİ (Turkey) - 28 gün, 4 evre
// Kaynak: Aviagen Turkeys / Petersime tavsiyeleri
const AnimalProfile PROFILE_HINDI = {
    "Hindi", "Turkey", 28, 4,
    {
        { 1,   3, 37.8f, 58.0f, 65.0f, true,  "On Isitma"     },
        { 4,  12, 37.6f, 52.0f, 58.0f, true,  "Erken Gelisim" },
        { 13, 25, 37.5f, 55.0f, 62.0f, true,  "Gec Gelisim"   },
        { 26, 28, 37.2f, 70.0f, 80.0f, false, "Cikim"         }
    }
};

// 5 - SÜLÜN (Pheasant) - 25 gün, 4 evre
// Kaynak: MacFarlane Pheasants / GQF tavsiyeleri
const AnimalProfile PROFILE_SULUN = {
    "Sulun", "Pheasant", 25, 4,
    {
        { 1,   3, 37.8f, 60.0f, 65.0f, true,  "On Isitma"     },
        { 4,  10, 37.6f, 52.0f, 58.0f, true,  "Erken Gelisim" },
        { 11, 20, 37.5f, 55.0f, 60.0f, true,  "Gec Gelisim"   },
        { 21, 25, 37.2f, 65.0f, 75.0f, false, "Cikim"         }
    }
};

// 6 - GÜVERCİN (Pigeon) - 19 gün, 4 evre
// Kaynak: Brinsea tavsiyeleri
const AnimalProfile PROFILE_GUVERCIN = {
    "Guvercin", "Pigeon", 19, 4,
    {
        { 1,   2, 37.8f, 60.0f, 65.0f, true,  "On Isitma"     },
        { 3,   8, 37.6f, 50.0f, 55.0f, true,  "Erken Gelisim" },
        { 9,  15, 37.5f, 55.0f, 60.0f, true,  "Gec Gelisim"   },
        { 16, 19, 37.2f, 65.0f, 75.0f, false, "Cikim"         }
    }
};

// 7 - PAPAĞAN (Parrot) - 26 gün, 4 evre
// Kaynak: Avian Biotech / Brinsea tavsiyeleri
// Not: Tur bazli degisir, ortalama degerler
const AnimalProfile PROFILE_PAPAGAN = {
    "Papagan", "Parrot", 26, 4,
    {
        { 1,   3, 37.3f, 55.0f, 62.0f, true,  "On Isitma"     },
        { 4,  12, 37.1f, 48.0f, 55.0f, true,  "Erken Gelisim" },
        { 13, 23, 37.0f, 50.0f, 60.0f, true,  "Gec Gelisim"   },
        { 24, 26, 36.7f, 65.0f, 75.0f, false, "Cikim"         }
    }
};

// 8 - DEVEKUŞU (Ostrich) - 42 gün, 4 evre
// Kaynak: Klein Karoo / SA Ostrich tavsiyeleri
// Not: Cok dusuk nem, yavas cikim
const AnimalProfile PROFILE_DEVEKUSU = {
    "Devekusu", "Ostrich", 42, 4,
    {
        { 1,   7, 36.3f, 28.0f, 35.0f, true,  "On Isitma"     },
        { 8,  21, 36.1f, 22.0f, 30.0f, true,  "Erken Gelisim" },
        { 22, 38, 36.0f, 25.0f, 35.0f, true,  "Gec Gelisim"   },
        { 39, 42, 36.0f, 40.0f, 50.0f, false, "Cikim"         }
    }
};

// 9 - İPEK BÖCEĞİ / KOZA (Silkworm - Bombyx mori) - 42 gün, 4 evre
// Kaynak: Profesyonel serikultur verileri
// Not: Cevirme devre disi. Iyi havalandirma onemli (ozellikle 4-5. donem).
const AnimalProfile PROFILE_IPEKBOCEGI = {
    "Ipek Bocegi", "Silkworm", 42, 4,
    {
        {  1, 10, 25.0f, 75.0f, 80.0f, false, "Yumurta"    },
        { 11, 22, 27.0f, 80.0f, 90.0f, false, "Genc Larva" },
        { 23, 35, 24.0f, 65.0f, 75.0f, false, "Olgun Larva"},
        { 36, 42, 24.5f, 55.0f, 65.0f, false, "Koza Orme"  }
    }
};

// 10 - ARI KOVANI (Bee Hive - Apis mellifera) - 21 gün, 4 evre
// Kaynak: Apiculture — brood nest 34-35 C, nem %50-60. Cevirme YOK.
const AnimalProfile PROFILE_ARI = {
    "Ari Kovani", "Bee Hive", 21, 4,
    {
        { 1,   3, 34.2f, 50.0f, 60.0f, false, "On Isitma"   },
        { 4,   8, 34.5f, 55.0f, 65.0f, false, "Erken Larva" },
        { 9,  17, 35.0f, 55.0f, 65.0f, false, "Gec Larva"   },
        { 18, 21, 34.8f, 50.0f, 60.0f, false, "Pupa"        }
    }
};

// 11 - KEKLİK (Partridge / Chukar) - 24 gün, 4 evre
// Kaynak: MacFarlane Pheasants / Meyer Hatchery / GQF
const AnimalProfile PROFILE_KEKLIK = {
    "Keklik", "Partridge", 24, 4,
    {
        { 1,   2, 37.8f, 58.0f, 65.0f, true,  "On Isitma"     },
        { 3,  10, 37.7f, 50.0f, 55.0f, true,  "Erken Gelisim" },
        { 11, 22, 37.5f, 50.0f, 60.0f, true,  "Gec Gelisim"   },
        { 23, 24, 36.8f, 55.0f, 65.0f, false, "Cikim"         }
    }
};

// 12 - TAVUS KUŞU (Peacock / Peafowl) - 28 gün, 4 evre
// Kaynak: UFAW peafowl guide / Brinsea Ova-Easy
const AnimalProfile PROFILE_TAVUSKUSU = {
    "Tavus Kusu", "Peafowl", 28, 4,
    {
        { 1,   3, 37.8f, 60.0f, 65.0f, true,  "On Isitma"     },
        { 4,  10, 37.7f, 52.0f, 58.0f, true,  "Erken Gelisim" },
        { 11, 25, 37.5f, 55.0f, 62.0f, true,  "Gec Gelisim"   },
        { 26, 28, 36.5f, 65.0f, 75.0f, false, "Cikim"         }
    }
};

// 13 - KUĞU (Swan) - 37 gün, 4 evre
// Kaynak: WWT swan rearing / Brinsea Ova-Easy 380
// Not: COK BUYUK yumurta (~340 g). Gunluk sogutma + sprey onerilir.
const AnimalProfile PROFILE_KUGU = {
    "Kugu", "Swan", 37, 4,
    {
        { 1,   5, 37.8f, 58.0f, 65.0f, true,  "On Isitma"     },
        { 6,  14, 37.6f, 55.0f, 60.0f, true,  "Erken Gelisim" },
        { 15, 35, 37.4f, 58.0f, 65.0f, true,  "Gec Gelisim"   },
        { 36, 37, 36.5f, 78.0f, 90.0f, false, "Cikim"         }
    }
};

// -------------------- PROFİL DİZİSİ --------------------
#define PROFILE_COUNT 14

const AnimalProfile* const ALL_PROFILES[PROFILE_COUNT] = {
    &PROFILE_TAVUK,       // 0
    &PROFILE_BILDIRCIN,   // 1
    &PROFILE_KAZ,         // 2
    &PROFILE_ORDEK,       // 3
    &PROFILE_HINDI,       // 4
    &PROFILE_SULUN,       // 5
    &PROFILE_GUVERCIN,    // 6
    &PROFILE_PAPAGAN,     // 7
    &PROFILE_DEVEKUSU,    // 8
    &PROFILE_IPEKBOCEGI,  // 9
    &PROFILE_ARI,         // 10
    &PROFILE_KEKLIK,      // 11
    &PROFILE_TAVUSKUSU,   // 12
    &PROFILE_KUGU         // 13
};

#endif // ANIMAL_PROFILES_H
