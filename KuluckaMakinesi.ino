/*
 * ============================================================
 *  ESP32 TABANLI AKILLI KULUCKA KONTROL SISTEMI v1.0
 * ============================================================
 *  Donanim: ESP32 DevKit
 *  I2C Bus: TCA9546A Multiplexer (0x70)
 *    CH0 -> DS3231 RTC (0x68)
 *    CH1 -> PCF8574 (0x20) -> 4'lu Role (Isitici, Nemlendirici, Buzzer, LED)
 *  Sensor: RS485 SHT20 (Modbus RTU, MAX485)
 *  Fan: L298N Motor Surucu (PWM hiz kontrolu)
 *  Kontrol: PID + Auto-Tuning (Ziegler-Nichols)
 *  Web: ESP32 AsyncWebServer + PROGMEM (SPIFFS gerektirmez)
 * ============================================================
 *
 *  Arduino IDE Kurulum:
 *  1. Dosya > Tercihler > Ek Kart Yoneticisi URL:
 *     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
 *  2. Araclar > Kart > ESP32 Dev Module
 *  3. Araclar > Kart > Partition Scheme > Huge APP (3MB No OTA/1MB SPIFFS)
 *  4. Kutuphaneler (Sketch > Include Library > Manage Libraries):
 *     - RTClib (Adafruit)
 *  5. Manuel kutuphane (GitHub ZIP):
 *     - ESPAsyncWebServer  (https://github.com/mathieucarbou/ESPAsyncWebServer)
 *     - AsyncTCP           (https://github.com/mathieucarbou/AsyncTCP)
 *  NOT: Web sayfalari PROGMEM olarak firmware icinde gomulu, SPIFFS yuklemesi gerekmez.
 *  NOT: SHT20 sensor RS485 Modbus RTU ile okunur, harici kutuphane gerekmez.
 *  NOT: PCF8574 ve TCA9546A icin harici kutuphane gerekmez (Wire.h yeterli).
 */

#include <esp_task_wdt.h>
#include "src/config/Config.h"
#include "src/service/IncubationService.h"
#include "src/service/StorageService.h"
#include "src/service/WebService.h"

IncubationService incubator;
StorageService storage;
WebService webService(incubator, storage);

unsigned long lastLogTime = 0;

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(1000);

    // Watchdog baslat (ESP32 core v3+ uyumlu)
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = WATCHDOG_TIMEOUT * 1000,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
        .trigger_panic = true
    };
    esp_task_wdt_init(&wdt_config);
    esp_task_wdt_add(NULL);

    // Depolama servisi baslat
    storage.begin();

    // Kayitli ayarlari yukle
    StoredSettings saved = storage.loadSettings();
    incubator.setPIDParams(saved.kp, saved.ki, saved.kd);
    incubator.setHumidityThresholds(saved.humLow, saved.humHigh);
    if (!saved.isCustomProfile && saved.profileIndex < PROFILE_COUNT) {
        incubator.setProfile(saved.profileIndex);
    }

    // Kulucka sistemi baslat
    incubator.begin();

    // Web sunucu baslat
    webService.begin();

    Serial.println("============================================");
    if (webService.isAPActive()) {
        Serial.print("[SYSTEM] Telefon ile baglan: ");
        Serial.print(AP_SSID);
        Serial.print(" (Sifre: ");
        Serial.print(AP_PASSWORD);
        Serial.println(")");
        Serial.print("[SYSTEM] AP arayuz:   http://");
        Serial.println(webService.getAPIPAddress());
    }
    if (webService.isConnected()) {
        Serial.print("[SYSTEM] WiFi arayuz: http://");
        Serial.println(webService.getIPAddress());
    }
    Serial.println("============================================");
}

void loop() {
    // Watchdog besle
    esp_task_wdt_reset();

    // Ana guncelleme dongusu
    incubator.update();

    // Periyodik veri loglama
    if (millis() - lastLogTime >= LOG_INTERVAL) {
        lastLogTime = millis();
        SystemStatus st = incubator.getStatus();
        storage.logData(st.temperature, st.humidity, st.heaterPWM, st.fanPWM, st.phaseName);
    }
}
