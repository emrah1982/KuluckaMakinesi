#ifndef ALARM_SERVICE_H
#define ALARM_SERVICE_H

#include <Arduino.h>
#include "Config.h"
#include "RelayBoard.h"

// Forward declaration — speaker driver opsiyonel bagimlilik
class SpeakerDriver;

enum AlarmType {
    ALARM_NONE,
    ALARM_TEMP_HIGH,
    ALARM_TEMP_LOW,
    ALARM_HUM_HIGH,
    ALARM_HUM_LOW,
    ALARM_CO2_HIGH,         // CO₂ üst limit aşıldı
    ALARM_CO2_CRITICAL,     // CO₂ kritik seviye
    ALARM_SENSOR_FAIL,
    ALARM_SENSOR_MISMATCH,      // Iki sensor calisiyor ama degerler uyumsuz
    ALARM_SAFETY_SHUTDOWN,
    ALARM_TURNING_STOPPED,
    ALARM_INCUBATION_COMPLETE,
    ALARM_CANDLING_DUE,         // Dol kontrolu zamani geldi
    ALARM_POWER_RECOVERY,
    ALARM_PHASE_TRANSITION,     // Faz gecisi gerceklesti (Gelisim->Cikim/Lockdown vb.)
    ALARM_CANDLING_TOMORROW,    // Yarin dol kontrolu (T-1 gun erken hatirlatma)
    ALARM_IO_FAIL,              // Role/I2C yazma arizasi - cikislar kontrol edilemiyor
    ALARM_THERMAL_RUNAWAY,      // Kapatildi ama sicaklik dusmuyor (role yapismis)
    ALARM_EGG_TEMP_HIGH,        // Yumurta kabuk sicakligi cok yuksek (embriyo riski)
    ALARM_EGG_TEMP_LOW,         // Yumurta kabuk sicakligi cok dusuk
    ALARM_EGG_SENSOR_LOST,      // Yumurta IR kaynagi kayboldu (yerel + uzak yok)
    // Nemlendirici calisiyor ama nem yukselmiyor -> su deposu bos olabilir.
    // ALARM_HUM_LOW'dan AYRI bir tip olmasi kasitli: kullanici "dusuk nem"
    // alarmini kapatmis olsa bile bu ariza uyarisi yine de gorunur.
    ALARM_HUMIDIFIER_FAIL
};

struct AlarmEvent {
    AlarmType   type;
    String      message;
    unsigned long timestamp;
    bool        active;
};

#define MAX_ALARM_HISTORY 20

class AlarmService {
public:
    AlarmService();

    void begin();
    // ESKI imza (statik esikler) — geriye uyum, ama yeni kodda kullanmayin.
    void check(float temperature, float humidity, bool sensorOK,
               uint16_t co2 = 0, uint16_t co2High = 0, uint16_t co2Critical = 0);

    // YENI: profil bazli dinamik esikler. targetTemp = aktif fazin hedef sicakligi,
    // tempTolerance = +/- toleransi (Config.h ALARM_TEMP_TOLERANCE), humLow/High =
    // aktif fazin nem aralig. Bu degerler aktif faza/profile gore degisir.
    void checkDynamic(float temperature, float humidity, bool sensorOK,
                      float targetTemp, float tempTolerance,
                      float humLow, float humHigh,
                      uint16_t co2 = 0, uint16_t co2High = 0, uint16_t co2Critical = 0);

    // Her dongude cagrilir. Hoparlorun DURUMUNU alarm durumuyla esitler.
    //
    // Neden gerekli: startAlarmPattern() yalnizca triggerAlarm() icinde
    // cagriliyordu, yani ses OLAY tabanliydi. Susturma suresi dolunca
    // modal aninda geri geliyordu (shouldAutoShowModal DURUM tabanli) ama
    // ses ancak bir sonraki triggerAlarm cagrisinda basliyordu; o da
    // ALARM_COOLDOWN (60 sn) ile gecikiyordu. Tekrar tetiklenmeyen alarm
    // tiplerinde (candling, faz gecisi) ses hic gelmiyordu.
    void update();

    void triggerAlarm(AlarmType type, const String &message);
    void clearAlarm(AlarmType type);
    void clearAll();

    // Kullanici tarafindan alarmi sustur (speaker durur, ekrandaki gosterim
    // devam eder; durum normale donerse alarm temizlenir, bozulursa
    // ALARM_MUTE_DURATION_MS sonra tekrar calar).
    void acknowledgeAlarm();
    // Belirli sure kadar ertele (susturur — modal kapanir, sure sonunda tekrar calar).
    // durationMs = 0 ise default ALARM_MUTE_DURATION_MS kullanir.
    void snoozeAlarm(uint32_t durationMs = 0);
    // Alarmi tamamen kapat: aktif alarm temizlenir + bu tip ayni gun icinde
    // tekrar tetiklenmesin diye uzun sureli mute (~24 saat) uygulanir.
    void dismissAlarm();
    bool isMuted() const;
    // UI'a "tam ekran alarm modal'i otomatik ac" sinyali. Alarm aktif VE
    // mute degilse true. Kullanici sustur tikladiginda mute aktif olur ve
    // modal kapanir; mute biter + alarm hala aktif ise tekrar true doner.
    bool shouldAutoShowModal() const;

    // Hoparlor bagla (begin'den sonra ya da setter ile)
    void setSpeaker(SpeakerDriver* spk) { _speaker = spk; }

    bool hasActiveAlarm() const;
    AlarmType getActiveAlarmType() const;
    String getActiveAlarmMessage() const;

    const AlarmEvent* getHistory() const;
    uint8_t getHistoryCount() const;

    String getAlarmJSON() const;

private:
    AlarmEvent _history[MAX_ALARM_HISTORY];
    uint8_t    _historyIndex;
    uint8_t    _historyCount;

    AlarmType  _activeAlarm;
    String     _activeMessage;
    unsigned long _lastAlarmTime;
    bool       _alarmActive;

    // Mute/ack — kullanici sustursa speaker durur ama alarm durumu kalir.
    bool          _muted;
    unsigned long _muteUntilMs;
    AlarmType     _mutedType;       // hangi alarm icin sustur (yeni tip gelirse tekrar cal)
    SpeakerDriver* _speaker;
};

#endif // ALARM_SERVICE_H
