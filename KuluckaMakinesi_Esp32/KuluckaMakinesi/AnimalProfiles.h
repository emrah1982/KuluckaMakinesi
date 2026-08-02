#ifndef ANIMAL_PROFILES_H
#define ANIMAL_PROFILES_H

#include <Arduino.h>

// ============================================================
// HAYVAN KULUCKA PROFILLERI (Profesyonel v3 — 4 EVRE — 2026-05-07)
// Referans: Showa Furanki (JP), Xinzhou WQ-serisi (CN),
//           Nanchang Howard HHD (CN), Pas Reform, Brinsea Ova-Easy,
//           Cimuka HB100, Petersime, Aviagen Turkeys
//
// 2026-05-07 EVRE GENISLETMESI (v2 -> v3): Tum profiller 4 evreli yapildi.
// Eski sema (Gelisim + Cikim) yerine endustri standardi 4 evre kullaniliyor:
//
//   Evre 1 - On Isitma:      Yumurta makineye aliisma donemi (~1-3 gun)
//   Evre 2 - Erken Gelisim:  Organogenez (organ olusumu), cooling YOK
//   Evre 3 - Gec Gelisim:    Buyume, metabolic isi artisi (cooling/spray buyuk
//                             yumurtali profillerde — kaz/ordek/devekusu/kugu)
//   Evre 4 - Cikim/Lockdown: Cevirme durur, nem yukseltilir, sicaklik dusurulur
//
// Struct DEGISMEDI (geriye uyumluluk). Sadece evre sayisi 2-3'ten 4'e cikti.
// turning/cooling/spray/CO2 parametreleri korundu — fonksiyon kaybi YOK.
//
//
// 2026-04-25 revizyonlari (endustri kaynak hizalamasi):
//   - BILDIRCIN gelisim nemi:    %55-60 -> %50-55 (GQF)
//   - ORDEK cikim sicakligi:     37.0 -> 36.7 C  (Cherry Valley)
//   - HINDI cikim sicakligi:     37.0 -> 36.7 C  (Aviagen 36.1-36.7 ortasi)
//   - SULUN gelisim sicakligi:   37.8->37.5 -> 37.7->37.4 (MacFarlane)
//   - PAPAGAN gelisim nemi:      %50-60 -> %40-50 (Brinsea African Grey)
//   - KEKLIK cikim nemi:         %70-80 -> %55-65 (Meyer/Cackle, endustri)
//   - KAZ ileri faz cooling:     2x20dk -> 2x45dk (Brinsea 3sa orta yol)
//   - KUGU ileri faz cooling:    2x25dk -> 2x45dk (Kaz stratejisi)
//
// TODO (sonraki surum) — EGGSHELL TEMPERATURE DESTEGI:
//   Petersime/Aviagen profesyonel kuluckalarinda hava sicakligi yerine
//   "yumurta kabuk sicakligi" (eggshell temperature) hedef olarak alinir.
//   Hava sicakligi ile arasinda ~0.2-0.5 C fark vardir (embriyo metabolik
//   isi ureterek kabuk sicakligini hava sicakligindan 0.3-0.5 C yukari
//   cikarir gun 12+'da). Yapilacaklar:
//     1. IncubationPhase struct'a `float eggShellTempTarget` alani (opsiyonel)
//        veya `bool useEggShellMode` flag (mod secimi)
//     2. Ek sensor: NTC thermistor / IR pyrometer (yumurta uzerine yerlesir)
//     3. PIDController hedef olarak hava degil eggshell okur
//     4. Mevcut hava-sicakligi profilleri "fallback" olarak kalir
//     5. Calibration: hava-eggshell offset her hayvana gore farkli (kucuk
//        yumurta -> dusuk delta; buyuk yumurta -> yuksek delta)
//   Referans: Petersime "Eggshell temperature" makalesi.
//
// Struct degisikligi (v1 -> v2):
//   + turningIntervalMin   (kac dk'da bir cevirme)
//   + turningDurationSec   (her cevirme kac sn surer; 0 = aci ile hesapla)
//   + turningAngleDeg      (cevirme acisi, derece)
//   + coolingEnabled       (faz icinde gunluk sogutma var mi)
//   + coolingDurationMin   (her sogutma kac dk surer)
//   + coolingPerDay        (gunde kac kere sogutma)
//   + sprayingEnabled      (sogutma sonrasi su puskurtme)
//   + sprayingDurationSec  (puskurtme kac sn)
//
// Donanim notu: Cooling ve spraying mevcut 4'lu role ile
// simule edilir — CoolingSprayDriver'a bakiniz (Secenek B mimari).
// Aci-suresi iliskisi: TurnerDriver durationSec=0 verilirse
// aci/TURNER_DEG_PER_SEC (Config.h) ile hesaplar (yazilim kalibrasyonu).
// Stepper modunda (TURNER_TYPE=1) suresi atlanir, dogrudan aci kullanilir.
// ============================================================

struct IncubationPhase {
    uint8_t  startDay;             // Evrenin baslangic gunu
    uint8_t  endDay;               // Evrenin bitis gunu
    float    temperature;          // Baslangic sicakligi (C)
    float    tempEnd;              // Bitis sicakligi (C), 0=gradyan yok
    float    humidityLow;          // Nem alt esik (%)
    float    humidityHigh;         // Nem ust esik (%)

    bool     turningEnabled;       // Yumurta cevirme aktif mi
    uint16_t turningIntervalMin;   // Cevirme araligi (dk) — 0=turning kapali
    uint8_t  turningDurationSec;   // Cevirme suresi (sn) — 0=aciden hesapla
    uint8_t  turningAngleDeg;      // Cevirme acisi (derece) — tipik 45..90

    bool     coolingEnabled;       // Gunluk sogutma periyodu var mi
    uint8_t  coolingDurationMin;   // Her sogutma seansi kac dk
    uint8_t  coolingPerDay;        // Gunde kac kere sogutma

    bool     sprayingEnabled;      // Sogutma sonrasi su puskurtme
    uint8_t  sprayingDurationSec;  // Puskurtme suresi (sn)

    const char* phaseName;         // Evre adi
};

struct AnimalProfile {
    const char*      name;
    const char*      nameEN;
    uint8_t          totalDays;
    uint8_t          phaseCount;
    uint16_t         defaultEggCount;
    uint16_t         co2Low;          // CO2 alt limit (ppm) - normal
    uint16_t         co2High;         // CO2 ust limit (ppm) - alarm
    uint16_t         co2Critical;     // CO2 kritik limit (ppm) - acil
    IncubationPhase  phases[4];
};

// Bos faz — struct sonundaki kullanilmayan slotlar icin
#define EMPTY_PHASE \
    { 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, false, 0, 0, 0, false, 0, 0, false, 0, "" }

// -------------------- PROFIL TANIMLARI --------------------

// 0 - TAVUK (Chicken) — 21 gun, 4 evre
// Referans: Pas Reform SmartPro / Xinzhou WQ-48 / Cimuka HB100
// Standart: 60 dk / 15 sn turning, gunde 24 cevirme. Lockdown gun 18.
const AnimalProfile PROFILE_TAVUK = {
    "Tavuk", "Chicken", 21, 4, 50, 3000, 5000, 7000,
    {
        // Evre 1 - On Isitma (gun 1-2): yumurta makineye alisir, nem hafif yuksek
        { 1, 2, 37.8f, 0.0f, 58.0f, 62.0f,
          true,  60, 15, 90,
          false,  0, 0,
          false,  0,
          "On Isitma" },
        // Evre 2 - Erken Gelisim (gun 3-10): organogenez, sabit sicaklik
        { 3, 10, 37.8f, 0.0f, 55.0f, 60.0f,
          true,  60, 15, 90,
          false,  0, 0,
          false,  0,
          "Erken Gelisim" },
        // Evre 3 - Gec Gelisim (gun 11-18): kademeli sicaklik dususu
        { 11, 18, 37.8f, 37.5f, 55.0f, 60.0f,
          true,  60, 15, 90,
          false,  0, 0,
          false,  0,
          "Gec Gelisim" },
        // Evre 4 - Cikim/Lockdown (gun 19-21): cevirme KAPALI, nem yuksek
        { 19, 21, 37.2f, 0.0f, 65.0f, 75.0f,
          false,  0, 0, 0,
          false,  0, 0,
          false,  0,
          "Cikim" }
    }
};

// 1 - BILDIRCIN (Quail) — 18 gun, 4 evre
// Referans: GQF Sportsman / Nanchang HHD
// Kucuk yumurta, hizli gelisim. 45 dk / 8 sn turning. Lockdown gun 14.
const AnimalProfile PROFILE_BILDIRCIN = {
    "Bildircin", "Quail", 18, 4, 100, 3000, 5000, 7000,
    {
        // Evre 1 - On Isitma (gun 1): kisa, kucuk yumurta hizli isinir
        { 1, 1, 37.8f, 0.0f, 55.0f, 60.0f,
          true,  45, 8, 90,
          false,  0, 0,
          false,  0,
          "On Isitma" },
        // Evre 2 - Erken Gelisim (gun 2-8)
        { 2, 8, 37.8f, 0.0f, 50.0f, 55.0f,
          true,  45, 8, 90,
          false,  0, 0,
          false,  0,
          "Erken Gelisim" },
        // Evre 3 - Gec Gelisim (gun 9-14): kademeli dusus
        { 9, 14, 37.8f, 37.5f, 50.0f, 55.0f,
          true,  45, 8, 90,
          false,  0, 0,
          false,  0,
          "Gec Gelisim" },
        // Evre 4 - Cikim (gun 15-18)
        { 15, 18, 37.2f, 0.0f, 65.0f, 75.0f,
          false,  0, 0, 0,
          false,  0, 0,
          false,  0,
          "Cikim" }
    }
};

// 2 - KAZ (Goose) — 30 gun, 4 evre
// Referans: Brinsea Ova-Easy 380 / Masalles MP-100 / Showa SF
// BUYUK yumurta, yavas cevirme: 90 dk / 45 sn.
// Gun 10+ itibaren GUNDE 2 KERE 45 dk cooling + 8 sn sprey (kritik!).
const AnimalProfile PROFILE_KAZ = {
    "Kaz", "Goose", 30, 4, 15, 4000, 6000, 8000,
    {
        // Evre 1 - On Isitma (gun 1-3): buyuk yumurta yavas isinir, 3 gun
        { 1, 3, 37.8f, 0.0f, 60.0f, 65.0f,
          true,  90, 45, 45,
          false,  0, 0,
          false,  0,
          "On Isitma" },
        // Evre 2 - Erken Gelisim (gun 4-9): cooling YOK, organogenez
        { 4, 9, 37.8f, 37.6f, 58.0f, 62.0f,
          true,  90, 45, 45,
          false,  0, 0,
          false,  0,
          "Erken Gelisim" },
        // Evre 3 - Gec Gelisim (gun 10-25): cooling + sprey AKTIF (kritik!)
        { 10, 25, 37.6f, 37.2f, 60.0f, 65.0f,
          true,  90, 45, 45,
          true,  45, 2,    // gunde 2 kere 45 dk (Brinsea 3sa orta yol)
          true,  8,
          "Gec Gelisim" },
        // Evre 4 - Cikim/Lockdown (gun 26-30): 37.0, %78-85
        { 26, 30, 37.0f, 0.0f, 78.0f, 85.0f,
          false,  0, 0, 0,
          false,  0, 0,
          false,  0,
          "Cikim" }
    }
};

// 3 - ORDEK (Duck) — 28 gun, 4 evre (pekin; musk ordek icin 35 gun ayri profil)
// Referans: Cherry Valley / Grimaud Freres / Xinzhou WQ
// 60 dk / 25 sn turning. Gun 14+ gunluk 15 dk cooling + 5 sn sprey.
const AnimalProfile PROFILE_ORDEK = {
    "Ordek", "Duck", 28, 4, 30, 4000, 6000, 8000,
    {
        // Evre 1 - On Isitma (gun 1-2)
        { 1, 2, 37.8f, 0.0f, 60.0f, 65.0f,
          true,  60, 25, 90,
          false,  0, 0,
          false,  0,
          "On Isitma" },
        // Evre 2 - Erken Gelisim (gun 3-13): cooling YOK
        { 3, 13, 37.8f, 37.6f, 58.0f, 62.0f,
          true,  60, 25, 90,
          false,  0, 0,
          false,  0,
          "Erken Gelisim" },
        // Evre 3 - Gec Gelisim (gun 14-25): cooling + sprey AKTIF
        { 14, 25, 37.6f, 37.2f, 60.0f, 65.0f,
          true,  60, 25, 90,
          true,  15, 1,    // gunde 1 kere 15 dk
          true,  5,
          "Gec Gelisim" },
        // Evre 4 - Cikim/Lockdown (gun 26-28)
        { 26, 28, 36.7f, 0.0f, 75.0f, 85.0f,
          false,  0, 0, 0,
          false,  0, 0,
          false,  0,
          "Cikim" }
    }
};

// 4 - HINDI (Turkey) — 28 gun, 4 evre
// Referans: Aviagen Turkeys / Petersime BioStreamer
// 60 dk / 20 sn turning. (Hindi sicakliga hassas; sprey kullanilmaz)
const AnimalProfile PROFILE_HINDI = {
    "Hindi", "Turkey", 28, 4, 20, 3500, 5500, 7500,
    {
        // Evre 1 - On Isitma (gun 1-2)
        { 1, 2, 37.5f, 0.0f, 58.0f, 65.0f,
          true,  60, 20, 90,
          false,  0, 0,
          false,  0,
          "On Isitma" },
        // Evre 2 - Erken Gelisim (gun 3-12): organogenez
        { 3, 12, 37.5f, 0.0f, 52.0f, 58.0f,
          true,  60, 20, 90,
          false,  0, 0,
          false,  0,
          "Erken Gelisim" },
        // Evre 3 - Gec Gelisim (gun 13-25): kademeli dusus
        { 13, 25, 37.5f, 37.2f, 55.0f, 62.0f,
          true,  60, 20, 90,
          false,  0, 0,
          false,  0,
          "Gec Gelisim" },
        // Evre 4 - Cikim (gun 26-28)
        { 26, 28, 36.7f, 0.0f, 70.0f, 80.0f,
          false,  0, 0, 0,
          false,  0, 0,
          false,  0,
          "Cikim" }
    }
};

// 5 - SULUN (Pheasant) — 25 gun, 4 evre
// Referans: MacFarlane Pheasants / GQF
// 60 dk / 10 sn turning (kucuk yumurta).
const AnimalProfile PROFILE_SULUN = {
    "Sulun", "Pheasant", 25, 4, 30, 3000, 5000, 7000,
    {
        // Evre 1 - On Isitma (gun 1-2)
        { 1, 2, 37.7f, 0.0f, 58.0f, 62.0f,
          true,  60, 10, 90,
          false,  0, 0,
          false,  0,
          "On Isitma" },
        // Evre 2 - Erken Gelisim (gun 3-10)
        { 3, 10, 37.7f, 0.0f, 52.0f, 58.0f,
          true,  60, 10, 90,
          false,  0, 0,
          false,  0,
          "Erken Gelisim" },
        // Evre 3 - Gec Gelisim (gun 11-20): kademeli dusus
        { 11, 20, 37.7f, 37.4f, 55.0f, 60.0f,
          true,  60, 10, 90,
          false,  0, 0,
          false,  0,
          "Gec Gelisim" },
        // Evre 4 - Cikim (gun 21-25)
        { 21, 25, 37.2f, 0.0f, 65.0f, 75.0f,
          false,  0, 0, 0,
          false,  0, 0,
          false,  0,
          "Cikim" }
    }
};

// 6 - GUVERCIN (Pigeon) — 19 gun, 4 evre
// Referans: Brinsea Mini II
// Kucuk yumurta, hassas. 90 dk / 8 sn turning.
const AnimalProfile PROFILE_GUVERCIN = {
    "Guvercin", "Pigeon", 19, 4, 10, 3000, 5000, 7000,
    {
        // Evre 1 - On Isitma (gun 1)
        { 1, 1, 37.5f, 0.0f, 58.0f, 62.0f,
          true,  90, 8, 90,
          false,  0, 0,
          false,  0,
          "On Isitma" },
        // Evre 2 - Erken Gelisim (gun 2-8)
        { 2, 8, 37.5f, 0.0f, 50.0f, 55.0f,
          true,  90, 8, 90,
          false,  0, 0,
          false,  0,
          "Erken Gelisim" },
        // Evre 3 - Gec Gelisim (gun 9-15)
        { 9, 15, 37.5f, 0.0f, 55.0f, 60.0f,
          true,  90, 8, 90,
          false,  0, 0,
          false,  0,
          "Gec Gelisim" },
        // Evre 4 - Cikim (gun 16-19)
        { 16, 19, 37.2f, 0.0f, 65.0f, 75.0f,
          false,  0, 0, 0,
          false,  0, 0,
          false,  0,
          "Cikim" }
    }
};

// 7 - PAPAGAN (Parrot) — 26 gun, 4 evre (turlere gore 18-30 arasi degisir, ortalama)
// Referans: Avian Biotech / Brinsea / Voren's Aviaries
// Cok hassas, yavas cevirme: 120 dk / 5 sn. Hassas yumurta -> 45 deg
const AnimalProfile PROFILE_PAPAGAN = {
    "Papagan", "Parrot", 26, 4, 6, 2500, 4000, 6000,
    {
        // Evre 1 - On Isitma (gun 1-2)
        { 1, 2, 37.0f, 0.0f, 50.0f, 60.0f,
          true,  120, 5, 45,
          false,  0, 0,
          false,  0,
          "On Isitma" },
        // Evre 2 - Erken Gelisim (gun 3-12)
        { 3, 12, 37.0f, 0.0f, 40.0f, 48.0f,
          true,  120, 5, 45,
          false,  0, 0,
          false,  0,
          "Erken Gelisim" },
        // Evre 3 - Gec Gelisim (gun 13-23)
        { 13, 23, 37.0f, 0.0f, 45.0f, 55.0f,
          true,  120, 5, 45,
          false,  0, 0,
          false,  0,
          "Gec Gelisim" },
        // Evre 4 - Cikim (gun 24-26)
        { 24, 26, 36.7f, 0.0f, 65.0f, 75.0f,
          false,  0, 0, 0,
          false,  0, 0,
          false,  0,
          "Cikim" }
    }
};

// 8 - DEVEKUSU (Ostrich) — 42 gun, 4 evre
// Referans: Klein Karoo / SA Ostrich Business Chamber
// ANORMAL BUYUK yumurta (1.5 kg). 180 dk (3 saat) / 60 sn turning.
// Gun 5+ GUNDE 2 KERE 30 dk cooling zorunlu. Sprey YOK (kuru ortam).
const AnimalProfile PROFILE_DEVEKUSU = {
    "Devekusu", "Ostrich", 42, 4, 10, 4000, 6000, 8000,
    {
        // Evre 1 - On Isitma (gun 1-4): dev yumurta, uzun isinma suresi
        { 1, 4, 36.0f, 0.0f, 28.0f, 35.0f,
          true,  180, 60, 45,
          false,  0, 0,
          false,  0,
          "On Isitma" },
        // Evre 2 - Erken Gelisim (gun 5-21): cooling AKTIF (5. gunden itibaren)
        { 5, 21, 36.0f, 0.0f, 25.0f, 35.0f,
          true,  180, 60, 45,
          true,  30, 2,
          false,  0,
          "Erken Gelisim" },
        // Evre 3 - Gec Gelisim (gun 22-38): cooling devam
        { 22, 38, 36.0f, 0.0f, 25.0f, 35.0f,
          true,  180, 60, 45,
          true,  30, 2,
          false,  0,
          "Gec Gelisim" },
        // Evre 4 - Cikim (gun 39-42): %40-50
        { 39, 42, 36.0f, 0.0f, 40.0f, 50.0f,
          false,  0, 0, 0,
          false,  0, 0,
          false,  0,
          "Cikim" }
    }
};

// 9 - IPEK BOCEGI (Silkworm - Bombyx mori)
// Referans: Profesyonel serikultur (Japon sericulture institute referansi)
// Cevirme YOK. Hava akisi kritik (4-5. donem).
const AnimalProfile PROFILE_IPEKBOCEGI = {
    "Ipek Bocegi", "Silkworm", 42, 4, 500, 2000, 3500, 5000,
    {
        { 1, 10, 25.0f, 0.0f, 75.0f, 80.0f,
          false,  0, 0, 0,
          false,  0, 0,
          false,  0,
          "Yumurta" },
        { 11, 22, 27.0f, 0.0f, 80.0f, 90.0f,
          false,  0, 0, 0,
          false,  0, 0,
          false,  0,
          "Genc Larva" },
        { 23, 35, 24.0f, 0.0f, 65.0f, 75.0f,
          false,  0, 0, 0,
          false,  0, 0,
          false,  0,
          "Olgun Larva" },
        { 36, 42, 24.5f, 0.0f, 55.0f, 65.0f,
          false,  0, 0, 0,
          false,  0, 0,
          false,  0,
          "Koza Orme" }
    }
};

// 10 - ARI KOVANI (Bee Hive - Apis mellifera) — 21 gun, 4 evre
// Referans: Apiculture — brood nest 34-35 C, nem %50-60. Cevirme YOK.
const AnimalProfile PROFILE_ARI = {
    "Ari Kovani", "Bee Hive", 21, 4, 1, 5000, 8000, 10000,
    {
        // Evre 1 - On Isitma / Yumurta (gun 1-3)
        { 1, 3, 34.2f, 0.0f, 50.0f, 60.0f,
          false,  0, 0, 0,
          false,  0, 0,
          false,  0,
          "On Isitma" },
        // Evre 2 - Erken Larva (gun 4-8)
        { 4, 8, 34.5f, 0.0f, 55.0f, 65.0f,
          false,  0, 0, 0,
          false,  0, 0,
          false,  0,
          "Erken Larva" },
        // Evre 3 - Gec Larva (gun 9-17)
        { 9, 17, 35.0f, 0.0f, 55.0f, 65.0f,
          false,  0, 0, 0,
          false,  0, 0,
          false,  0,
          "Gec Larva" },
        // Evre 4 - Pupa (gun 18-21)
        { 18, 21, 34.8f, 0.0f, 50.0f, 60.0f,
          false,  0, 0, 0,
          false,  0, 0,
          false,  0,
          "Pupa" }
    }
};

// 11 - KEKLIK (Partridge / Chukar) — 24 gun, 4 evre
// Referans: MacFarlane Pheasants / Meyer Hatchery / Cackle / GQF
// Kucuk-orta yumurta (~18-22 g): 60 dk / 12 sn turning, 90 deg.
const AnimalProfile PROFILE_KEKLIK = {
    "Keklik", "Partridge", 24, 4, 40, 3000, 5000, 7000,
    {
        // Evre 1 - On Isitma (gun 1-2)
        { 1, 2, 37.8f, 0.0f, 58.0f, 65.0f,
          true,  60, 12, 90,
          false,  0, 0,
          false,  0,
          "On Isitma" },
        // Evre 2 - Erken Gelisim (gun 3-10)
        { 3, 10, 37.8f, 0.0f, 50.0f, 55.0f,
          true,  60, 12, 90,
          false,  0, 0,
          false,  0,
          "Erken Gelisim" },
        // Evre 3 - Gec Gelisim (gun 11-22): kademeli dusus
        { 11, 22, 37.8f, 37.5f, 50.0f, 60.0f,
          true,  60, 12, 90,
          false,  0, 0,
          false,  0,
          "Gec Gelisim" },
        // Evre 4 - Cikim (gun 23-24)
        { 23, 24, 36.8f, 0.0f, 55.0f, 65.0f,
          false,  0, 0, 0,
          false,  0, 0,
          false,  0,
          "Cikim" }
    }
};

// 12 - TAVUS KUSU (Peacock / Peafowl) — 28 gun, 4 evre
// Referans: UFAW peafowl guide / Brinsea Ova-Easy / Hindi profili analojisi
// Orta-buyuk yumurta (~90-110 g): 60 dk / 20 sn turning, 90 deg.
const AnimalProfile PROFILE_TAVUSKUSU = {
    "Tavus Kusu", "Peafowl", 28, 4, 12, 3500, 5500, 7500,
    {
        // Evre 1 - On Isitma (gun 1-3)
        { 1, 3, 37.8f, 0.0f, 58.0f, 65.0f,
          true,  60, 20, 90,
          false,  0, 0,
          false,  0,
          "On Isitma" },
        // Evre 2 - Erken Gelisim (gun 4-12)
        { 4, 12, 37.8f, 0.0f, 52.0f, 58.0f,
          true,  60, 20, 90,
          false,  0, 0,
          false,  0,
          "Erken Gelisim" },
        // Evre 3 - Gec Gelisim (gun 13-25): kademeli dusus
        { 13, 25, 37.8f, 37.5f, 55.0f, 62.0f,
          true,  60, 20, 90,
          false,  0, 0,
          false,  0,
          "Gec Gelisim" },
        // Evre 4 - Cikim (gun 26-28)
        { 26, 28, 36.5f, 0.0f, 65.0f, 75.0f,
          false,  0, 0, 0,
          false,  0, 0,
          false,  0,
          "Cikim" }
    }
};

// 13 - KUGU (Swan) — 37 gun, 4 evre (resim: 36-38; ortalama 37)
// Referans: WWT swan rearing / Brinsea Ova-Easy 380 / Kaz profili analojisi
// COK BUYUK yumurta (~340 g, kazdan da iri): 90 dk / 50 sn turning, 45 deg.
// Gun 12+ itibaren GUNDE 2 KERE 45 dk cooling + 8 sn sprey (kaz orta yol).
const AnimalProfile PROFILE_KUGU = {
    "Kugu", "Swan", 37, 4, 6, 4000, 6000, 8000,
    {
        // Evre 1 - On Isitma (gun 1-4): dev yumurta uzun isinma
        { 1, 4, 37.8f, 0.0f, 58.0f, 65.0f,
          true,  90, 50, 45,
          false,  0, 0,
          false,  0,
          "On Isitma" },
        // Evre 2 - Erken Gelisim (gun 5-11): cooling YOK, organogenez
        { 5, 11, 37.8f, 37.6f, 55.0f, 62.0f,
          true,  90, 50, 45,
          false,  0, 0,
          false,  0,
          "Erken Gelisim" },
        // Evre 3 - Gec Gelisim (gun 12-35): cooling + sprey AKTIF
        { 12, 35, 37.6f, 37.2f, 58.0f, 65.0f,
          true,  90, 50, 45,
          true,  45, 2,    // gunde 2 kere 45 dk (Kaz orta yol stratejisi)
          true,  8,
          "Gec Gelisim" },
        // Evre 4 - Cikim (gun 36-37): %78-90
        { 36, 37, 36.5f, 0.0f, 78.0f, 90.0f,
          false,  0, 0, 0,
          false,  0, 0,
          false,  0,
          "Cikim" }
    }
};

// -------------------- PROFIL DIZISI --------------------
#define PROFILE_COUNT 14

const AnimalProfile* const ALL_PROFILES[PROFILE_COUNT] = {
    &PROFILE_TAVUK,
    &PROFILE_BILDIRCIN,
    &PROFILE_KAZ,
    &PROFILE_ORDEK,
    &PROFILE_HINDI,
    &PROFILE_SULUN,
    &PROFILE_GUVERCIN,
    &PROFILE_PAPAGAN,
    &PROFILE_DEVEKUSU,
    &PROFILE_IPEKBOCEGI,
    &PROFILE_ARI,
    &PROFILE_KEKLIK,
    &PROFILE_TAVUSKUSU,
    &PROFILE_KUGU
};

#endif // ANIMAL_PROFILES_H
