#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "../config/Config.h"
#include "../config/AnimalProfiles.h"

// ==================== SEKME ENUM ====================
enum DisplayTab {
    TAB_DASH = 0,  // Durum
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
    TOUCH_SET_START_DATE
};

// ==================== GOSTERGE VERISI ====================
struct DisplayData {
    // Sensor
    float temperature;
    float humidity;
    float targetTemp;
    float targetHumLow;
    float targetHumHigh;
    // Cikislar
    uint8_t heaterPWM;
    uint8_t fanPWM;
    bool humidifierOn;
    // Gun / evre
    int currentDay;
    int totalDays;
    int remainingDays;
    int phaseRemainingDays;
    String startDate;
    String hatchDate;
    const char* phaseName;
    const char* profileName;
    int systemState;  // 0-5 (SYS_INITIALIZING..SYS_EMERGENCY)
    bool turningEnabled;
    // Sensor durumu
    bool sensor1OK;
    bool sensor2OK;
    unsigned long uptimeSec;
    // Alarm
    bool alarmActive;
    String alarmMsg;
    // PID
    double kp, ki, kd;
    // Profil detay (Profil sekmesi icin)
    const AnimalProfile* profile;   // Aktif profil (faz bilgileri)
    uint8_t currentPhaseIndex;      // Suanki evre indexi
    // Ag bilgisi (Ayar sekmesi icin)
    bool apActive;
    bool staConnected;
    String apIP;
    String staIP;
    uint8_t apClients;
    uint32_t freeHeap;
};

// ==================== DISPLAY MANAGER ====================
class DisplayManager {
public:
    DisplayManager();

    void begin();
    TouchAction update(const DisplayData &data);
    uint8_t getSelectedProfileIdx() const { return _pendingProfileIdx; }
    void getEditedStartDate(uint16_t &year, uint8_t &month, uint8_t &day) const;

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

    uint16_t _editYear;
    uint8_t _editMonth;
    uint8_t _editDay;
    bool _editDateInit;

    // Dokunma
    TouchAction handleTouch(const DisplayData &data);
    bool getTouchXY(uint16_t &x, uint16_t &y);

    // Ana cizim
    void draw(const DisplayData &data);
    void drawTabBar();

    // Sayfa cizimleri
    void drawDashboard(const DisplayData &data);
    void drawControl(const DisplayData &data);
    void drawProfile(const DisplayData &data);
    void drawProfileList();
    void drawSettings(const DisplayData &data);

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
    bool touchInRect(uint16_t tx, uint16_t ty,
                     int rx, int ry, int rw, int rh);
};

#endif // DISPLAY_MANAGER_H
