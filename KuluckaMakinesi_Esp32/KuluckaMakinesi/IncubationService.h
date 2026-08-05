#ifndef INCUBATION_SERVICE_H
#define INCUBATION_SERVICE_H

#include <Arduino.h>
#include "Config.h"
#include "AnimalProfiles.h"
#include "SensorManager.h"
#include "RelayBoard.h"
#include "HeaterDriver.h"
#include "FanDriver.h"
#include "HumidifierDriver.h"
#include "TurnerDriver.h"
#include "StepperTurnerDriver.h"
#include "CoolingSprayDriver.h"
#include "DisplayManager.h"
#include "RTCManager.h"
#include "PIDController.h"
#include "HumidityController.h"
#include "FanController.h"
#include "PhaseManager.h"
#include "AlarmService.h"
#include "EggTempService.h"
#include "EggIRSensor.h"
#include "SafetyService.h"
#include "PersistentStorage.h"
#include "CO2Sensor.h"
#include "SpeakerDriver.h"
#include "CandlingScheduler.h"

// Forward declaration — StorageService.h dahil etmek dairesel bagimliliga
// neden olabilir; sadece pointer/referans kullaniyoruz.
class StorageService;

// Grafik icin history buffer boyutu
#define GRAPH_HISTORY_SIZE 60

// Yumurta sicakliginin hangi kaynaktan geldigi.
// Yerel MLX90614 (MUX CH4) birincildir; gecersizse WiFi yedegine dusulur.
enum EggTempSource : uint8_t {
    EGG_SRC_NONE   = 0,   // Hicbir kaynak gecerli veri vermiyor
    EGG_SRC_LOCAL  = 1,   // MLX90614 - yerel I2C sensor
    EGG_SRC_REMOTE = 2    // EggTempService - ayri ESP32, HTTP (yedek)
};

enum SystemState {
    SYS_INITIALIZING,
    SYS_AUTOTUNING,
    SYS_RUNNING,
    SYS_PAUSED,
    SYS_COMPLETED,
    SYS_EMERGENCY,
    SYS_CLEANING     // Temizlik/bakim modu: tum kontrol manuel, alarmlar yumusatilir
};

struct SystemStatus {
    float    temperature;
    float    humidity;
    float    targetTemp;
    float    targetHumLow;
    float    targetHumHigh;
    uint8_t  heaterPWM;
    uint8_t  fanPWM;
    bool     humidifierOn;
    int      currentDay;
    int      totalDays;
    int      remainingDays;
    int      phaseRemainingDays;
    int      phaseEndDay;
    const char* phaseName;
    const char* profileName;
    SystemState state;
    bool     sensor1OK;
    bool     sensor2OK;
    double   kp, ki, kd;
    bool     alarmActive;
    String   alarmMsg;
    unsigned long uptime;
    // Yumurta IR sicaklik (yerel MLX90614 oncelikli, WiFi servisi yedek)
    float    eggTemp;
    bool     eggTempValid;
    uint8_t  eggTempSource;   // EggTempSource
    String   eggSensorIP;
};

class IncubationService {
public:
    IncubationService();

    void begin();
    void update();

    void setProfile(uint8_t profileIndex);
    void start();
    void pause();
    void resume();
    void stop();

    SystemStatus getStatus() const;
    String getStatusJSON() const;
    // Grafik icin gecmis veri (sicaklik/nem/CO2 - son 60 okuma, 5 sn periyot)
    // Veritabanina aktarim icin de kullanilabilir
    String getHistoryJSON() const;

    // Manuel tarih/saat ayarlama (web UI veya NTP'den).
    // Validate eder, RTC'ye yazar, NVS'i guncel zamanla seed eder.
    // Donus: true = basarili, false = gecersiz timestamp.
    bool setSystemTime(uint32_t unixSec);

    uint32_t getUnixTime() const;

    // WiFi üzerinden erişilebilir kontroller
    void setPIDParams(double kp, double ki, double kd);
    void setTargetTemp(float temp);
    void  setHumidityThresholds(float low, float high);
    float getHumidityLow()  const;   // Aktif esik (manuel override veya profil)
    float getHumidityHigh() const;
    bool  isHumidityManualOverride() const;
    void  clearHumidityOverride();
    void  resetSafety();
    // Alarmi sustur (kullanici onaylama). Speaker durur, ekrandaki
    // alarm gosterimi devam eder. Durum normale donerse alarm temizlenir.
    void acknowledgeAlarm();
    // Aktif alarmi belirli sure kadar ertele (default Config'den)
    void snoozeActiveAlarm(uint32_t durationMs = 0);
    // Aktif alarmi tamamen kapat (bugun icinde tekrar tetiklenmez)
    void dismissActiveAlarm();

    // ---------- Temizlik / Bakim Modu ----------
    // Tum otomatik kontrolleri (PID, fan, nemlendirici, cevirme motoru)
    // devre disi birakir. Kullanici heater/fan/humidifier'i manuel set eder.
    // Alarmlar yumusatilir (sadece kritik guvenlik alarmlari).
    // Otomatik timeout: CLEANING_TIMEOUT_MS sonra onceki duruma donulur.
    bool startCleaning();                                 // Onceki state'i saklar
    void stopCleaning();                                  // Onceki state'e doner

#if RELAY_TEST_ENABLED
    // ---------- ROLE TEST MODU (GECICI) ----------
    // Devreye alma testleri icin PCF8574 (MUX CH7) rolelerinin tek tek
    // acilip kapatilmasi. Otomatik kontrol askiya alinir; kulucka STATE'i
    // DEGISMEZ, test bitince kaldigi yerden devam eder (stop() gibi kayit
    // silmez). Asiri isinma korumasi test sirasinda da calisir.
    // Config.h'de RELAY_TEST_ENABLED 0 yapilinca tumu derlemeden cikar.
    bool startRelayTest();
    void stopRelayTest();
    bool isRelayTestActive() const { return _relayTestActive; }
    void toggleTestRelay(uint8_t bit);      // 0..3 (P0..P3)
    void toggleTestFan();                   // fan PWM 0 <-> FAN_MAX_PWM
    bool    getTestRelay(uint8_t bit) const;
    uint8_t getTestFan() const { return _relayTestFan; }
#endif
    bool isCleaning() const;
    void setCleaningOutputs(uint8_t heaterPWM, uint8_t fanPWM, bool humidifierOn, bool turnerOn);
    uint8_t getCleaningHeater()    const { return _cleaningHeater; }
    uint8_t getCleaningFan()       const { return _cleaningFan; }
    bool    getCleaningHumidifier()const { return _cleaningHum; }
    bool    getCleaningTurner()    const { return _cleaningTurner; }
    unsigned long getCleaningTimeRemainingMs() const;     // 0 = yok / bitti

    // ---------- Dol Kontrolu (Candling) ----------
    // Aktif profilin candling programini dondurur.
    // currentDay ile eslesen kontrol gunu varsa label doner.
    const CandlingSchedule& getCandlingSchedule() const { return _candlingSched; }
    bool isCandlingDayToday() const;
    const char* getCandlingLabelToday() const;

    // ---------- Faz Gecis Gecmisi (Phase Log) ----------
    // Hangi faza ne zaman girildi, ne kadar surdu — kuluckanin tam takibi
    // start() ile sifirlanir, updatePhase() ile guncellenir
    String getPhaseLogJSON() const;
    uint8_t getPhaseLogCount() const { return _phaseLogCount; }

    // Sensor kalibrasyon
    void setSensorCalibration(const SensorCalibration &cal);
    SensorCalibration getSensorCalibration() const;
    
    // CO2 sensoru
    void setCO2Value(uint16_t ppm);   // Manuel deger (sensor yoksa)
    uint16_t getCO2Value() const;
    bool hasCO2Sensor() const;        // Sensor bagli mi
    const char* getCO2SensorType() const;  // Sensor tipi adi

    // Bileşenlere erişim
    PhaseManager& getPhaseManager();
    AlarmService& getAlarmService();
    PersistentStorage& getPersistentStorage();

    // Override sistemi (2026-04-25): WebService olusturulurken cagrilir.
    // nullptr verirse override yok, hep hazir profil kullanilir.
    void setCustomProfileStorage(StorageService* s);
    StorageService* getCustomProfileStorage() const;

    // Aktif profilin aktif fazindaki sicaklik/nem'i delta kadar kaydir.
    // Override yoksa otomatik clone yapilir; varsa CustomProfile guncellenir.
    // Sicaklik delta tipik ±0.1 C, nem delta ±1.0 %. Donus: basarili mi?
    bool adjustActivePhaseTemp(float delta);     // tempEnd ile birlikte (gradyan korunur)
    bool adjustActivePhaseHumLow(float delta);
    bool adjustActivePhaseHumHigh(float delta);
    // Aktif fazin yumurta cevirme araligini (dk) delta kadar kaydir.
    // 0 set edilirse turning otomatik DISABLE olur (cevirme kapali).
    bool adjustActivePhaseTurningInterval(int16_t deltaMin);
    // Aktif profil icin override'i sil (fabrika ayarlarina don)
    bool resetActiveProfileOverride();
    // Aktif profil icin override degistirildiyse PhaseManager'i yeniden yukle.
    // setProfile()'in aksine egg count'u sifirlamaz, profil indexini bozmaz.
    // Web handler'lari override clone/delete sonrasi bunu cagirir.
    void reloadActiveProfile();

private:
    // HAL
    SensorManager    _sensorMgr;
    HeaterDriver     _heater;
    FanDriver        _fan;
    HumidifierDriver _humidifier;
#if TURNER_TYPE == 1
    StepperTurnerDriver _turner;   // A4988/DRV8825 step motor (PCF8574 P4/P5/P6)
#else
    TurnerDriver        _turner;   // Klasik DC/role motor (PCF8574 P2/P3)
#endif
    CoolingSprayDriver _coolSpray;
    DisplayManager   _display;
    RTCManager       _rtc;

    // Control
    PIDController      _pid;
    HumidityController _humCtrl;
    FanController      _fanCtrl;
    PhaseManager       _phaseMgr;

    // Service
    EggTempService _eggTempSvc;   // Uzak (WiFi) yumurta IR - yedek kaynak
    EggIRSensor    _eggIR;        // Yerel MLX90614 (MUX CH4) - birincil kaynak
    AlarmService  _alarm;
    SafetyService _safety;
    PersistentStorage _storage;
    CO2Sensor     _co2Sensor;
    SpeakerDriver _speaker;

    // Override sistemi (2026-04-25): opsiyonel bagimlilik.
    // nullptr ise override kontrolu atlanir, hep ALL_PROFILES kullanilir.
    StorageService* _customProfileStorage;
    // Override aktifken PhaseManager'a verilen pointer'in yasadigi buffer.
    // Static degil, instance member: yasamı IncubationService kadar surer.
    AnimalProfile   _overrideProfileBuffer;

    SystemState   _state;
    unsigned long _lastUpdateTime;
    unsigned long _startMillis;
    
    // CO2 sensoru (SCD30, MUX CH3)
    uint16_t _co2Value;   // Son okunan deger (ppm)
    bool     _co2Valid;   // Gercek sensorden gelen gecerli veri var mi.
                          // false ise CO2 alarm esikleri 0 gecilir -> alarm uretilmez
                          // (uydurma bir deger uzerinden alarm vermek yaniltici olur)

    // Grafik icin history buffer
    float _tempHistory[GRAPH_HISTORY_SIZE];
    float _humHistory[GRAPH_HISTORY_SIZE];
    uint16_t _co2History[GRAPH_HISTORY_SIZE];
    uint16_t _historyIndex;
    uint16_t _historyCount;
    unsigned long _lastHistoryTime;
    unsigned long _lastProgressSaveTime;  // Periyodik NVS ilerleme kaydı
    bool          _humManualOverride;     // Web/API'den manuel nem esigi ayarlandi mi

    // ---------- Temizlik modu state ----------
    unsigned long _cleaningStartTime;     // millis() — başlangıç
    SystemState   _cleaningPrevState;     // Geri donulecek state
    uint8_t       _cleaningHeater;        // 0-255
    uint8_t       _cleaningFan;           // 0-255
    bool          _cleaningHum;           // true=nemlendirici ON
    bool          _cleaningTurner;        // true=çevirme motoru ON

    // ---------- Candling (dol kontrolu) ----------
    CandlingSchedule _candlingSched;        // Aktif profilin kontrol programi
    int             _candlingLastDay;       // Son kontrol edilen gun (tekrar tetikleme onleme)
    void updateCandlingSchedule();          // Profil degistiginde cagrilir

    // ---------- Wall Clock NVS Fallback ----------
    // RTC bozuksa NVS'den okunan zamanin millis offset'i ile saat hesabi.
    // begin()'de NVS'den yuklenir; setSystemTime()'da guncellenir.
    uint32_t      _nvsTimeBaseUnix;
    unsigned long _nvsTimeBaseMs;

    // ---------- Faz Gecis Gecmisi ----------
    // Bir kulucka boyunca max 8 faz olabilir (genelde 2-4)
    static constexpr uint8_t MAX_PHASE_LOG_ENTRIES = 8;
    struct PhaseLogEntry {
        const char* phaseName;     // profile->phaseName pointer (sabit string)
        uint32_t    startUnix;     // Faza giris (Unix timestamp)
        uint32_t    endUnix;       // 0 = halen aktif faz
        uint16_t    startDay;      // 1-based gun
        uint16_t    endDay;        // 0 = halen aktif faz
    };
    PhaseLogEntry _phaseLog[MAX_PHASE_LOG_ENTRIES];
    uint8_t       _phaseLogCount;
    // Faz log entry ekle/kapat
    void recordPhaseTransition(const IncubationPhase* prev,
                               const IncubationPhase* next, int day);

    // Manuel nem override: web/API'den set edildiginde updatePhase() override etmez.
    // setProfile() veya faz gecisinde otomatik temizlenir.
    void applyHumidityOverride(float low, float high);
    // Yumurta sicakligi kaynak secimi: once yerel MLX90614, o gecersizse
    // WiFi uzerindeki EggTempService. Ikisi de yoksa EGG_SRC_NONE.
    void resolveEggTemp(float &temp, bool &valid, uint8_t &source) const;

    // I/O saglik denetimi: role kartina yazilabiliyor mu, MUX ayakta mi?
    // Yazilamiyorsa "isiticiyi kapat" komutlari bosa gidiyor demektir;
    // bu durumda alarm uretilir ve guvenlik servisi kacis moduna alinir.
    void checkIOHealth();

    // Yumurta (kabuk) sicaklik alarmlari. targetTemp aktif fazin hedefidir.
    // Kabuk sicakligi embriyonun gercek sicakligina en yakin olcumdur.
    void checkEggTempAlarms(float targetTemp);

#if RELAY_TEST_ENABLED
    // Role test modu durumu
    bool          _relayTestActive;
    bool          _relayTestOn[4];    // P0..P3
    uint8_t       _relayTestFan;      // fan PWM (rolede degil, IO18 PWM)
    unsigned long _relayTestStartMs;
#endif

    // Yumurta IR kaynagi izleme (alarm zamanlamasi icin)
    unsigned long _eggLastValidMs;   // Son gecerli okuma zamani (0 = hic olmadi)
    bool          _eggEverValid;     // Bir kez bile gecerli veri geldi mi
public:
    EggTempService& getEggTempService() { return _eggTempSvc; }
    EggIRSensor&    getEggIRSensor()    { return _eggIR; }
private:

    void updateHistory();
    void updatePhase();
    void updateControls();
    void updateDisplay();
    void debugLog();
};

#endif // INCUBATION_SERVICE_H
