#ifndef WEB_SERVICE_H
#define WEB_SERVICE_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "../config/Config.h"
#include "web_server.h"
#include "../config/AnimalProfiles.h"
#include "StorageService.h"

// Forward declaration
class IncubationService;

class WebService {
public:
    WebService(IncubationService &incubator, StorageService &storage);

    void begin();
    void connectWiFi();
    bool isConnected() const;
    String getIPAddress() const;
    String getAPIPAddress() const;
    bool isAPActive() const;

    StorageService& getStorage();

private:
    IncubationService &_incubator;
    StorageService    &_storage;
    AsyncWebServer     _server;
    bool               _wifiConnected;
    bool               _apActive;

    void setupAP(bool withSTA);
    bool _hasRealWiFiSSID();

    void setupRoutes();
    void setupAPI();

    // Mevcut API handler'lari
    void handleGetStatus(AsyncWebServerRequest *request);
    void handleGetAlarm(AsyncWebServerRequest *request);
    void handleGetProfiles(AsyncWebServerRequest *request);
    void handlePostProfile(AsyncWebServerRequest *request);
    void handlePostControl(AsyncWebServerRequest *request);
    void handlePostPID(AsyncWebServerRequest *request);
    void handlePostHumidity(AsyncWebServerRequest *request);
    void handlePostSafety(AsyncWebServerRequest *request);

    // Ozel profil CRUD
    void handleGetCustomProfiles(AsyncWebServerRequest *request);
    void handlePostCustomProfile(AsyncWebServerRequest *request);
    void handleDeleteCustomProfile(AsyncWebServerRequest *request);

    // Ayar kaydetme / yukleme
    void handlePostSaveSettings(AsyncWebServerRequest *request);
    void handleGetLoadSettings(AsyncWebServerRequest *request);

    // WiFi ayarlari
    void handlePostWiFi(AsyncWebServerRequest *request);

    // Log
    void handleGetLog(AsyncWebServerRequest *request);
    void handleDeleteLog(AsyncWebServerRequest *request);
};

#endif // WEB_SERVICE_H
