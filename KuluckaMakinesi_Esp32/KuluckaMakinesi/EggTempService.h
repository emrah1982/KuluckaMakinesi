#ifndef EGG_TEMP_SERVICE_H
#define EGG_TEMP_SERVICE_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// =====================================================================
//  EggTempService
//  Yumurta ESP32'deki GY-906 IR sensöründen HTTP ile sıcaklık çeker.
//  Ayrı FreeRTOS task'ı kullanır — ana kontrol döngüsünü bloklamaz.
//
//  Kullanım:
//    EggTempService _eggSvc;
//    _eggSvc.setIP("192.168.1.50");
//    _eggSvc.begin();                // FreeRTOS task başlatır
//    ...
//    float t = _eggSvc.getEggTemp();
//    bool  v = _eggSvc.isValid();
// =====================================================================
class EggTempService {
public:
    EggTempService();

    void begin();

    void   setIP(const String& ip);
    String getIP()          const;
    bool   isConfigured()   const;

    void   setEnabled(bool en);
    bool   isEnabled()      const;

    float getEggTemp()      const;
    float getAmbientTemp()  const;
    bool  isValid()         const;

    // ---------- Otomatik IP kesfi (mDNS) ----------
    // Asenkron: kisa omurlu task'ta calisir, ana kodu bloklamaz.
    enum DiscoveryStatus : uint8_t {
        DISC_NONE    = 0,   // hic istenmemis / temizlendi
        DISC_RUNNING = 1,   // mDNS sorgusu devam ediyor
        DISC_OK      = 2,   // bulundu (IP guncellendi)
        DISC_FAILED  = 3    // bulunamadi (timeout)
    };
    void            requestDiscovery();          // fire-and-forget
    DiscoveryStatus getDiscoveryStatus() const;  // UI polling icin

private:
    static void  fetchTask(void* pvParameters);
    static void  discoveryTask(void* pvParameters);
    static float parseJsonFloat(const String& json, const char* key);

    String _ip;
    float  _eggTemp;
    float  _ambientTemp;
    bool   _valid;
    bool   _enabled;       // Kullanici aktif/pasif kontrolu

    volatile uint8_t _discoveryStatus;  // DiscoveryStatus enum
    TaskHandle_t     _discoveryTaskHandle;

    mutable SemaphoreHandle_t _mutex;
    TaskHandle_t              _taskHandle;
};

#endif // EGG_TEMP_SERVICE_H
