#include "WebService.h"
#include "IncubationService.h"
#include "SensorManager.h"
#include "DeviceIdentity.h"
#include <ESPmDNS.h>

WebService::WebService(IncubationService &incubator, StorageService &storage)
    : _incubator(incubator)
    , _storage(storage)
    , _server(WEB_SERVER_PORT)
    , _wifiConnected(false)
    , _apActive(false)
    , _captiveActive(false)
    , _lastWiFiCheck(0)
{
}

StorageService& WebService::getStorage() {
    return _storage;
}

OTAService& WebService::getOTAService() {
    return _ota;
}

OTAUpdater& WebService::getOTAUpdater() {
    return _otaPull;
}

void WebService::begin() {
    // Gercek WiFi SSID var mi kontrol et
    bool hasRealSSID = _hasRealWiFiSSID();

    // AP modunu baslat (gercek SSID varsa AP+STA, yoksa sadece AP)
    setupAP(hasRealSSID);

    // Route tanimlari + sunucu baslat (AP hazir olunca hemen)
    setupRoutes();
    setupAPI();
    
    // OTA servisini baslat
    _ota.begin(_server);

    // Pull-based OTA (internet uzerinden) — NVS'den URL'i yukler
    _otaPull.begin();

    _server.begin();
    Serial.println("[WEB] Web sunucu baslatildi - Port: " + String(WEB_SERVER_PORT));

    // STA baglantisi sadece gercek SSID varsa denensin
    if (hasRealSSID) {
        connectWiFi();
    } else {
        Serial.println("[STA] WiFi SSID ayarlanmamis, sadece AP modu aktif");
    }

    // Captive portal: STA henuz baglanmadiysa baslat (kullanici WiFi kurulumu icin)
    // STA bagli olunca update() icinde otomatik kapanir
    if (_apActive && !_wifiConnected) {
        startCaptivePortal();
    }

    // mDNS baslat — "kulucka-a4c138f2.local" hostname'i ile
    // AP veya STA aktif oldugu surece her iki ag uzerinden de cozumlenir.
    if (_apActive || _wifiConnected) {
        const char* host = DeviceIdentity::getMdnsHostname().c_str();
        if (MDNS.begin(host)) {
            MDNS.addService("http", "tcp", WEB_SERVER_PORT);
            // Cihaz kesfi icin ozel servis — merkezi panel/scanner bunu sorgular
            MDNS.addService("kulucka", "tcp", WEB_SERVER_PORT);
            MDNS.addServiceTxt("kulucka", "tcp", "id",   DeviceIdentity::getDeviceId());
            MDNS.addServiceTxt("kulucka", "tcp", "name", DeviceIdentity::getDeviceName());
            MDNS.addServiceTxt("kulucka", "tcp", "fw",   DeviceIdentity::getFirmwareVersion());
            Serial.printf("[mDNS] %s.local yayinda (port %d)\n", host, WEB_SERVER_PORT);
        } else {
            Serial.println("[mDNS] HATA: baslatilamadi");
        }
    }

    Serial.println("--------------------------------------------");
    Serial.printf("[ID]   %s (%s)\n",
                  DeviceIdentity::getDeviceName().c_str(),
                  DeviceIdentity::getDeviceId().c_str());
    if (_apActive) {
        Serial.print("[AP]   Hotspot: ");
        Serial.print(DeviceIdentity::getApSsid());
        Serial.print(" → http://");
        Serial.println(WiFi.softAPIP());
    }
    if (_wifiConnected) {
        Serial.print("[STA]  WiFi:    ");
        Serial.print(WIFI_SSID);
        Serial.print(" → http://");
        Serial.println(WiFi.localIP());
    }
    if (_apActive || _wifiConnected) {
        Serial.printf("[mDNS] http://%s.local\n",
                      DeviceIdentity::getMdnsHostname().c_str());
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

    // AP SSID — DeviceIdentity'den (her cihazda farkli, yan yana cakismaz)
    const char* apSsid = DeviceIdentity::getApSsid().c_str();

    // AP baslat (softAP once, config sonra - ESP32 core 3.x gerekliligi)
    bool apOK = WiFi.softAP(apSsid, AP_PASSWORD, AP_CHANNEL, false, AP_MAX_CONNECTIONS);
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
        Serial.print(apSsid);
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

bool WebService::isCaptivePortalActive() const {
    return _captiveActive;
}

// ============================================================
//  CAPTIVE PORTAL — DNS yonlendirme
// ============================================================
void WebService::startCaptivePortal() {
    if (!_apActive) return;
    if (_captiveActive) return;

    // Tum DNS sorgulari AP IP'sine cevap dondurur — herhangi bir alan adi -> portal
    if (_dnsServer.start(CAPTIVE_DNS_PORT, "*", WiFi.softAPIP())) {
        _captiveActive = true;
        Serial.printf("[CAPTIVE] Portal acildi: http://%s%s\n",
                      WiFi.softAPIP().toString().c_str(), CAPTIVE_PORTAL_URL);
    } else {
        Serial.println("[CAPTIVE] HATA: DNS server baslatilamadi");
    }
}

void WebService::stopCaptivePortal() {
    if (!_captiveActive) return;
    _dnsServer.stop();
    _captiveActive = false;
    Serial.println("[CAPTIVE] Portal kapatildi (STA bagli)");
}

// Loop'tan periyodik cagrilir — DNS isleme + STA durum izleme + pull-OTA isteklerinin islenmesi
void WebService::update() {
    if (_captiveActive) {
        _dnsServer.processNextRequest();
    }

    // Pull-OTA: kullanici endpoint'ten istek tetikledi mi diye bakilir,
    // tetiklendiyse blocking indirme/dogrulama burada yurutulur (loop duraklar).
    _otaPull.update();

    // STA durumunu 1sn'de bir kontrol et (sik polling fayda etmez)
    unsigned long now = millis();
    if (now - _lastWiFiCheck < 1000) return;
    _lastWiFiCheck = now;

    bool nowConnected = (WiFi.status() == WL_CONNECTED);
    if (nowConnected != _wifiConnected) {
        _wifiConnected = nowConnected;
        if (nowConnected) {
            Serial.print("[STA] Bagli: ");
            Serial.println(WiFi.localIP());
            // STA bagli — captive portal'a artik gerek yok
            stopCaptivePortal();
        } else {
            Serial.println("[STA] Baglanti koptu");
            // STA dustu, AP hala varsa portal'i tekrar ac
            if (_apActive) startCaptivePortal();
        }
    }
}

// Kayitli ssid/pass ile non-blocking baglanma denemesi (portal flow icin)
bool WebService::_tryConnectAsync(const String &ssid, const String &pass) {
    // AP'yi koru, STA'yi acmak icin moda gecisi garantile
    wifi_mode_t mode = WiFi.getMode();
    if (mode == WIFI_AP) {
        WiFi.mode(WIFI_AP_STA);
        delay(100);
    }
    WiFi.disconnect(false, true);  // ssid/pass'i sil ama AP'yi koru
    delay(100);
    return WiFi.begin(ssid.c_str(), pass.c_str()) != WL_CONNECT_FAILED;
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

    // ----- Pull-based OTA (Internet uzerinden firmware guncelleme) -----
    // /ota — kullanici arayuzu
    _server.on("/ota", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetOtaOnlinePage(request);
    });

    // ----- Captive Portal -----
    // Portal kurulum sayfasi
    _server.on(CAPTIVE_PORTAL_URL, HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetPortal(request);
    });

    // Captive portal trigger URL'leri — OS otomatik bunlari sorgular,
    // 302 ile portal'a yonlendirilince "Sign in to network" dialogu acilir
    // iOS / macOS
    _server.on("/hotspot-detect.html",         HTTP_GET, [this](AsyncWebServerRequest *r){ handleCaptiveTrigger(r); });
    _server.on("/library/test/success.html",   HTTP_GET, [this](AsyncWebServerRequest *r){ handleCaptiveTrigger(r); });
    // Android
    _server.on("/generate_204",                HTTP_GET, [this](AsyncWebServerRequest *r){ handleCaptiveTrigger(r); });
    _server.on("/gen_204",                     HTTP_GET, [this](AsyncWebServerRequest *r){ handleCaptiveTrigger(r); });
    // Windows
    _server.on("/connecttest.txt",             HTTP_GET, [this](AsyncWebServerRequest *r){ handleCaptiveTrigger(r); });
    _server.on("/redirect",                    HTTP_GET, [this](AsyncWebServerRequest *r){ handleCaptiveTrigger(r); });
    _server.on("/ncsi.txt",                    HTTP_GET, [this](AsyncWebServerRequest *r){ handleCaptiveTrigger(r); });
    // Firefox
    _server.on("/canonical.html",              HTTP_GET, [this](AsyncWebServerRequest *r){ handleCaptiveTrigger(r); });
    // Chrome
    _server.on("/success.txt",                 HTTP_GET, [this](AsyncWebServerRequest *r){ handleCaptiveTrigger(r); });

    // 404 — captive aktifken portal'a yonlendir, degilse normal 404
    _server.onNotFound([this](AsyncWebServerRequest *request) {
        if (_captiveActive) {
            handleCaptiveTrigger(request);
            return;
        }
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

    // GET /api/history (sicaklik/nem/CO2 grafik verisi - son 60 okuma)
    _server.on("/api/history", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetHistory(request);
    });

    // GET /api/phase-log (faz gecis gecmisi: hangi faza ne zaman girildi)
    _server.on("/api/phase-log", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetPhaseLog(request);
    });

    // POST /api/time — manuel tarih/saat ayarla (web UI'dan)
    // Form: unix=<unixSec>  veya  date=YYYY-MM-DD&time=HH:MM
    // Hem POST body hem URL query parametre destegi (form parse hatasi yedek)
    // GET /api/time?unix=... — yedek (POST body parse hatasinda kullanilabilir)
    auto timeHandler = [this](AsyncWebServerRequest *request) {
        uint32_t unixSec = 0;
        const char* errMsg = "";
        String parsedDate = "", parsedTime = "";

        // Once unix parametresi (programatik) - body veya query
        bool hasUnix = request->hasParam("unix", true) || request->hasParam("unix", false);
        if (hasUnix) {
            String us = request->hasParam("unix", true)
                       ? request->getParam("unix", true)->value()
                       : request->getParam("unix", false)->value();
            unixSec = (uint32_t)us.toInt();
        } else {
            // date + time formati (body veya query)
            bool hasDate = request->hasParam("date", true) || request->hasParam("date", false);
            bool hasTime = request->hasParam("time", true) || request->hasParam("time", false);
            if (hasDate && hasTime) {
                parsedDate = request->hasParam("date", true)
                             ? request->getParam("date", true)->value()
                             : request->getParam("date", false)->value();
                parsedTime = request->hasParam("time", true)
                             ? request->getParam("time", true)->value()
                             : request->getParam("time", false)->value();
                int y = parsedDate.substring(0, 4).toInt();
                int mo = parsedDate.substring(5, 7).toInt();
                int da = parsedDate.substring(8, 10).toInt();
                int hh = parsedTime.substring(0, 2).toInt();
                int mm = parsedTime.substring(3, 5).toInt();
                if (y >= 2024 && y <= 2099 && mo >= 1 && mo <= 12 &&
                    da >= 1 && da <= 31 && hh <= 23 && mm <= 59) {
                    DateTime dt(y, mo, da, hh, mm, 0);
                    unixSec = (uint32_t)dt.unixtime();
                } else {
                    errMsg = "validation";
                }
            } else {
                errMsg = "missing_params";
            }
        }

        // Setrayim tarihi
        bool ok = false;
        if (unixSec > 0) {
            ok = _incubator.setSystemTime(unixSec);
            if (!ok) errMsg = "rtc_set_failed";
        } else if (errMsg[0] == '\0') {
            errMsg = "no_unix_computed";
        }

        // Debug log (Serial)
        Serial.printf("[API /time] unix=%u date='%s' time='%s' ok=%d err='%s'\n",
                      unixSec, parsedDate.c_str(), parsedTime.c_str(),
                      ok ? 1 : 0, errMsg);

        // JSON cevap (detayli hata)
        String resp = String("{\"ok\":") + (ok ? "true" : "false") +
                      ",\"unix\":" + String(unixSec) +
                      ",\"err\":\"" + errMsg + "\"" +
                      ",\"date\":\"" + parsedDate + "\"" +
                      ",\"time\":\"" + parsedTime + "\"}";
        request->send(ok ? 200 : 400, "application/json", resp);
    };
    _server.on("/api/time", HTTP_POST, timeHandler);
    _server.on("/api/time", HTTP_GET,  timeHandler);   // YEDEK: POST body parse hatalarinda GET kullanilabilir

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

    // POST /api/alarm/ack — kullanici alarmi sustur (10 dk default)
    _server.on("/api/alarm/ack", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostAlarmAck(request);
    });

    // POST /api/alarm/snooze — alarmi ertele (candling icin 1 saat default)
    _server.on("/api/alarm/snooze", HTTP_POST, [this](AsyncWebServerRequest *request) {
        uint32_t durationMs = 0;
        if (request->hasParam("ms", true)) {
            durationMs = (uint32_t)request->getParam("ms", true)->value().toInt();
        }
        _incubator.snoozeActiveAlarm(durationMs);
        request->send(200, "application/json", "{\"ok\":true,\"action\":\"snooze\"}");
    });

    // POST /api/alarm/dismiss — alarmi tamamen kapat (bugun icin tekrar tetiklenmez)
    _server.on("/api/alarm/dismiss", HTTP_POST, [this](AsyncWebServerRequest *request) {
        _incubator.dismissActiveAlarm();
        request->send(200, "application/json", "{\"ok\":true,\"action\":\"dismiss\"}");
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

    // POST /api/profile-override/clone — hazir profili klonla, custom slot olustur
    _server.on("/api/profile-override/clone", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostProfileOverrideClone(request);
    });

    // DELETE /api/profile-override — fabrika ayarlarina dondur (override sil)
    _server.on("/api/profile-override", HTTP_DELETE, [this](AsyncWebServerRequest *request) {
        handleDeleteProfileOverride(request);
    });

    // POST /api/save-settings
    _server.on("/api/save-settings", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostSaveSettings(request);
    });

    // POST /api/cleaning — Temizlik modu kontrolleri
    _server.on("/api/cleaning", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostCleaning(request);
    });

    // POST /api/egg-ip — Yumurta ESP32 IP adresini kaydet
    _server.on("/api/egg-ip", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostEggSensorIP(request);
    });
    // GET /api/egg-ip — Mevcut IP'yi dondur
    _server.on("/api/egg-ip", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetEggSensorIP(request);
    });

    // POST/GET /api/egg-discover — Asenkron mDNS kesfi baslat (fire-and-forget)
    // EggTempService kendi task'inda yumurta.local'u arar.
    // UI /api/egg-discover-status ile durumu polling yapar.
    auto startDiscovery = [this](AsyncWebServerRequest *request) {
        _incubator.getEggTempService().requestDiscovery();
        request->send(200, "application/json",
            "{\"ok\":true,\"status\":\"started\"}");
    };
    _server.on("/api/egg-discover", HTTP_GET,  startDiscovery);
    _server.on("/api/egg-discover", HTTP_POST, startDiscovery);

    // GET /api/egg-discover-status — Kesif durumu (UI polling)
    _server.on("/api/egg-discover-status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        EggTempService& svc = _incubator.getEggTempService();
        EggTempService::DiscoveryStatus s = svc.getDiscoveryStatus();
        const char* sStr = "none";
        switch (s) {
            case EggTempService::DISC_RUNNING: sStr = "running"; break;
            case EggTempService::DISC_OK:      sStr = "ok";      break;
            case EggTempService::DISC_FAILED:  sStr = "failed";  break;
            default:                            sStr = "none";    break;
        }
        // OK durumunda IP'yi NVS'e kalici yap (idempotent: zaten kayitliysa overwrite)
        if (s == EggTempService::DISC_OK) {
            String ip = svc.getIP();
            StoredSettings saved = _storage.loadSettings();
            if (strcmp(saved.yumurtaIP, ip.c_str()) != 0) {
                strncpy(saved.yumurtaIP, ip.c_str(), sizeof(saved.yumurtaIP) - 1);
                saved.yumurtaIP[sizeof(saved.yumurtaIP) - 1] = '\0';
                _storage.saveSettings(saved);
            }
        }
        String resp = "{\"status\":\"" + String(sStr) + "\",\"ip\":\"" +
                      svc.getIP() + "\"}";
        request->send(200, "application/json", resp);
    });

    // GET /api/load-settings
    _server.on("/api/load-settings", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetLoadSettings(request);
    });

    // POST /api/wifi (eski) — sadece kaydet, reboot gerek
    _server.on("/api/wifi", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostWiFi(request);
    });

    // GET /api/wifi/scan — etraftaki WiFi aglarini tara (asenkron)
    // Parametreler: ?refresh=1 ile yeni tarama tetiklenir, parametresizken son sonucu doner
    _server.on("/api/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetWifiScan(request);
    });

    // GET /api/wifi/status — STA baglanti durumu (portal polling icin)
    _server.on("/api/wifi/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetWifiStatus(request);
    });

    // POST /api/wifi/connect — kaydet + bagla (reboot olmadan)
    _server.on("/api/wifi/connect", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostWifiConnect(request);
    });

    // GET /api/log
    _server.on("/api/log", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetLog(request);
    });

    // DELETE /api/log
    _server.on("/api/log", HTTP_DELETE, [this](AsyncWebServerRequest *request) {
        handleDeleteLog(request);
    });

    // GET /api/calibration
    _server.on("/api/calibration", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetCalibration(request);
    });

    // POST /api/calibration
    _server.on("/api/calibration", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostCalibration(request);
    });

    // GET /api/identity — cihaz kimligi (deviceId, isim, FW versiyonu, MAC, AP SSID, mDNS)
    _server.on("/api/identity", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetIdentity(request);
    });

    // POST /api/device-name — kullanici tanimli cihaz ismini gunceller
    _server.on("/api/device-name", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostDeviceName(request);
    });

    // ----- Pull-based OTA endpoint'leri -----
    // GET  /api/ota/status — JSON: state, progress, remoteVersion, updateUrl, ...
    _server.on("/api/ota/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetOtaStatus(request);
    });

    // POST /api/ota/check — version.json indirme + semver karsilastirma tetikleyici
    _server.on("/api/ota/check", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostOtaCheck(request);
    });

    // POST /api/ota/pull — .bin indir + SHA256 dogrula + flash + reboot
    _server.on("/api/ota/pull", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostOtaPull(request);
    });

    // POST /api/ota/url — version.json adresini NVS'e kaydet
    _server.on("/api/ota/url", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostOtaUrl(request);
    });
}

// ==================== API HANDLER'LARI ====================

void WebService::handleGetStatus(AsyncWebServerRequest *request) {
    String json = _incubator.getStatusJSON();
    // JSON'un sonundaki '}' karakterini kaldir, kimlik + ag bilgisi ekle
    if (json.endsWith("}")) {
        json.remove(json.length() - 1);
        json += ",\"deviceId\":\""   + DeviceIdentity::getDeviceId()   + "\"";
        json += ",\"deviceName\":\"" + DeviceIdentity::getDeviceName() + "\"";
        json += ",\"fwVersion\":\""  + String(DeviceIdentity::getFirmwareVersion()) + "\"";
        json += ",\"eggCount\":" + String(_storage.getEggCount());
        json += ",\"apActive\":" + String(_apActive ? "true" : "false");
        if (_apActive) {
            json += ",\"apSSID\":\"" + DeviceIdentity::getApSsid() + "\"";
            json += ",\"apIP\":\"" + WiFi.softAPIP().toString() + "\"";
            json += ",\"apClients\":" + String(WiFi.softAPgetStationNum());
        }
        json += ",\"mdns\":\"" + DeviceIdentity::getMdnsHostname() + ".local\"";
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

void WebService::handleGetHistory(AsyncWebServerRequest *request) {
    String json = _incubator.getHistoryJSON();
    request->send(200, "application/json", json);
}

void WebService::handleGetPhaseLog(AsyncWebServerRequest *request) {
    String json = _incubator.getPhaseLogJSON();
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
        // Override durumu (kullanici bu hazir profili duzenlemis mi?)
        json += ",\"hasOverride\":";
        json += _storage.hasProfileOverride(i) ? "true" : "false";
        json += ",\"overrideIdx\":";
        json += String(_storage.getOverrideCustomIdx(i));
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

void WebService::handlePostAlarmAck(AsyncWebServerRequest *request) {
    _incubator.acknowledgeAlarm();
    request->send(200, "application/json",
        "{\"ok\":true,\"msg\":\"Alarm susturuldu (10 dk)\"}");
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
            cp.phases[p].tempEnd = request->hasParam(prefix + "tempEnd", true) ?
                request->getParam(prefix + "tempEnd", true)->value().toFloat() : 0.0f;
            cp.phases[p].humidityLow = request->getParam(prefix + "humLow", true)->value().toFloat();
            cp.phases[p].humidityHigh = request->getParam(prefix + "humHigh", true)->value().toFloat();
            cp.phases[p].turningEnabled = request->getParam(prefix + "turning", true)->value().toInt() == 1;
            // V2: faz bazli turning/cooling/spray parametreleri (opsiyonel,
            // gonderilmezse varsayilan=0 -> mantikli default uygulanir)
            cp.phases[p].turningIntervalMin = request->hasParam(prefix + "turnIntMin", true) ?
                (uint16_t)request->getParam(prefix + "turnIntMin", true)->value().toInt() :
                (cp.phases[p].turningEnabled ? 60 : 0);
            cp.phases[p].turningDurationSec = request->hasParam(prefix + "turnDurSec", true) ?
                (uint8_t)request->getParam(prefix + "turnDurSec", true)->value().toInt() :
                (cp.phases[p].turningEnabled ? 15 : 0);
            cp.phases[p].turningAngleDeg = request->hasParam(prefix + "turnAngleDeg", true) ?
                (uint8_t)request->getParam(prefix + "turnAngleDeg", true)->value().toInt() :
                (cp.phases[p].turningEnabled ? TURNER_DEFAULT_ANGLE_DEG : 0);
            cp.phases[p].coolingEnabled = request->hasParam(prefix + "cool", true) &&
                request->getParam(prefix + "cool", true)->value().toInt() == 1;
            cp.phases[p].coolingDurationMin = request->hasParam(prefix + "coolMin", true) ?
                (uint8_t)request->getParam(prefix + "coolMin", true)->value().toInt() : 0;
            cp.phases[p].coolingPerDay = request->hasParam(prefix + "coolPerDay", true) ?
                (uint8_t)request->getParam(prefix + "coolPerDay", true)->value().toInt() : 0;
            cp.phases[p].sprayingEnabled = request->hasParam(prefix + "spray", true) &&
                request->getParam(prefix + "spray", true)->value().toInt() == 1;
            cp.phases[p].sprayingDurationSec = request->hasParam(prefix + "spraySec", true) ?
                (uint8_t)request->getParam(prefix + "spraySec", true)->value().toInt() : 0;
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

// ==================== PROFIL OVERRIDE (2026-04-25) ====================

// POST /api/profile-override/clone?idx=N
// Hazir profili klonlar, custom slot olusturur, override map'e baglar.
// Daha sonra /api/custom-profile?index=M ile duzenleme yapilir.
void WebService::handlePostProfileOverrideClone(AsyncWebServerRequest *request) {
    if (!request->hasParam("idx", true) && !request->hasParam("idx")) {
        request->send(400, "application/json",
            "{\"ok\":false,\"msg\":\"idx parametresi eksik\"}");
        return;
    }
    String idxStr = request->hasParam("idx", true)
                        ? request->getParam("idx", true)->value()
                        : request->getParam("idx")->value();
    uint8_t idx = (uint8_t)idxStr.toInt();
    if (idx >= PROFILE_COUNT) {
        request->send(400, "application/json",
            "{\"ok\":false,\"msg\":\"Gecersiz profil indexi\"}");
        return;
    }
    if (!_storage.cloneProfileToOverride(idx)) {
        request->send(400, "application/json",
            "{\"ok\":false,\"msg\":\"Override olusturulamadi (slot dolu olabilir)\"}");
        return;
    }
    int8_t cidx = _storage.getOverrideCustomIdx(idx);
    String msg = "{\"ok\":true,\"msg\":\"Override olusturuldu: ";
    msg += String(ALL_PROFILES[idx]->name);
    msg += "\",\"profileIdx\":" + String(idx);
    msg += ",\"customIdx\":" + String((int)cidx) + "}";
    request->send(200, "application/json", msg);

    // Aktif profil bu ise PhaseManager'i yeniden yukle (override aktif olsun)
    // reloadActiveProfile egg count'a dokunmaz, sadece override'i uygular.
    if (_incubator.getPhaseManager().getProfileIndex() == idx) {
        _incubator.reloadActiveProfile();
    }
}

// DELETE /api/profile-override?idx=N
// Override'i kaldirir, hazir profil tekrar aktif olur.
void WebService::handleDeleteProfileOverride(AsyncWebServerRequest *request) {
    if (!request->hasParam("idx")) {
        request->send(400, "application/json",
            "{\"ok\":false,\"msg\":\"idx parametresi eksik\"}");
        return;
    }
    uint8_t idx = (uint8_t)request->getParam("idx")->value().toInt();
    if (idx >= PROFILE_COUNT) {
        request->send(400, "application/json",
            "{\"ok\":false,\"msg\":\"Gecersiz profil indexi\"}");
        return;
    }
    if (!_storage.clearProfileOverride(idx)) {
        request->send(400, "application/json",
            "{\"ok\":false,\"msg\":\"Override zaten yok\"}");
        return;
    }
    String msg = "{\"ok\":true,\"msg\":\"Fabrika ayarlarina donuldu: ";
    msg += String(ALL_PROFILES[idx]->name);
    msg += "\"}";
    request->send(200, "application/json", msg);

    // Aktif profil bu ise PhaseManager'i yeniden yukle (hazir profile don)
    if (_incubator.getPhaseManager().getProfileIndex() == idx) {
        _incubator.setProfile(idx);
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

    // Nem esikleri: formdan geliyorsa manuel override olarak uygula,
    // yoksa mevcut profil degerlerini kaydet (sadece referans icin).
    bool humChanged = false;
    if (request->hasParam("humLow", true) && request->hasParam("humHigh", true)) {
        float newLow  = request->getParam("humLow",  true)->value().toFloat();
        float newHigh = request->getParam("humHigh", true)->value().toFloat();
        // Profil degerlerinden farkli mi? (kullanici gercekten degistirmis mi?)
        if (fabsf(newLow  - st.targetHumLow)  > 0.5f ||
            fabsf(newHigh - st.targetHumHigh) > 0.5f) {
            _incubator.setHumidityThresholds(newLow, newHigh);  // Manuel override
            s.humLow  = _incubator.getHumidityLow();   // Validasyondan gecmis degeri kaydet
            s.humHigh = _incubator.getHumidityHigh();
            humChanged = true;
        }
    }
    if (!humChanged) {
        s.humLow  = st.targetHumLow;
        s.humHigh = st.targetHumHigh;
    }

    if (request->hasParam("eggCount", true)) {
        s.eggCount = (uint16_t)request->getParam("eggCount", true)->value().toInt();
    } else {
        s.eggCount = _storage.getEggCount();
    }
    s.profileIndex = 0;
    s.isCustomProfile = false;
    strncpy(s.wifiSSID, WIFI_SSID, 32);
    strncpy(s.wifiPass, WIFI_PASSWORD, 64);
    _storage.saveSettings(s);
    String msg = humChanged
        ? "{\"ok\":true,\"msg\":\"Ayarlar kaydedildi (nem manuel override aktif)\"}"
        : "{\"ok\":true,\"msg\":\"Ayarlar ESP hafizasina kaydedildi\"}";
    request->send(200, "application/json", msg);
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
    json += ",\"eggCount\":" + String(s.eggCount);
    json += ",\"ssid\":\"" + String(s.wifiSSID) + "\"";
    json += ",\"yumurtaIP\":\"" + String(s.yumurtaIP) + "\"";
    json += ",\"yumurtaEnabled\":" + String(s.yumurtaEnabled ? "true" : "false");
    json += "}";
    request->send(200, "application/json", json);
}

// ==================== TEMIZLIK / BAKIM MODU ====================
// POST /api/cleaning
//   action=start    -> Temizlik modunu baslat (guvenli varsayilanlarla)
//   action=stop     -> Temizlik modunu bitir (onceki state'e don)
//   action=set      -> Manuel cikislari guncelle (heater, fan, hum, turner)
//     heater: 0-255, fan: 0-255, hum: 0|1, turner: 0|1
void WebService::handlePostCleaning(AsyncWebServerRequest *request) {
    String action;
    if (request->hasParam("action", true)) {
        action = request->getParam("action", true)->value();
    }
    if (action == "start") {
        bool ok = _incubator.startCleaning();
        String resp = String("{\"ok\":") + (ok ? "true" : "false") +
                      ",\"msg\":\"" + (ok ? "Temizlik modu baslatildi" :
                                      "Baslatilamadi (EMERGENCY veya INIT durumu)") + "\"}";
        request->send(ok ? 200 : 409, "application/json", resp);
        return;
    }
    if (action == "stop") {
        _incubator.stopCleaning();
        request->send(200, "application/json",
            "{\"ok\":true,\"msg\":\"Temizlik bitti\"}");
        return;
    }
    if (action == "set") {
        if (!_incubator.isCleaning()) {
            request->send(409, "application/json",
                "{\"ok\":false,\"msg\":\"Temizlik modu aktif degil\"}");
            return;
        }
        uint8_t h   = _incubator.getCleaningHeater();
        uint8_t f   = _incubator.getCleaningFan();
        bool    hum = _incubator.getCleaningHumidifier();
        bool    trn = _incubator.getCleaningTurner();
        if (request->hasParam("heater", true))
            h = (uint8_t)constrain(request->getParam("heater", true)->value().toInt(), 0, 255);
        if (request->hasParam("fan", true))
            f = (uint8_t)constrain(request->getParam("fan", true)->value().toInt(), 0, 255);
        if (request->hasParam("hum", true)) {
            String v = request->getParam("hum", true)->value();
            hum = (v == "1" || v == "true" || v == "on");
        }
        if (request->hasParam("turner", true)) {
            String v = request->getParam("turner", true)->value();
            trn = (v == "1" || v == "true" || v == "on");
        }
        _incubator.setCleaningOutputs(h, f, hum, trn);
        char buf[160];
        snprintf(buf, sizeof(buf),
            "{\"ok\":true,\"msg\":\"Ayarlandi\","
            "\"heater\":%u,\"fan\":%u,\"hum\":%s,\"turner\":%s}",
            h, f, hum ? "true" : "false", trn ? "true" : "false");
        request->send(200, "application/json", buf);
        return;
    }
    request->send(400, "application/json",
        "{\"ok\":false,\"msg\":\"action: start | stop | set\"}");
}

// ==================== YUMURTA IR IP ====================

void WebService::handlePostEggSensorIP(AsyncWebServerRequest *request) {
    StoredSettings s = _storage.loadSettings();
    bool changed = false;

    // IP (opsiyonel) — sadece toggle icin kullanildiginda atlanir
    if (request->hasParam("ip", true)) {
        String ip = request->getParam("ip", true)->value();
        ip.trim();
        _incubator.getEggTempService().setIP(ip);
        strncpy(s.yumurtaIP, ip.c_str(), 39);
        s.yumurtaIP[39] = '\0';
        changed = true;
    }

    // enabled (opsiyonel) — "1"/"true" → aktif
    if (request->hasParam("enabled", true)) {
        String e = request->getParam("enabled", true)->value();
        bool en = (e == "1" || e == "true" || e == "on");
        _incubator.getEggTempService().setEnabled(en);
        s.yumurtaEnabled = en;
        changed = true;
    }

    if (!changed) {
        request->send(400, "application/json", "{\"ok\":false,\"msg\":\"ip veya enabled parametresi gerekli\"}");
        return;
    }

    _storage.saveSettings(s);
    String resp = "{\"ok\":true,\"msg\":\"Kaydedildi\",\"ip\":\"" + String(s.yumurtaIP) +
                  "\",\"enabled\":" + (s.yumurtaEnabled ? "true" : "false") + "}";
    request->send(200, "application/json", resp);
    Serial.printf("[WebService] Yumurta ayar guncellendi: ip=%s en=%d\n",
                  s.yumurtaIP, (int)s.yumurtaEnabled);
}

void WebService::handleGetEggSensorIP(AsyncWebServerRequest *request) {
    EggTempService& svc = _incubator.getEggTempService();
    String ip    = svc.getIP();
    bool   valid = svc.isValid();
    bool   en    = svc.isEnabled();
    String json = "{\"ip\":\"" + ip + "\",\"valid\":" + (valid ? "true" : "false") +
                  ",\"enabled\":" + (en ? "true" : "false") + "}";
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

// ==================== SENSOR KALIBRASYON ====================

void WebService::handleGetCalibration(AsyncWebServerRequest *request) {
    SensorCalibration cal = _incubator.getSensorCalibration();
    char buf[160];
    snprintf(buf, sizeof(buf),
        "{\"tempOffset1\":%.2f,\"humOffset1\":%.2f,\"tempOffset2\":%.2f,\"humOffset2\":%.2f}",
        cal.tempOffset1, cal.humOffset1, cal.tempOffset2, cal.humOffset2);
    request->send(200, "application/json", buf);
}

void WebService::handlePostCalibration(AsyncWebServerRequest *request) {
    SensorCalibration cal = _incubator.getSensorCalibration();

    if (request->hasParam("tempOffset1", true))
        cal.tempOffset1 = request->getParam("tempOffset1", true)->value().toFloat();
    if (request->hasParam("humOffset1", true))
        cal.humOffset1 = request->getParam("humOffset1", true)->value().toFloat();
    if (request->hasParam("tempOffset2", true))
        cal.tempOffset2 = request->getParam("tempOffset2", true)->value().toFloat();
    if (request->hasParam("humOffset2", true))
        cal.humOffset2 = request->getParam("humOffset2", true)->value().toFloat();

    // Aralik kontrolu (max +-5°C sicaklik, +-10% nem)
    if (fabs(cal.tempOffset1) > 5.0f || fabs(cal.humOffset1) > 10.0f ||
        fabs(cal.tempOffset2) > 5.0f || fabs(cal.humOffset2) > 10.0f) {
        request->send(400, "application/json",
            "{\"ok\":false,\"msg\":\"Offset aralik disi (max T:+-5C, H:+-10%)\"}");
        return;
    }

    _incubator.setSensorCalibration(cal);
    request->send(200, "application/json",
        "{\"ok\":true,\"msg\":\"Kalibrasyon kaydedildi ve uygulandi\"}");
}

// ==================== CIHAZ KIMLIGI ====================

void WebService::handleGetIdentity(AsyncWebServerRequest *request) {
    request->send(200, "application/json", DeviceIdentity::getIdentityJSON());
}

void WebService::handlePostDeviceName(AsyncWebServerRequest *request) {
    if (!request->hasParam("name", true)) {
        request->send(400, "application/json",
            "{\"ok\":false,\"msg\":\"name parametresi eksik\"}");
        return;
    }
    String name = request->getParam("name", true)->value();
    if (!DeviceIdentity::setDeviceName(name)) {
        request->send(400, "application/json",
            "{\"ok\":false,\"msg\":\"Gecersiz isim (maks 31 char; harf/rakam/bosluk/-/_)\"}");
        return;
    }
    String json = "{\"ok\":true,\"deviceName\":\"" + DeviceIdentity::getDeviceName() + "\"}";
    request->send(200, "application/json", json);
}

// ==================== CAPTIVE PORTAL ====================

void WebService::handleGetPortal(AsyncWebServerRequest *request) {
    request->send(200, "text/html", PAGE_PORTAL_HTML);
}

// OS'un internet algilama URL'leri buraya dusunce 302 ile portal'a yonlendir
// → "Sign in to network" / "WiFi Login" dialogu otomatik acilir
void WebService::handleCaptiveTrigger(AsyncWebServerRequest *request) {
    String location = "http://" + WiFi.softAPIP().toString() + String(CAPTIVE_PORTAL_URL);
    AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "");
    response->addHeader("Location", location);
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    request->send(response);
}

void WebService::handleGetWifiScan(AsyncWebServerRequest *request) {
    bool refresh = request->hasParam("refresh");
    int n = WiFi.scanComplete();

    if (refresh && n != WIFI_SCAN_RUNNING) {
        WiFi.scanDelete();
        WiFi.scanNetworks(true);  // async tarama tetikle
        request->send(202, "application/json", "{\"status\":\"started\"}");
        return;
    }

    if (n == WIFI_SCAN_RUNNING) {
        request->send(202, "application/json", "{\"status\":\"scanning\"}");
        return;
    }
    if (n == WIFI_SCAN_FAILED || n < 0) {
        // Henuz hic tarama yapilmamis — baslat
        WiFi.scanNetworks(true);
        request->send(202, "application/json", "{\"status\":\"started\"}");
        return;
    }

    // Sonuclari JSON olarak dondur
    String json = "{\"status\":\"done\",\"count\":" + String(n) + ",\"networks\":[";
    for (int i = 0; i < n; i++) {
        if (i > 0) json += ",";
        String ssid = WiFi.SSID(i);
        // Tirnak/backslash escape
        ssid.replace("\\", "\\\\");
        ssid.replace("\"", "\\\"");
        json += "{\"ssid\":\"" + ssid + "\"";
        json += ",\"rssi\":" + String(WiFi.RSSI(i));
        json += ",\"sec\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? 0 : 1);
        json += ",\"ch\":" + String(WiFi.channel(i));
        json += "}";
    }
    json += "]}";
    request->send(200, "application/json", json);
}

void WebService::handleGetWifiStatus(AsyncWebServerRequest *request) {
    bool conn = (WiFi.status() == WL_CONNECTED);
    String json = "{\"connected\":";
    json += (conn ? "true" : "false");
    json += ",\"status\":" + String((int)WiFi.status());
    if (conn) {
        json += ",\"ssid\":\"" + WiFi.SSID() + "\"";
        json += ",\"ip\":\""   + WiFi.localIP().toString() + "\"";
        json += ",\"rssi\":"   + String(WiFi.RSSI());
    }
    json += ",\"captive\":" + String(_captiveActive ? "true" : "false");
    json += "}";
    request->send(200, "application/json", json);
}

void WebService::handlePostWifiConnect(AsyncWebServerRequest *request) {
    if (!request->hasParam("ssid", true)) {
        request->send(400, "application/json",
            "{\"ok\":false,\"msg\":\"ssid parametresi eksik\"}");
        return;
    }
    String ssid = request->getParam("ssid", true)->value();
    String pass = request->hasParam("pass", true) ? request->getParam("pass", true)->value() : "";

    if (ssid.length() == 0 || ssid.length() > 32) {
        request->send(400, "application/json",
            "{\"ok\":false,\"msg\":\"SSID gecersiz (1-32 karakter)\"}");
        return;
    }

    // NVS'e kaydet — guc kesintisi sonrasi otomatik baglanma icin
    _storage.saveWiFi(ssid.c_str(), pass.c_str());

    // Hemen baglanmayi dene (reboot gerek yok)
    bool started = _tryConnectAsync(ssid, pass);

    String json = "{\"ok\":";
    json += (started ? "true" : "false");
    json += ",\"msg\":\"";
    json += (started ? "Baglanti deneniyor — /api/wifi/status ile takip edin"
                     : "WiFi.begin() basarisiz");
    json += "\",\"ssid\":\"" + ssid + "\"}";
    request->send(200, "application/json", json);
}

// ==================== INTERNET UZERINDEN OTA (PULL) ====================
//
// Endpoint'ler sadece istegi tetikler — gercek isi loop()'taki _otaPull.update()
// blocking olarak yurutur. Bu sayede AsyncWebServer'in is parcaciklarinin uzun
// HTTPS indirmesi tarafindan bloklanmasi engellenir.
// ============================================================================

void WebService::handleGetOtaOnlinePage(AsyncWebServerRequest *request) {
    request->send(200, "text/html", PAGE_OTA_ONLINE_HTML);
}

void WebService::handleGetOtaStatus(AsyncWebServerRequest *request) {
    request->send(200, "application/json", _otaPull.getStatusJSON());
}

void WebService::handlePostOtaCheck(AsyncWebServerRequest *request) {
    if (!isConnected()) {
        request->send(400, "application/json",
            "{\"ok\":false,\"msg\":\"Internet baglantisi yok — once /portal'dan WiFi'ye baglanin\"}");
        return;
    }
    if (_otaPull.getUpdateUrl().length() == 0) {
        request->send(400, "application/json",
            "{\"ok\":false,\"msg\":\"version.json URL'i ayarli degil — /api/ota/url ile kaydedin\"}");
        return;
    }
    _otaPull.requestCheck();
    request->send(200, "application/json",
        "{\"ok\":true,\"msg\":\"Kontrol tetiklendi — /api/ota/status ile takip edin\"}");
}

void WebService::handlePostOtaPull(AsyncWebServerRequest *request) {
    if (!isConnected()) {
        request->send(400, "application/json",
            "{\"ok\":false,\"msg\":\"Internet baglantisi yok\"}");
        return;
    }
    OTAUpdater::State st = _otaPull.getState();
    if (st != OTAUpdater::STATE_UPDATE_AVAILABLE) {
        request->send(400, "application/json",
            "{\"ok\":false,\"msg\":\"Once 'Kontrol Et' ile yeni surum bulunmali\"}");
        return;
    }
    _otaPull.requestPull();
    request->send(200, "application/json",
        "{\"ok\":true,\"msg\":\"Indirme tetiklendi — cihaz birkac dakika icinde yeniden baslatilacak\"}");
}

void WebService::handlePostOtaUrl(AsyncWebServerRequest *request) {
    if (!request->hasParam("url", true)) {
        request->send(400, "application/json",
            "{\"ok\":false,\"msg\":\"url parametresi eksik\"}");
        return;
    }
    String url = request->getParam("url", true)->value();
    if (!_otaPull.setUpdateUrl(url)) {
        request->send(400, "application/json",
            "{\"ok\":false,\"msg\":\"Gecersiz URL (http/https zorunlu)\"}");
        return;
    }
    String json = "{\"ok\":true,\"updateUrl\":\"" + _otaPull.getUpdateUrl() + "\"}";
    request->send(200, "application/json", json);
}
