#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "Config.h"
#include "AnimalProfiles.h"
#include "RTCManager.h"
#include "AlarmService.h"

// Forward declaration — sadece pointer kullaniyoruz, dairesel include yok.
class StorageService;

// ==================== SEKME ENUM ====================
enum DisplayTab {
    TAB_DASH = 0,  // Durum
    TAB_GRAPH,     // Grafik
    TAB_CTRL,      // Kontrol
    TAB_PROF,      // Profil
    TAB_SET,       // Ayar
    TAB_COUNT
};

// ==================== DOKUNMA AKSIYONU ====================
enum TouchAction {
    TOUCH_NONE = 0,
    TOUCH_START,
    TOUCH_PAUSE,
    TOUCH_RESUME,
    TOUCH_STOP,
    TOUCH_SAFETY_RESET,
    TOUCH_PROFILE_SELECT,  // Profil secildi (_pendingProfileIdx)
    TOUCH_EGG_DEC,
    TOUCH_EGG_INC,
    TOUCH_EGG_SENSOR_TOGGLE,    // IR Yumurta sensoru aktif/pasif (Ayar tab)
    TOUCH_EGG_DISCOVER,         // mDNS ile yumurta.local kesfi baslat (Ayar tab)
    // Profil override editor (2026-04-25)
    TOUCH_EDIT_TEMP_DEC,        // Aktif faz sicakligi -0.1 C
    TOUCH_EDIT_TEMP_INC,        // Aktif faz sicakligi +0.1 C
    TOUCH_EDIT_HUMLOW_DEC,      // Aktif faz nem alt esigi -1%
    TOUCH_EDIT_HUMLOW_INC,      // Aktif faz nem alt esigi +1%
    TOUCH_EDIT_HUMHIGH_DEC,     // Aktif faz nem ust esigi -1%
    TOUCH_EDIT_HUMHIGH_INC,     // Aktif faz nem ust esigi +1%
    TOUCH_EDIT_TURNINT_DEC,     // Aktif faz cevirme araligi -5 dk
    TOUCH_EDIT_TURNINT_INC,     // Aktif faz cevirme araligi +5 dk
    TOUCH_EDIT_RESET_FACTORY,   // Fabrika ayarlarina don (override sil)
    TOUCH_ALARM_ACK,            // Aktif alarmi sustur (kullanici onaylama, 10 dk)
    TOUCH_ALARM_SNOOZE,         // Aktif alarmi ertele (candling: 1 saat)
    TOUCH_ALARM_DISMISS,        // Aktif alarmi kapat (bugun icinde tekrar gosterilmez)
    TOUCH_CLEANING_TOGGLE       // Temizlik modu baslat/durdur
};

// ==================== GOSTERGE VERISI ====================
struct DisplayData {
    // Sensor
    float temperature;
    float humidity;
    uint16_t co2;           // CO₂ değeri (ppm)
    float targetTemp;
    float targetHumLow;
    float targetHumHigh;
    uint16_t co2Low;        // CO₂ alt limit (ppm)
    uint16_t co2High;       // CO₂ üst limit (ppm)
    uint16_t co2Critical;   // CO₂ kritik limit (ppm)
    // Cikislar
    uint8_t heaterPWM;
    uint8_t fanPWM;
    bool humidifierOn;
    // Gun / evre
    int currentDay;
    int totalDays;
    int remainingDays;
    int phaseRemainingDays;
    String hatchDate;
    const char* phaseName;
    const char* profileName;
    int systemState;  // 0-5 (SYS_INITIALIZING..SYS_EMERGENCY)
    bool turningEnabled;
    uint16_t turningIntervalMin;   // Aktif faz cevirme araligi (dk; 0=kapali)
    // Sensor durumu
    bool sensor1OK;
    bool sensor2OK;
    unsigned long uptimeSec;
    uint16_t eggCount;
    // Alarm
    bool alarmActive;
    String alarmMsg;
    AlarmType alarmType;    // Alarm tipi (ALARM_CANDLING_DUE, ALARM_TEMP_HIGH, vb.)
    bool alarmMuted;        // Susturuldu mu (10 dk timer)
    bool alarmAutoShow;     // Tam ekran modal'i otomatik ac (alarm + mute degil)
    // PID
    double kp, ki, kd;
    // Profil detay (Profil sekmesi icin)
    const AnimalProfile* profile;   // Aktif profil (faz bilgileri)
    uint8_t currentPhaseIndex;      // Suanki evre indexi
    uint8_t profileIndex;           // Aktif profil indexi (ikon icin)
    // Ag bilgisi (Ayar sekmesi icin)
    bool apActive;
    bool staConnected;
    String apIP;
    String staIP;
    uint8_t apClients;
    uint32_t freeHeap;
    // Grafik verisi (son 60 okuma)
    float* tempHistory;      // Sicaklik gecmisi dizisi
    float* humHistory;       // Nem gecmisi dizisi
    uint16_t historyCount;   // Dizideki eleman sayisi
    uint16_t historyMax;     // Max eleman (60)
    // Yumurta IR sicaklik (EggTempService)
    float    eggTemp;          // Son okunan yumurta yuzey sicakligi (°C)
    bool     eggTempValid;     // true = gecerli veri var, false = baglanti yok
    bool     eggSensorEnabled; // Servis aktif/pasif (kullanici toggle)
    uint8_t  eggDiscoveryStatus; // 0=NONE, 1=RUNNING, 2=OK, 3=FAILED
    // Temizlik / bakim modu
    bool     cleaningActive;         // Temizlik modunda mi
    unsigned long cleaningRemainMs;  // Otomatik bitise kalan sure
    // Candling (dol kontrolu)
    bool     candlingToday;          // Bugun kontrol gunu mu
    const char* candlingLabel;       // "1.Kontrol (%25)" vb. (bos string = yok)
    uint8_t  candlingLockdownDay;    // Lockdown gunu
    String   startDate;              // Baslangi tarihi (DD/MM/YYYY)
    uint8_t  startDay;               // Baslangi gunu (1-31)
    uint8_t  startMonth;             // Baslangi ayi (1-12)
    uint16_t startYear;              // Baslangi yili
    uint8_t  todayDay;               // Bugunun tarihi (gunu: 1-31)
    uint8_t  todayMonth;             // Bugunun tarihi (ayi: 1-12)
    uint16_t todayYear;              // Bugunun tarihi (yili)
    uint8_t  todayHour;              // Saat (0-23)
    uint8_t  todayMinute;            // Dakika (0-59)
    uint8_t  todayDow;               // Haftanin gunu (0=Pazar..6=Cumartesi, RTClib ile uyumlu)
};

// ==================== DISPLAY MANAGER ====================
class DisplayManager {
public:
    DisplayManager();

    void begin();
    TouchAction update(const DisplayData &data);
    uint8_t getSelectedProfileIdx() const { return _pendingProfileIdx; }

    // Override sistemi (2026-04-25): kullanici hazir profili duzenlemis mi
    // gormek icin opsiyonel storage referansi. nullptr ise rozet cizilmez.
    void setCustomProfileStorage(StorageService* s) { _customProfileStorage = s; }

private:
    TFT_eSPI _tft;
    unsigned long _lastUpdate;
    unsigned long _lastTouch;
    DisplayTab _tab;
    bool _forceRedraw;
    bool _bgDraw;
    int8_t _scrollOffset;  // Scroll pozisyonu (sayfa bazli)
    bool _profileListOpen;  // Profil dropdown acik mi
    uint8_t _pendingProfileIdx; // Secilen profil indexi
    StorageService* _customProfileStorage = nullptr;  // Override rozet kontrolu
    bool _profileEditorOpen = false;  // Profil duzenleme tam-ekran modu
    bool _alarmModalOpen    = false;  // Alarm modal'i (tum tab'larin ustunde)
    bool _alarmModalDrawn   = false;  // Modal cizildi mi (cycle bazli yenileme)
    
    // Onceki degerler (flicker-free icin)
    float _prevTemp;
    float _prevHum;
    uint16_t _prevCO2;
    uint16_t _prevTempColor;
    uint16_t _prevHumColor;
    uint16_t _prevCO2Color;

    // Dokunma
    TouchAction handleTouch(const DisplayData &data);
    bool getTouchXY(uint16_t &x, uint16_t &y);

    // Ana cizim
    void draw(const DisplayData &data);
    void drawTabBar();

    // Sayfa cizimleri
    void drawDashboard(const DisplayData &data);
    void drawGraph(const DisplayData &data);
    void drawControl(const DisplayData &data);
    void drawProfile(const DisplayData &data);
    void drawProfileList();
    void drawProfileEditor(const DisplayData &data);
    void drawSettings(const DisplayData &data);
    void drawAlarmModal(const DisplayData &data);   // Tum tab'larin ustunde tam ekran alarm

    // Dashboard parcalari
    void drawHeader(const DisplayData &data);
    void drawGauges(const DisplayData &data);
    void drawInfoRow(const DisplayData &data);
    void drawOutputs(const DisplayData &data);
    void drawSensors(const DisplayData &data);
    void drawAlarm(const DisplayData &data);

    // Yardimci
    void drawBar(int x, int y, int w, int h, int val, int maxVal,
                 uint16_t fg, uint16_t bg);
    void drawButton(int x, int y, int w, int h,
                    const char* label, uint16_t bg, uint16_t fg);
    void drawGauge(int cx, int cy, int r, float value, float minVal,
                   float maxVal, float target, uint16_t color,
                   const char* label, const char* unit);
    bool touchInRect(uint16_t tx, uint16_t ty,
                     int rx, int ry, int rw, int rh);
    
    // Splash animasyonu
    void showSplashAnimation();

    // Temizlik modu uyari banti (tum sekmeler uzerinde, yanip soner)
    void drawCleaningBanner(const DisplayData &data);
    // Candling (dol kontrolu) bildiri banti
    void drawCandlingBanner(const DisplayData &data);

    // ---------- Loading animasyonu (splash sonrasi) ----------
    // Splash bitiminde "Yukleniyor..." noktalari donen kucuk bir
    // animasyon arka plan task'inda gosterir. Dashboard ilk kez
    // cizilmeden once otomatik durdurulur.
    void startLoadingAnimation();
    void stopLoadingAnimation();
    static void loadingTask(void* pvParameters);

    TaskHandle_t      _loadingTaskHandle;
    volatile bool     _loadingActive;
};

#endif // DISPLAY_MANAGER_H
