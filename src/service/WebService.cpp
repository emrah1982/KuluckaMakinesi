#include "WebService.h"
#include "IncubationService.h"

WebService::WebService(IncubationService &incubator, StorageService &storage)
    : _incubator(incubator)
    , _storage(storage)
    , _server(WEB_SERVER_PORT)
    , _wifiConnected(false)
    , _apActive(false)
{
}

StorageService& WebService::getStorage() {
    return _storage;
}

void WebService::begin() {
    // Gercek WiFi SSID var mi kontrol et
    bool hasRealSSID = _hasRealWiFiSSID();

    // AP modunu baslat (gercek SSID varsa AP+STA, yoksa sadece AP)
    setupAP(hasRealSSID);

    // Route tanimlari + sunucu baslat (AP hazir olunca hemen)
    setupRoutes();
    setupAPI();
    _server.begin();
    Serial.println("[WEB] Web sunucu baslatildi - Port: " + String(WEB_SERVER_PORT));

    // STA baglantisi sadece gercek SSID varsa denensin
    if (hasRealSSID) {
        connectWiFi();
    } else {
        Serial.println("[STA] WiFi SSID ayarlanmamis, sadece AP modu aktif");
    }

    Serial.println("--------------------------------------------");
    if (_apActive) {
        Serial.print("[AP]   Hotspot: ");
        Serial.print(AP_SSID);
        Serial.print(" → http://");
        Serial.println(WiFi.softAPIP());
    }
    if (_wifiConnected) {
        Serial.print("[STA]  WiFi:    ");
        Serial.print(WIFI_SSID);
        Serial.print(" → http://");
        Serial.println(WiFi.localIP());
    }
    Serial.println("--------------------------------------------");
}

bool WebService::_hasRealWiFiSSID() {
    StoredSettings s = _storage.loadSettings();
    const char* ssid = s.wifiSSID;
    // Kayitli SSID bos veya placeholder ise gercek SSID yok
    if (strlen(ssid) > 0 && strcmp(ssid, "WIFI_ADI") != 0) {
        return true;
    }
    // Config.h'deki SSID de placeholder mi?
    if (strcmp(WIFI_SSID, "WIFI_ADI") != 0 && strlen(WIFI_SSID) > 0) {
        return true;
    }
    return false;
}

void WebService::setupAP(bool withSTA) {
    // Gercek WiFi varsa AP+STA, yoksa sadece AP modu
    if (withSTA) {
        WiFi.mode(WIFI_AP_STA);
        Serial.println("[WIFI] Mod: AP+STA");
    } else {
        WiFi.mode(WIFI_AP);
        Serial.println("[WIFI] Mod: Sadece AP");
    }
    delay(100);

    // AP baslat (softAP once, config sonra - ESP32 core 3.x gerekliligi)
    bool apOK = WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, false, AP_MAX_CONNECTIONS);
    delay(200);

    // AP IP konfigurasyonu (softAP basladiktan sonra)
    IPAddress apIP(AP_IP);
    IPAddress apGateway(AP_GATEWAY);
    IPAddress apSubnet(AP_SUBNET);
    WiFi.softAPConfig(apIP, apGateway, apSubnet);
    delay(100);

    if (apOK) {
        _apActive = true;
        Serial.print("[AP] Hotspot acildi: ");
        Serial.print(AP_SSID);
        Serial.print(" | Sifre: ");
        Serial.println(AP_PASSWORD);
        Serial.print("[AP] IP: ");
        Serial.println(WiFi.softAPIP());
    } else {
        _apActive = false;
        Serial.println("[AP] HATA: Hotspot acilamadi!");
    }
}

void WebService::connectWiFi() {
    // Kayitli WiFi bilgilerini kontrol et
    StoredSettings s = _storage.loadSettings();
    const char* ssid = s.wifiSSID;
    const char* pass = s.wifiPass;

    // Eger kayitli SSID bos veya varsayilansa Config.h'den al
    if (strlen(ssid) == 0 || strcmp(ssid, "WIFI_ADI") == 0) {
        ssid = WIFI_SSID;
        pass = WIFI_PASSWORD;
    }

    Serial.print("[STA] WiFi'ye baglaniyor: ");
    Serial.println(ssid);

    WiFi.begin(ssid, pass);

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startTime > WIFI_CONNECT_TIMEOUT) {
            Serial.println();
            Serial.println("[STA] WiFi baglanti zaman asimi!");
            Serial.println("[STA] Sadece AP (hotspot) modu aktif");
            _wifiConnected = false;
            return;
        }
        delay(500);
        Serial.print(".");
    }

    _wifiConnected = true;
    Serial.println();
    Serial.print("[STA] WiFi baglandi! IP: ");
    Serial.println(WiFi.localIP());
}

bool WebService::isConnected() const {
    return _wifiConnected && (WiFi.status() == WL_CONNECTED);
}

bool WebService::isAPActive() const {
    return _apActive;
}

String WebService::getIPAddress() const {
    if (isConnected()) {
        return WiFi.localIP().toString();
    }
    return "Bagli degil";
}

String WebService::getAPIPAddress() const {
    if (_apActive) {
        return WiFi.softAPIP().toString();
    }
    return "";
}

void WebService::setupRoutes() {
    // Ana sayfa (PROGMEM)
    _server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", PAGE_INDEX_HTML);
    });

    // Statik dosyalar (PROGMEM)
    _server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/css", PAGE_STYLE_CSS);
    });

    _server.on("/app.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "application/javascript", PAGE_APP_JS);
    });

    // 404
    _server.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404, "text/plain", "Sayfa bulunamadi");
    });
}

void WebService::setupAPI() {
    // GET /api/status
    _server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetStatus(request);
    });

    // GET /api/alarm
    _server.on("/api/alarm", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetAlarm(request);
    });

    // GET /api/profiles
    _server.on("/api/profiles", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetProfiles(request);
    });

    // POST /api/profile
    _server.on("/api/profile", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostProfile(request);
    });

    // POST /api/control
    _server.on("/api/control", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostControl(request);
    });

    // POST /api/pid
    _server.on("/api/pid", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostPID(request);
    });

    // POST /api/humidity
    _server.on("/api/humidity", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostHumidity(request);
    });

    // POST /api/safety
    _server.on("/api/safety", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostSafety(request);
    });

    // GET /api/custom-profiles
    _server.on("/api/custom-profiles", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetCustomProfiles(request);
    });

    // POST /api/custom-profile
    _server.on("/api/custom-profile", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostCustomProfile(request);
    });

    // DELETE /api/custom-profile
    _server.on("/api/custom-profile", HTTP_DELETE, [this](AsyncWebServerRequest *request) {
        handleDeleteCustomProfile(request);
    });

    // POST /api/save-settings
    _server.on("/api/save-settings", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostSaveSettings(request);
    });

    // GET /api/load-settings
    _server.on("/api/load-settings", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetLoadSettings(request);
    });

    // POST /api/wifi
    _server.on("/api/wifi", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostWiFi(request);
    });

    // GET /api/log
    _server.on("/api/log", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetLog(request);
    });

    // DELETE /api/log
    _server.on("/api/log", HTTP_DELETE, [this](AsyncWebServerRequest *request) {
        handleDeleteLog(request);
    });
}

// ==================== API HANDLER'LARI ====================

void WebService::handleGetStatus(AsyncWebServerRequest *request) {
    String json = _incubator.getStatusJSON();
    // JSON'un sonundaki '}' karakterini kaldir, ag bilgisi ekle
    if (json.endsWith("}")) {
        json.remove(json.length() - 1);
        json += ",\"apActive\":" + String(_apActive ? "true" : "false");
        if (_apActive) {
            json += ",\"apSSID\":\"" + String(AP_SSID) + "\"";
            json += ",\"apIP\":\"" + WiFi.softAPIP().toString() + "\"";
            json += ",\"apClients\":" + String(WiFi.softAPgetStationNum());
        }
        json += ",\"staConnected\":" + String(isConnected() ? "true" : "false");
        if (isConnected()) {
            json += ",\"staIP\":\"" + WiFi.localIP().toString() + "\"";
        }
        json += "}";
    }
    request->send(200, "application/json", json);
}

void WebService::handleGetAlarm(AsyncWebServerRequest *request) {
    String json = _incubator.getAlarmService().getAlarmJSON();
    request->send(200, "application/json", json);
}

void WebService::handleGetProfiles(AsyncWebServerRequest *request) {
    String json = "[";
    for (uint8_t i = 0; i < PROFILE_COUNT; i++) {
        if (i > 0) json += ",";
        json += "{\"index\":";
        json += String(i);
        json += ",\"name\":\"";
        json += String(ALL_PROFILES[i]->name);
        json += "\",\"nameEN\":\"";
        json += String(ALL_PROFILES[i]->nameEN);
        json += "\",\"days\":";
        json += String(ALL_PROFILES[i]->totalDays);
        json += "}";
    }
    json += "]";
    request->send(200, "application/json", json);
}

void WebService::handlePostProfile(AsyncWebServerRequest *request) {
    if (request->hasParam("index", true)) {
        uint8_t idx = request->getParam("index", true)->value().toInt();
        if (idx < PROFILE_COUNT) {
            _incubator.setProfile(idx);
            request->send(200, "application/json", "{\"ok\":true,\"msg\":\"Profil secildi: " + String(ALL_PROFILES[idx]->name) + "\"}");
        } else {
            request->send(400, "application/json", "{\"ok\":false,\"msg\":\"Gecersiz profil indexi\"}");
        }
    } else {
        request->send(400, "application/json", "{\"ok\":false,\"msg\":\"index parametresi eksik\"}");
    }
}

void WebService::handlePostControl(AsyncWebServerRequest *request) {
    if (request->hasParam("action", true)) {
        String action = request->getParam("action", true)->value();

        if (action == "start") {
            _incubator.start();
            request->send(200, "application/json", "{\"ok\":true,\"msg\":\"Kulucka baslatildi\"}");
        } else if (action == "pause") {
            _incubator.pause();
            request->send(200, "application/json", "{\"ok\":true,\"msg\":\"Sistem duraklatildi\"}");
        } else if (action == "resume") {
            _incubator.resume();
            request->send(200, "application/json", "{\"ok\":true,\"msg\":\"Sistem devam ediyor\"}");
        } else if (action == "stop") {
            _incubator.stop();
            request->send(200, "application/json", "{\"ok\":true,\"msg\":\"Sistem durduruldu\"}");
        } else {
            request->send(400, "application/json", "{\"ok\":false,\"msg\":\"Gecersiz aksiyon\"}");
        }
    } else {
        request->send(400, "application/json", "{\"ok\":false,\"msg\":\"action parametresi eksik\"}");
    }
}

void WebService::handlePostPID(AsyncWebServerRequest *request) {
    if (request->hasParam("kp", true) && request->hasParam("ki", true) && request->hasParam("kd", true)) {
        double kp = request->getParam("kp", true)->value().toDouble();
        double ki = request->getParam("ki", true)->value().toDouble();
        double kd = request->getParam("kd", true)->value().toDouble();
        _incubator.setPIDParams(kp, ki, kd);
        request->send(200, "application/json",
            "{\"ok\":true,\"msg\":\"PID guncellendi Kp=" + String(kp, 2) +
            " Ki=" + String(ki, 2) + " Kd=" + String(kd, 2) + "\"}");
    } else {
        request->send(400, "application/json", "{\"ok\":false,\"msg\":\"kp, ki, kd parametreleri eksik\"}");
    }
}

void WebService::handlePostHumidity(AsyncWebServerRequest *request) {
    if (request->hasParam("low", true) && request->hasParam("high", true)) {
        float low  = request->getParam("low", true)->value().toFloat();
        float high = request->getParam("high", true)->value().toFloat();
        _incubator.setHumidityThresholds(low, high);
        request->send(200, "application/json",
            "{\"ok\":true,\"msg\":\"Nem esikleri guncellendi: %" + String(low, 1) +
            " - %" + String(high, 1) + "\"}");
    } else {
        request->send(400, "application/json", "{\"ok\":false,\"msg\":\"low, high parametreleri eksik\"}");
    }
}

void WebService::handlePostSafety(AsyncWebServerRequest *request) {
    _incubator.resetSafety();
    request->send(200, "application/json", "{\"ok\":true,\"msg\":\"Guvenlik sifirlandi\"}");
}

// ==================== OZEL PROFIL CRUD ====================

void WebService::handleGetCustomProfiles(AsyncWebServerRequest *request) {
    String json = _storage.getCustomProfilesJSON();
    request->send(200, "application/json", json);
}

void WebService::handlePostCustomProfile(AsyncWebServerRequest *request) {
    // Gerekli parametreler: name, nameEN, totalDays, phaseCount
    // Her evre icin: p0_start, p0_end, p0_temp, p0_humLow, p0_humHigh, p0_turning, p0_name
    if (!request->hasParam("name", true) || !request->hasParam("totalDays", true) ||
        !request->hasParam("phaseCount", true)) {
        request->send(400, "application/json", "{\"ok\":false,\"msg\":\"Eksik parametreler: name, totalDays, phaseCount\"}");
        return;
    }

    CustomProfile cp;
    memset(&cp, 0, sizeof(cp));

    strncpy(cp.name, request->getParam("name", true)->value().c_str(), 31);
    String nameEN = request->hasParam("nameEN", true) ?
        request->getParam("nameEN", true)->value() : request->getParam("name", true)->value();
    strncpy(cp.nameEN, nameEN.c_str(), 31);
    cp.totalDays = request->getParam("totalDays", true)->value().toInt();
    cp.phaseCount = request->getParam("phaseCount", true)->value().toInt();
    if (cp.phaseCount > 4) cp.phaseCount = 4;

    for (uint8_t p = 0; p < cp.phaseCount; p++) {
        String prefix = "p" + String(p) + "_";
        if (request->hasParam(prefix + "start", true)) {
            cp.phases[p].startDay = request->getParam(prefix + "start", true)->value().toInt();
            cp.phases[p].endDay = request->getParam(prefix + "end", true)->value().toInt();
            cp.phases[p].temperature = request->getParam(prefix + "temp", true)->value().toFloat();
            cp.phases[p].humidityLow = request->getParam(prefix + "humLow", true)->value().toFloat();
            cp.phases[p].humidityHigh = request->getParam(prefix + "humHigh", true)->value().toFloat();
            cp.phases[p].turningEnabled = request->getParam(prefix + "turning", true)->value().toInt() == 1;
            if (request->hasParam(prefix + "name", true)) {
                strncpy(cp.phases[p].phaseName, request->getParam(prefix + "name", true)->value().c_str(), 15);
            }
        }
    }
    cp.active = true;

    // Guncelleme mi yoksa yeni mi?
    if (request->hasParam("updateIndex", true)) {
        uint8_t idx = request->getParam("updateIndex", true)->value().toInt();
        if (_storage.updateCustomProfile(idx, cp)) {
            request->send(200, "application/json", "{\"ok\":true,\"msg\":\"Profil guncellendi: " + String(cp.name) + "\"}");
        } else {
            request->send(400, "application/json", "{\"ok\":false,\"msg\":\"Guncelleme basarisiz\"}");
        }
    } else {
        if (_storage.addCustomProfile(cp)) {
            request->send(200, "application/json", "{\"ok\":true,\"msg\":\"Profil eklendi: " + String(cp.name) + "\"}");
        } else {
            request->send(400, "application/json", "{\"ok\":false,\"msg\":\"Maks profil sayisina ulasildi (10)\"}");
        }
    }
}

void WebService::handleDeleteCustomProfile(AsyncWebServerRequest *request) {
    if (request->hasParam("index")) {
        uint8_t idx = request->getParam("index")->value().toInt();
        if (_storage.deleteCustomProfile(idx)) {
            request->send(200, "application/json", "{\"ok\":true,\"msg\":\"Profil silindi\"}");
        } else {
            request->send(400, "application/json", "{\"ok\":false,\"msg\":\"Gecersiz index\"}");
        }
    } else {
        request->send(400, "application/json", "{\"ok\":false,\"msg\":\"index parametresi eksik\"}");
    }
}

// ==================== AYAR KAYDETME / YUKLEME ====================

void WebService::handlePostSaveSettings(AsyncWebServerRequest *request) {
    SystemStatus st = _incubator.getStatus();
    StoredSettings s;
    s.kp = st.kp;
    s.ki = st.ki;
    s.kd = st.kd;
    s.targetTemp = st.targetTemp;
    s.humLow = st.targetHumLow;
    s.humHigh = st.targetHumHigh;
    s.profileIndex = 0;
    s.isCustomProfile = false;
    strncpy(s.wifiSSID, WIFI_SSID, 32);
    strncpy(s.wifiPass, WIFI_PASSWORD, 64);
    _storage.saveSettings(s);
    request->send(200, "application/json", "{\"ok\":true,\"msg\":\"Ayarlar ESP hafizasina kaydedildi\"}");
}

void WebService::handleGetLoadSettings(AsyncWebServerRequest *request) {
    StoredSettings s = _storage.loadSettings();
    String json = "{";
    json += "\"kp\":" + String(s.kp, 2);
    json += ",\"ki\":" + String(s.ki, 2);
    json += ",\"kd\":" + String(s.kd, 2);
    json += ",\"profileIndex\":" + String(s.profileIndex);
    json += ",\"isCustomProfile\":" + String(s.isCustomProfile ? "true" : "false");
    json += ",\"targetTemp\":" + String(s.targetTemp, 1);
    json += ",\"humLow\":" + String(s.humLow, 1);
    json += ",\"humHigh\":" + String(s.humHigh, 1);
    json += ",\"ssid\":\"" + String(s.wifiSSID) + "\"";
    json += "}";
    request->send(200, "application/json", json);
}

// ==================== WIFI ====================

void WebService::handlePostWiFi(AsyncWebServerRequest *request) {
    if (request->hasParam("ssid", true) && request->hasParam("pass", true)) {
        String ssid = request->getParam("ssid", true)->value();
        String pass = request->getParam("pass", true)->value();
        _storage.saveWiFi(ssid.c_str(), pass.c_str());
        request->send(200, "application/json",
            "{\"ok\":true,\"msg\":\"WiFi kaydedildi. Yeni baglanti icin ESP'yi yeniden baslatin.\"}");
    } else {
        request->send(400, "application/json", "{\"ok\":false,\"msg\":\"ssid ve pass parametreleri gerekli\"}");
    }
}

// ==================== LOG ====================

void WebService::handleGetLog(AsyncWebServerRequest *request) {
    String csv = _storage.getLogCSV();
    request->send(200, "text/csv", csv);
}

void WebService::handleDeleteLog(AsyncWebServerRequest *request) {
    _storage.clearLog();
    request->send(200, "application/json", "{\"ok\":true,\"msg\":\"Log temizlendi\"}");
}
