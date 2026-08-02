#include "EggTempService.h"
#include "Config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>

#ifndef YUMURTA_FETCH_INTERVAL_MS
#define YUMURTA_FETCH_INTERVAL_MS 10000
#endif

// =====================================================================
//  Constructor
// =====================================================================
EggTempService::EggTempService()
    : _ip(""), _eggTemp(0.0f), _ambientTemp(0.0f), _valid(false),
      _enabled(true),
      _discoveryStatus(DISC_NONE), _discoveryTaskHandle(nullptr),
      _mutex(nullptr), _taskHandle(nullptr) {}

// =====================================================================
//  begin() — FreeRTOS task başlat
// =====================================================================
void EggTempService::begin() {
    _mutex = xSemaphoreCreateMutex();
    xTaskCreate(fetchTask, "EggFetch", 8192, this, 1, &_taskHandle);
    Serial.println("[EggTemp] Servis baslatildi");
}

// =====================================================================
//  Setter / Getter'lar — thread-safe (mutex ile korunur)
// =====================================================================
void EggTempService::setIP(const String& ip) {
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    _ip    = ip;
    _valid = false;
    if (_mutex) xSemaphoreGive(_mutex);
    Serial.printf("[EggTemp] IP guncellendi: %s\n", ip.c_str());
}

String EggTempService::getIP() const {
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    String ip = _ip;
    if (_mutex) xSemaphoreGive(_mutex);
    return ip;
}

bool EggTempService::isConfigured() const {
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    bool v = (_ip.length() > 0);
    if (_mutex) xSemaphoreGive(_mutex);
    return v;
}

void EggTempService::setEnabled(bool en) {
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    _enabled = en;
    if (!en) _valid = false;   // pasifte UI "Devre Disi" gostersin
    if (_mutex) xSemaphoreGive(_mutex);
    Serial.printf("[EggTemp] Servis %s\n", en ? "AKTIF" : "PASIF");
}

bool EggTempService::isEnabled() const {
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    bool v = _enabled;
    if (_mutex) xSemaphoreGive(_mutex);
    return v;
}

float EggTempService::getEggTemp() const {
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    float v = _eggTemp;
    if (_mutex) xSemaphoreGive(_mutex);
    return v;
}

float EggTempService::getAmbientTemp() const {
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    float v = _ambientTemp;
    if (_mutex) xSemaphoreGive(_mutex);
    return v;
}

bool EggTempService::isValid() const {
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    bool v = _valid;
    if (_mutex) xSemaphoreGive(_mutex);
    return v;
}

// =====================================================================
//  Otomatik IP kesfi (mDNS) — asenkron
// =====================================================================
EggTempService::DiscoveryStatus EggTempService::getDiscoveryStatus() const {
    return (DiscoveryStatus)_discoveryStatus;
}

void EggTempService::requestDiscovery() {
    // Halen calisan bir kesif varsa yeni baslatma
    if (_discoveryStatus == DISC_RUNNING) {
        Serial.println("[EggTemp] Kesif zaten calisiyor");
        return;
    }
    _discoveryStatus = DISC_RUNNING;
    // Kisa omurlu task: mDNS sorgusu yap, sonuc kaydet, kendini sil
    BaseType_t r = xTaskCreate(discoveryTask, "EggDisc", 4096,
                               this, 1, &_discoveryTaskHandle);
    if (r != pdPASS) {
        _discoveryStatus = DISC_FAILED;
        Serial.println("[EggTemp] Kesif task'i olusturulamadi");
    }
}

void EggTempService::discoveryTask(void* pvParameters) {
    EggTempService* svc = static_cast<EggTempService*>(pvParameters);
    Serial.println("[EggTemp] mDNS kesfi basliyor: yumurta.local ...");

    // WiFi STA aktif degilse direkt basarisiz
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[EggTemp] WiFi STA bagli degil, kesif atlandi");
        svc->_discoveryStatus = DISC_FAILED;
        svc->_discoveryTaskHandle = nullptr;
        vTaskDelete(NULL);
        return;
    }

    IPAddress ip = MDNS.queryHost("yumurta", 3000);
    if (ip == IPAddress(0, 0, 0, 0)) {
        Serial.println("[EggTemp] yumurta.local bulunamadi");
        svc->_discoveryStatus = DISC_FAILED;
    } else {
        String ipStr = ip.toString();
        Serial.printf("[EggTemp] yumurta.local -> %s\n", ipStr.c_str());
        svc->setIP(ipStr);
        svc->_discoveryStatus = DISC_OK;
    }
    svc->_discoveryTaskHandle = nullptr;
    vTaskDelete(NULL);
}

// =====================================================================
//  JSON yardımcısı — "key": değeri yakala
// =====================================================================
float EggTempService::parseJsonFloat(const String& json, const char* key) {
    String search = "\"";
    search += key;
    search += "\":";
    int idx = json.indexOf(search);
    if (idx < 0) return 0.0f;
    return json.substring(idx + search.length()).toFloat();
}

// =====================================================================
//  FreeRTOS task — arka planda periyodik HTTP fetch
// =====================================================================
void EggTempService::fetchTask(void* pvParameters) {
    EggTempService* self = (EggTempService*)pvParameters;

    // WiFi'nin bağlanması için ilk fetch'i bekle
    vTaskDelay(pdMS_TO_TICKS(6000));

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(YUMURTA_FETCH_INTERVAL_MS));

        // IP, enabled ve WiFi kontrolü (mutex ile thread-safe)
        xSemaphoreTake(self->_mutex, portMAX_DELAY);
        String ip = self->_ip;
        bool   en = self->_enabled;
        xSemaphoreGive(self->_mutex);

        if (!en || ip.length() == 0 || WiFi.status() != WL_CONNECTED) {
            continue;
        }

        HTTPClient http;
        String url = "http://" + ip + "/api/egg";
        http.begin(url);
        http.setTimeout(2000);  // 2 sn timeout — ana loop bloklanmaz

        int code = http.GET();
        if (code == 200) {
            String body = http.getString();
            float et = parseJsonFloat(body, "eggTemp");
            float at = parseJsonFloat(body, "ambientTemp");
            bool  ok = (body.indexOf("\"valid\":true") >= 0) && (et > 5.0f);

            xSemaphoreTake(self->_mutex, portMAX_DELAY);
            if (ok) self->_eggTemp = et;
            self->_ambientTemp = at;
            self->_valid       = ok;
            xSemaphoreGive(self->_mutex);

            Serial.printf("[EggTemp] %.2fC (ortam %.1fC)\n", et, at);
        } else {
            xSemaphoreTake(self->_mutex, portMAX_DELAY);
            self->_valid = false;
            xSemaphoreGive(self->_mutex);
            Serial.printf("[EggTemp] HTTP hata: %d url=%s\n", code, url.c_str());
        }
        http.end();
    }
}
