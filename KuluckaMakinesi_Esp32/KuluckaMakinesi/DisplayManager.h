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
    TOUCH_CLEANING_TOGGLE,      // Temizlik modu baslat/durdur
    // Kontrol sekmesi manuel cikis kontrolu. Manuel mod (SYS_CLEANING)
    // aktif degilse once o moda gecilir: PID ile elle kontrolun ayni anda
    // isiticiye komut vermesi kabul edilemez.
    TOUCH_MANUAL_HEATER,        // Isitici ac/kapa (manuel)
    TOUCH_MANUAL_HUM,           // Nemlendirici ac/kapa (manuel)
    // ---- Role testi (GECICI - Config.h RELAY_TEST_ENABLED) ----
    TOUCH_RTEST_ENTER,          // Test moduna gir
    TOUCH_RTEST_EXIT,           // Test modundan cik
    TOUCH_RTEST_R0,             // P0 Isitici rolesi
    TOUCH_RTEST_R1,             // P1 Nemlendirici rolesi
    TOUCH_RTEST_R2,             // P2 Cevirme guc rolesi
    TOUCH_RTEST_R3,             // P3 Cevirme yon rolesi
    TOUCH_RTEST_FAN             // Fan PWM (role degil, IO18)
};

// ==================== GOSTERGE VERISI ====================
struct DisplayData {
    // Sensor
    float temperature;
    float humidity;
    uint16_t co2;           // CO₂ değeri (ppm)
    bool     co2Valid;      // Gercek sensorden mi geliyor.
                            // false iken 0 ppm'i YESIL "normal" olarak gostermek
                            // "hava temiz" izlenimi verir; oysa olcum hic yoktur.
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
    // Donanim takili mi. "Takili degil" ile "takili ama arizali" ayri seylerdir:
    // takili olmayan sensor icin surekli kirmizi HATA yazmak, gercek bir
    // arizayi da siradanlastirip gozden kacirtir.
    bool sensor1Present;
    bool sensor2Present;
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
    // Yumurta IR sicaklik (yerel MLX90614 oncelikli, WiFi servisi yedek)
    float    eggTemp;          // Son okunan yumurta yuzey sicakligi (°C)
    bool     eggTempValid;     // true = gecerli veri var, false = baglanti yok
    uint8_t  eggTempSource;    // 0=yok, 1=yerel MLX90614, 2=uzak WiFi servisi

    // ---- I/O sagligi ve termal kacis ----
    // Bunlar donanim arizasidir, ortam kosulu degil. Ekranda gorunmezlerse
    // kullanici cikislarin kontrol edilemedigini hic ogrenemez.
    bool     relayOK;          // Role kartina yazilabiliyor mu
    bool     muxOK;            // I2C multiplexer ayakta mi
    uint32_t muxRecoverCount;  // Acilistan beri bus kurtarma sayisi (tani)
    bool     thermalRunaway;   // Kapatildi ama sicaklik dusmuyor -> FIZIKSEL MUDAHALE

#if RELAY_TEST_ENABLED
    // ---- Role testi (GECICI) ----
    bool     relayTestActive;
    bool     relayTestOn[4];   // P0..P3
    uint8_t  relayTestFan;     // fan PWM (role degil, IO18)
#endif
    bool     eggSensorEnabled; // Servis aktif/pasif (kullanici toggle)
    uint8_t  eggDiscoveryStatus; // 0=NONE, 1=RUNNING, 2=OK, 3=FAILED
    // Temizlik / bakim modu
    bool     cleaningActive;         // Temizlik modunda mi
    unsigned long cleaningRemainMs;  // Otomatik bitise kalan sure
    // Candling (dol kontrolu)
    bool     candlingToday;          // Bugun kontrol gunu mu
    const char* candlingLabel;       // "1.Kontrol (%25)" vb. (bos string = yok)
    uint8_t  candlingLockdownDay;    // Lockdown gunu
    // NOT: formatli "DD/MM/YYYY" String alani kaldirildi - hicbir cizim
    // fonksiyonu okumuyordu ama her veri hazirliginda heap'te uretiliyordu.
    // Tarih icin asagidaki sayisal alanlar kullanilir.
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
    // ---- Sistem kontrol modali (Durum sekmesi > DURUM karti) ----
    // Kulucka baslat/duraklat/devam/durdur icin hizli erisim. ONAY ISTER:
    // dokunmatik panel ahirda/kumeste yanlislikla surtunebilir ve stop()
    // NVS'deki kulucka durumunu siler (guc kesintisi kurtarmasi kaybolur).
    // Tek dokunusla 18 gunluk kulucka bitirilememeli.
    bool          _sysCtrlOpen;
    bool          _sysCtrlDrawn;      // bir kez cizildi mi (titreme onleme)
    unsigned long _sysCtrlOpenedMs;   // otomatik kapanma icin

    // Profil sekmesindeki faz listesinin son kaydirma siniri.
    // drawProfile() hesaplar, handleTouch() sinir kontrolu icin okur; boylece
    // "asagi" dokunusu listenin sonunda bosuna sayac artirmaz.
    int  _profMaxScroll;
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
    // Kadran olcek sinirlari. Isinma sirasinda olcum hedef penceresinin
    // disinda kalirsa pencere genisletilir; sinirlar degistiginde etiketler
    // ve hedef isaretcisi (sadece _bgDraw'da cizilir) yenilenmelidir.
    float _prevTempMin;
    float _prevTempMax;
    float _prevHumMin;
    float _prevHumMax;
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
    // Kadran isaretleme modeli (endustriyel HMI pratigine yakin):
    //   target        : SET NOKTASI - tek, kalin turuncu cizgi.
    //                   Sicaklik gibi tek dogru degeri olan olcumler icin.
    //   bandLo/bandHi : KABUL EDILEBILIR ARALIK - soluk yesil yay + iki
    //                   turuncu sinir cizgisi. Alarm esiklerini temsil eder.
    //
    // Nem gibi tek set noktasi OLMAYAN olcumlerde target = GAUGE_NONE gecilir;
    // sadece bant cizilir. Sicaklikta ikisi birden kullanilir: ortada set
    // noktasi, cevresinde tolerans bandi.
    static constexpr float GAUGE_NONE = -9999.0f;

    // Sapma satiri: buyuk degerin altina "hedefe ne kadar kaldi / ne kadar
    // asildi" yazar. Mutlak degeri okuyup farki kafadan cikarmaktan hizlidir.
    // Referans, olcumun dogasina gore degisir:
    enum GaugeDev : uint8_t {
        DEV_NONE = 0,   // satir yok; onun yerine birim yazilir
        DEV_SETPOINT,   // target'a gore sapma  -> tek hedefli (sicaklik)
        DEV_BAND        // bandLo..bandHi'a gore -> aralik hedefli (nem, CO2)
    };

    // Durum sekmesi alt seridi: CO2 + yumurta IR kaynagi + I/O sagligi.
    // Bu uc bilgi daha once TFT'de hic yoktu (CO2 sadece Olcum sekmesinde,
    // digerleri yalnizca durum JSON'unda gorunuyordu).
    void drawStatusStrip(const DisplayData &data);

    // Termal kacis banneri: tum sekmelerin ustunde, yanip sonen kirmizi.
    // Bu durum yazilimin cozemedigi bir donanim arizasidir; kullanicinin
    // hangi sekmede oldugundan bagimsiz olarak gormesi gerekir.
    void drawRunawayBanner(const DisplayData &data);

    // Sistem kontrol onay modali
    void drawSysControl(const DisplayData &data);

#if RELAY_TEST_ENABLED
    // Role test modali (GECICI). PCF8574 rolelerinin tek tek denenmesi.
    void drawRelayTest(const DisplayData &data);
#endif

    void drawGauge(int cx, int cy, int r, float value, float minVal,
                   float maxVal, float target, uint16_t color,
                   const char* label, const char* unit,
                   float bandLo = GAUGE_NONE, float bandHi = GAUGE_NONE,
                   GaugeDev devMode = DEV_NONE, uint8_t devDecimals = 1);
    // Sensor durum gosterimi - TEK KAYNAK.
    // Ayni uc durum eskiden Durum sekmesinde "OK/HATA/YOK", Ayar sekmesinde
    // "OK/X/-" olarak yaziliyordu; ayni sey icin iki farkli dil kullanmak
    // kullaniciyi yaniltiyordu. Iki ekran da artik bu ikiliyi cagirir.
    static const char* sensorStateText(bool ok, bool present);
    uint16_t           sensorStateColor(bool ok, bool present) const;

    // Olcum durum renkleri - TEK KAYNAK.
    // Durum ve Olcum sekmeleri ayni olcumu gosteriyor; "normal/yuksek/dusuk"
    // karari iki yerde ayri hesaplanirsa iki ekran farkli renk gosterebilir.
    // Ikisi de bu fonksiyonlari cagirir.
    uint16_t tempStateColor(float temp, float target) const;
    uint16_t humStateColor(float hum, float low, float high) const;
    uint16_t co2StateColor(uint16_t co2, bool valid,
                           uint16_t low, uint16_t high, uint16_t critical) const;

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
