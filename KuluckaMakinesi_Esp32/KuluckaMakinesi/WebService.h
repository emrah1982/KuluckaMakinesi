#ifndef WEB_SERVICE_H
#define WEB_SERVICE_H

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include "Config.h"
#include "web_server.h"
#include "AnimalProfiles.h"
#include "StorageService.h"
#include "OTAService.h"
#include "OTAUpdater.h"

// Forward declaration
class IncubationService;

class WebService {
public:
    WebService(IncubationService &incubator, StorageService &storage);

    void begin();
    // Ana loop'tan periyodik cagrilir: DNS captive portal islemi + STA baglanti durumu izleme
    void update();
    void connectWiFi();
    bool isConnected() const;
    bool isCaptivePortalActive() const;
    String getIPAddress() const;
    String getAPIPAddress() const;
    bool isAPActive() const;

    StorageService& getStorage();
    OTAService& getOTAService();
    OTAUpdater& getOTAUpdater();

private:
    IncubationService &_incubator;
    StorageService    &_storage;
    AsyncWebServer     _server;
    OTAService         _ota;
    OTAUpdater         _otaPull;       // Internet uzerinden pull-OTA
    DNSServer          _dnsServer;
    bool               _wifiConnected;
    bool               _apActive;
    bool               _captiveActive;
    unsigned long      _lastWiFiCheck;

    void setupAP(bool withSTA);
    void startCaptivePortal();
    void stopCaptivePortal();
    bool _hasRealWiFiSSID();
    bool _tryConnectAsync(const String &ssid, const String &pass);

    void setupRoutes();
    void setupAPI();

    // Mevcut API handler'lari
    void handleGetStatus(AsyncWebServerRequest *request);
    void handleGetAlarm(AsyncWebServerRequest *request);
    void handleGetHistory(AsyncWebServerRequest *request);
    void handleGetPhaseLog(AsyncWebServerRequest *request);
    void handleGetProfiles(AsyncWebServerRequest *request);
    void handlePostProfile(AsyncWebServerRequest *request);
    void handlePostControl(AsyncWebServerRequest *request);
    void handlePostPID(AsyncWebServerRequest *request);
    void handlePostHumidity(AsyncWebServerRequest *request);
    void handlePostSafety(AsyncWebServerRequest *request);
    void handlePostAlarmAck(AsyncWebServerRequest *request);

    // Ozel profil CRUD
    void handleGetCustomProfiles(AsyncWebServerRequest *request);
    void handlePostCustomProfile(AsyncWebServerRequest *request);
    void handleDeleteCustomProfile(AsyncWebServerRequest *request);

    // Profil override (hazir profili kullanici icin duzenlenebilir kil)
    void handlePostProfileOverrideClone(AsyncWebServerRequest *request);
    void handleDeleteProfileOverride(AsyncWebServerRequest *request);

    // Ayar kaydetme / yukleme
    void handlePostSaveSettings(AsyncWebServerRequest *request);
    void handleGetLoadSettings(AsyncWebServerRequest *request);

    // Yumurta IR sensoru IP yonetimi
    void handlePostEggSensorIP(AsyncWebServerRequest *request);
    void handleGetEggSensorIP(AsyncWebServerRequest *request);

    // Temizlik / bakim modu
    // POST /api/cleaning  body: action=start|stop  veya
    //   action=set&heater=0-255&fan=0-255&hum=0|1&turner=0|1
    void handlePostCleaning(AsyncWebServerRequest *request);

    // WiFi ayarlari
    void handlePostWiFi(AsyncWebServerRequest *request);

    // Log
    void handleGetLog(AsyncWebServerRequest *request);
    void handleDeleteLog(AsyncWebServerRequest *request);

    // Sensor kalibrasyon
    void handleGetCalibration(AsyncWebServerRequest *request);
    void handlePostCalibration(AsyncWebServerRequest *request);

    // Cihaz kimligi (deviceId, isim, FW versiyonu)
    void handleGetIdentity(AsyncWebServerRequest *request);
    void handlePostDeviceName(AsyncWebServerRequest *request);

    // Captive portal (kullanici WiFi kurulumu)
    void handleGetPortal(AsyncWebServerRequest *request);
    void handleGetWifiScan(AsyncWebServerRequest *request);
    void handleGetWifiStatus(AsyncWebServerRequest *request);
    void handlePostWifiConnect(AsyncWebServerRequest *request);
    void handleCaptiveTrigger(AsyncWebServerRequest *request);

    // Internet uzerinden firmware guncelleme (pull-OTA)
    void handleGetOtaOnlinePage(AsyncWebServerRequest *request);
    void handleGetOtaStatus(AsyncWebServerRequest *request);
    void handlePostOtaCheck(AsyncWebServerRequest *request);
    void handlePostOtaPull(AsyncWebServerRequest *request);
    void handlePostOtaUrl(AsyncWebServerRequest *request);
};

#endif // WEB_SERVICE_H
