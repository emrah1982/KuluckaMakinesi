/*
 * ============================================================
 *  ESP32 TABANLI AKILLI KULUCKA KONTROL SISTEMI v1.0
 * ============================================================
 *  Donanim: ESP32 DevKit
 *  I2C Bus: Grove - 8 Channel I2C Multiplexer (TCA9548A, 0x70)
 *    CH0 -> DS3231 RTC (0x68)
 *    CH1 -> SHT40 (0x44) - Birincil sicaklik/nem sensoru
 *    CH2 -> SHT30 (0x44) - Ikincil sicaklik/nem sensoru (fuzyon)
 *    CH3 -> SCD30 (0x61) - CO2 sensoru
 *    CH4 -> MLX90614ESF-BCC (0x5A) - Yumurta IR sicaklik (yerel, birincil)
 *    CH7 -> PCF8574 (0x20) -> 4'lu Role (Isitici, Nemlendirici, Motor)
 *  Depolama: MicroSD kart (GPIO5 CS)
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
 *  NOT: PCF8574 ve Grove 8 kanal I2C MUX icin harici kutuphane gerekmez (Wire.h yeterli).
 */

#include <esp_task_wdt.h>
 #include <esp_system.h>
#include "Config.h"
#include "DeviceIdentity.h"
#include "IncubationService.h"
#include "StorageService.h"
#include "WebService.h"

IncubationService incubator;
StorageService storage;
WebService webService(incubator, storage);

unsigned long lastLogTime = 0;

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(200);

    Serial.print("[BOOT] reset_reason=");
    Serial.println((int)esp_reset_reason());

    // Cihaz kimligi (deviceId, isim, FW versiyonu) — diger servislerden once baslat
    DeviceIdentity::begin();

    // Depolama servisi baslat
    storage.begin();

    storage.beginSD();
    storage.setSDLoggingEnabled(storage.isSDReady());

    // Override sistemi: incubator profil setlerken storage'i sorgulasin
    // (kullanici hazir profili duzenlediyse override kullanilir)
    incubator.setCustomProfileStorage(&storage);

    // Kayitli ayarlari yukle
    StoredSettings saved = storage.loadSettings();
    incubator.setPIDParams(saved.kp, saved.ki, saved.kd);
    // NOT: setHumidityThresholds() KULLANILMIYOR.
    // Nem eslikleri her zaman secili hayvan profilinin aktif fazindan
    // dinamik olarak gelir (setProfile -> updatePhase -> humCtrl.setThresholds).
    // Sabit NVS degeri uygulamak profil bazli dinamizmi bozar.
    if (!saved.isCustomProfile && saved.profileIndex < PROFILE_COUNT) {
        incubator.setProfile(saved.profileIndex);
    }

    // Kulucka sistemi baslat (watchdog'dan ONCE - setup ne kadar sürerse sürsün)
    // NOT: incubator.begin() icinde EggTempService.begin() cagrilir ve mutex olusturulur.
    // Bu nedenle setIP/setEnabled cagrilarinin begin()'den SONRA olmasi sart.
    incubator.begin();

    // Kayitli Yumurta IR sensor ayarlarini uygula (begin() SONRASI - mutex hazir)
    if (strlen(saved.yumurtaIP) > 0) {
        incubator.getEggTempService().setIP(String(saved.yumurtaIP));
    }
    incubator.getEggTempService().setEnabled(saved.yumurtaEnabled);

    // Web sunucu baslat
    webService.begin();

    // Watchdog'u tüm begin() bittikten SONRA baslat
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = WATCHDOG_TIMEOUT * 1000,
        .idle_core_mask = 0,   // Sadece manuel eklenen taskları izle (idle core yok)
        .trigger_panic = true
    };
    esp_task_wdt_init(&wdt_config);
    esp_task_wdt_add(NULL);
    Serial.println("[BOOT] Watchdog baslatildi");
}

void loop() {
    // Watchdog besle
    esp_task_wdt_reset();

    // Web servisi: captive portal DNS islem + STA durum izleme
    webService.update();

    // Ana guncelleme dongusu
    incubator.update();

    storage.updateSD();

    // Periyodik veri loglama
    if (millis() - lastLogTime >= LOG_INTERVAL) {
        lastLogTime = millis();
        SystemStatus st = incubator.getStatus();
        storage.logData(st.temperature, st.humidity, st.heaterPWM, st.fanPWM, st.phaseName);
        storage.appendSDLog(incubator.getUnixTime(), st.profileName, st.temperature, st.humidity);
    }

}
