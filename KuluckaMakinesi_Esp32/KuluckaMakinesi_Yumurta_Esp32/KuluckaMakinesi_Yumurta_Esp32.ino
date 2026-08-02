/*
 * KuluckaMakinesi_Yumurta_Esp32
 * ---------------------------------------------------------------
 * GY-906 (MLX90614) IR termometre ile yumurta yüzey sıcaklığını
 * okur ve WiFi üzerinden HTTP JSON endpoint'i olarak yayar.
 *
 * Kütüphaneler (Arduino IDE Library Manager):
 *   - Adafruit MLX90614 Library  (Adafruit)
 *   - Adafruit BusIO             (Adafruit, bağımlılık)
 *   - ESPmDNS                    (ESP32 core ile gelir)
 *
 * Endpoint:
 *   GET http://<IP>/api/egg
 *   GET http://yumurta.local/api/egg   (mDNS)
 *
 * Board: ESP32C3 Dev Module  (esp32 core >= 2.0)
 * ---------------------------------------------------------------
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Adafruit_MLX90614.h>
#include "Config.h"

// =====================================================================
//  Global nesneler
// =====================================================================
Adafruit_MLX90614 mlx(MLX90614_ADDR);
WebServer         server(HTTP_PORT);

// =====================================================================
//  Durum değişkenleri
// =====================================================================
struct EggTempData {
    float eggTemp;      // Yumurta yüzey sıcaklığı (°C)
    float ambientTemp;  // Ortam sıcaklığı (°C)
    bool  valid;        // Ölçüm geçerli mi
    unsigned long lastReadMs;
};

static EggTempData g_data = { 0.0f, 0.0f, false, 0 };

// Hareketli ortalama tamponu
static float  g_history[TEMP_HISTORY_SIZE] = {};
static uint8_t g_histIdx  = 0;
static uint8_t g_histFill = 0;   // Kaç slot dolu (max TEMP_HISTORY_SIZE)
static bool   g_sensorOK  = false;

// =====================================================================
//  WiFi bağlantısı
// =====================================================================
static void connectWiFi() {
    Serial.printf("[WiFi] SSID: %s\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - t0 > WIFI_TIMEOUT_MS) {
            Serial.println("[WiFi] HATA: Baglanti kurulamadi! Yeniden deniyor...");
            WiFi.disconnect();
            delay(1000);
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
            t0 = millis();
        }
        delay(200);
        Serial.print(".");
    }
    Serial.printf("\n[WiFi] Baglandi. IP: %s\n", WiFi.localIP().toString().c_str());
}

// =====================================================================
//  MLX90614 sıcaklık okuma
// =====================================================================
static float movingAverage(float newVal) {
    g_history[g_histIdx] = newVal;
    g_histIdx = (g_histIdx + 1) % TEMP_HISTORY_SIZE;
    if (g_histFill < TEMP_HISTORY_SIZE) g_histFill++;

    float sum = 0.0f;
    for (uint8_t i = 0; i < g_histFill; i++) sum += g_history[i];
    return sum / g_histFill;
}

static void readSensor() {
    float obj = mlx.readObjectTempC();
    float amb = mlx.readAmbientTempC();

    // NaN veya aralık dışı kontrol
    if (isnan(obj) || isnan(amb) ||
        obj < TEMP_VALID_MIN || obj > TEMP_VALID_MAX) {
        g_data.valid = false;
        Serial.println("[MLX] Gecersiz okuma!");
        return;
    }

    // Emissivity düzeltmesi (kalibrasyon ofset)
    obj += EGG_EMISSIVITY_CORRECTION;

    g_data.eggTemp     = movingAverage(obj);
    g_data.ambientTemp = amb;
    g_data.valid       = true;
    g_data.lastReadMs  = millis();

    Serial.printf("[MLX] Yumurta: %.2f C  Ortam: %.2f C\n",
                  g_data.eggTemp, g_data.ambientTemp);
}

// =====================================================================
//  HTTP Endpoint: GET /api/egg
//  Yanıt formatı:
//  {
//    "eggTemp"    : 37.45,
//    "ambientTemp": 25.10,
//    "valid"      : true,
//    "uptime"     : 123456,
//    "ip"         : "192.168.1.50"
//  }
// =====================================================================
static void handleGetEggTemp() {
    // CORS başlığı — başka bir projenin HTTP isteği yapabilmesi için
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Cache-Control", "no-cache");

    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"eggTemp\":%.2f,\"ambientTemp\":%.2f,"
        "\"valid\":%s,\"uptime\":%lu,\"ip\":\"%s\"}",
        g_data.eggTemp,
        g_data.ambientTemp,
        g_data.valid ? "true" : "false",
        millis() / 1000UL,
        WiFi.localIP().toString().c_str());

    server.send(200, "application/json", buf);
}

// =====================================================================
//  HTTP Endpoint: GET /api/status
//  Cihaz ve sensör durumu özeti
// =====================================================================
static void handleGetStatus() {
    server.sendHeader("Access-Control-Allow-Origin", "*");

    char buf[320];
    snprintf(buf, sizeof(buf),
        "{\"device\":\"KuluckaMakinesi_Yumurta\","
        "\"sensor\":\"MLX90614\","
        "\"sensorOK\":%s,"
        "\"wifiRSSI\":%d,"
        "\"ip\":\"%s\","
        "\"uptime\":%lu,"
        "\"readInterval\":%d}",
        g_sensorOK ? "true" : "false",
        WiFi.RSSI(),
        WiFi.localIP().toString().c_str(),
        millis() / 1000UL,
        TEMP_READ_INTERVAL_MS);

    server.send(200, "application/json", buf);
}

// =====================================================================
//  HTTP Endpoint: GET /
//  Ana sayfa — basit bilgi sayfası
// =====================================================================
static void handleRoot() {
    String html = F(
        "<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<title>Yumurta Termometre</title></head><body>"
        "<h2>Yumurta IR Termometre</h2>"
        "<p>Sensor: GY-906 (MLX90614)</p>"
        "<ul>"
        "<li><a href='/api/egg'>/api/egg</a> — JSON sicaklik verisi</li>"
        "<li><a href='/api/status'>/api/status</a> — Cihaz durumu</li>"
        "</ul></body></html>");
    server.send(200, "text/html", html);
}

// =====================================================================
//  Setup
// =====================================================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== KuluckaMakinesi_Yumurta_Esp32 ===");

    // Status LED
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, LOW);

    // I2C başlat (ESP32-C3 Mini özel pin ataması)
    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(100000);   // 100 kHz (MLX90614 SMBus uyumlu)

    // MLX90614 başlat
    if (!mlx.begin(&Wire)) {
        Serial.println("[MLX] HATA: Sensor bulunamadi! Baglantilari kontrol edin.");
        g_sensorOK = false;
        // Hata durumunda LED hızlı yanıp söner
        for (int i = 0; i < 10; i++) {
            digitalWrite(PIN_STATUS_LED, HIGH); delay(100);
            digitalWrite(PIN_STATUS_LED, LOW);  delay(100);
        }
    } else {
        g_sensorOK = true;
        Serial.println("[MLX] Sensor hazir.");
        Serial.printf("[MLX] Emissivity: %.3f\n", mlx.readEmissivity());
        digitalWrite(PIN_STATUS_LED, HIGH);
    }

    // WiFi bağlan
    connectWiFi();

    // mDNS başlat → http://yumurta.local
    if (MDNS.begin(MDNS_HOSTNAME)) {
        MDNS.addService("http", "tcp", HTTP_PORT);
        Serial.printf("[mDNS] http://%s.local/api/egg\n", MDNS_HOSTNAME);
    }

    // HTTP rotaları
    server.on("/",           HTTP_GET, handleRoot);
    server.on("/api/egg",    HTTP_GET, handleGetEggTemp);
    server.on("/api/status", HTTP_GET, handleGetStatus);
    server.onNotFound([]() {
        server.send(404, "application/json", "{\"error\":\"not found\"}");
    });

    server.begin();
    Serial.printf("[HTTP] Sunucu portu: %d\n", HTTP_PORT);
    Serial.printf("[HTTP] http://%s/api/egg\n", WiFi.localIP().toString().c_str());
}

// =====================================================================
//  Loop
// =====================================================================
void loop() {
    // WiFi bağlantısı kopmuşsa yeniden bağlan
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Baglanti kesildi, yeniden baglaniyor...");
        digitalWrite(PIN_STATUS_LED, LOW);
        connectWiFi();
        digitalWrite(PIN_STATUS_LED, HIGH);
    }

    // HTTP isteklerini işle
    server.handleClient();

    // Periyodik sıcaklık okuma
    static unsigned long lastRead = 0;
    if (millis() - lastRead >= TEMP_READ_INTERVAL_MS) {
        lastRead = millis();
        if (g_sensorOK) {
            readSensor();
        }
    }

}
