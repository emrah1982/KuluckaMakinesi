#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================
// ESP32 TABANLI AKILLI KULUÇKA KONTROL SİSTEMİ - KONFIGÜRASYON
// ============================================================

// -------------------- WiFi (STA - Mevcut ağa bağlanma) --------------------
#define WIFI_SSID           "WIFI_ADI"
#define WIFI_PASSWORD       "WIFI_SIFRE"
#define WIFI_CONNECT_TIMEOUT 10000  // ms
#define WIFI_RETRY_INTERVAL  30000  // ms

// -------------------- WiFi (AP - Hotspot modu) --------------------
#define AP_SSID             "KuluckaMakinesi"
#define AP_PASSWORD         "kulucka123"   // Min 8 karakter, bos = acik ag
#define AP_CHANNEL          1
#define AP_MAX_CONNECTIONS  4
#define AP_IP               192,168,4,1
#define AP_GATEWAY          192,168,4,1
#define AP_SUBNET           255,255,255,0

// -------------------- I2C + TCA9548A Multiplexer --------------------
#define I2C_SDA_PIN         21
#define I2C_SCL_PIN         22
#define I2C_CLOCK_SPEED     100000  // 100kHz
#define MUX_ADDR            0x70    // TCA9548A varsayilan adres (A0=A1=A2=GND)
#define MUX_CH_RTC          0       // DS3231 RTC -> Kanal 0
#define MUX_CH_RELAY        1       // PCF8574 -> Kanal 1 (4'lu role modulu)
// Kanal 2,3 bos - gelecekte eklenecek cihazlar icin

// -------------------- PCF8574 I2C GPIO Expander (4'lu Role) --------------------
#define PCF8574_ADDR        0x20    // PCF8574 varsayilan adres (A0=A1=A2=GND)
// PCF8574 pin atamalari (Active LOW - LOW=Role ACIK, HIGH=Role KAPALI)
#define RELAY_BIT_HEATER    0       // P0 - Isitici rolesi
#define RELAY_BIT_HUMIDIFIER 1      // P1 - Nemlendirici rolesi
#define RELAY_BIT_BUZZER    2       // P2 - Alarm buzzer rolesi
#define RELAY_BIT_LED       3       // P3 - Durum LED rolesi
#define RELAY_ALL_OFF       0xFF    // Tum roleler kapali (Active LOW)

// -------------------- Isitici Time-Proportional Kontrol --------------------
#define HEATER_WINDOW_MS    10000   // ms - role pencere suresi (10 sn)

// -------------------- RS485 / Modbus RTU (SHT20 Sensor) --------------------
#define RS485_RX_PIN        16      // MAX485 RO -> ESP32 RX2
#define RS485_TX_PIN        17      // MAX485 DI -> ESP32 TX2
#define RS485_DE_RE_PIN     4       // MAX485 DE+RE (birlesik) -> ESP32 GPIO4
#define MODBUS_BAUD         9600    // SHT20 varsayilan baud
#define MODBUS_SLAVE_ADDR   0x01    // SHT20 varsayilan adres
#define MODBUS_TIMEOUT      500     // ms - yanitlama zamanlama
#define MODBUS_REG_TEMP     0x0001  // Sicaklik register adresi
#define MODBUS_REG_HUM      0x0002  // Nem register adresi

// -------------------- DS3231 RTC --------------------
// DS3231 varsayılan I2C adresi: 0x68

// -------------------- TFT Ekran (ST7789 - Kart Uzerinde Sabit) --------------------
// Pin tanimlari TFT_eSPI kutuphanesinin User_Setup.h dosyasinda yapilir.
// Proje referansi: src/config/TFT_Setup.h
// ESP32-3248S032: CS=15, DC=2, RST=-1, BL=27, MOSI=13, SCLK=14, MISO=12
#ifndef TFT_BL
#define TFT_BL              27      // Arka isik (DisplayManager icin)
#endif

// -------------------- Dokunmatik Ekran (XPT2046) --------------------
#ifndef TOUCH_CS
#define TOUCH_CS            33
#endif
#define TOUCH_IRQ           36

// Dokunmatik kalibrasyon (ESP32-3248S032 icin varsayilan)
// Farkli kart ise setTouch(calData) ile ayarlayin
#define TOUCH_CAL_X_MIN     300
#define TOUCH_CAL_X_MAX     3600
#define TOUCH_CAL_Y_MIN     300
#define TOUCH_CAL_Y_MAX     4000    // 3600->4000: Y ekseni ust kisim kalibrasyon telafisi
                                    // Yanlis satir seciliyorsa: artirin(+) ustte asagi kayar
#define TOUCH_DEBOUNCE_MS   250     // ms - dokunma arasi bekleme

// -------------------- Ekran Guncelleme --------------------
#define DISPLAY_UPDATE_MS   500     // ms - ekran yenileme araligi

// -------------------- L298N Motor Surucu (Yumurta Cevirme) --------------------
// -------------------- L298N Motor Surucu (Yumurta Cevirme) --------------------
#define TURNER_ENA_PIN      25      // PWM - Motor hiz kontrolu
#define TURNER_IN1_PIN      23      // Motor yon 1
#define TURNER_IN2_PIN      19      // Motor yon 2
#define TURNER_PWM_SPEED    200     // Cevirme hizi (0-255)

// -------------------- Yumurta Cevirme Zamanlama --------------------
#define TURNER_INTERVAL_MS  3600000 // ms - cevirme araligi (1 saat)
#define TURNER_DURATION_MS  5000    // ms - cevirme suresi (5 sn)
#define TURNER_PAUSE_MS     1000    // ms - yon degistirme arasi bekleme

// -------------------- Fan (PWM) --------------------
// -------------------- Fan (PWM) --------------------
#define FAN_PIN             18      // Fan PWM pini (bos pinlerden)

// -------------------- PWM Parametreleri --------------------
#define PWM_FREQ_FAN        25000   // 25 kHz (fan icin)
#define PWM_FREQ_TURNER     1000    // 1 kHz (cevirme motoru icin)
#define PWM_RESOLUTION      8       // 8-bit (0-255)

// -------------------- PID Varsayılan Değerler --------------------
#define PID_DEFAULT_KP      20.0
#define PID_DEFAULT_KI      0.8
#define PID_DEFAULT_KD      5.0
#define PID_INTEGRAL_MIN    -100.0
#define PID_INTEGRAL_MAX    100.0
#define PID_OUTPUT_MIN      0
#define PID_OUTPUT_MAX      255

// -------------------- Auto PID Tuning --------------------
#define AUTOTUNE_CYCLES     6       // Salınım sayısı
#define AUTOTUNE_OUTPUT     200     // Tuning sırasında PWM
#define AUTOTUNE_HYSTERESIS 0.2     // °C

// -------------------- Sensör Filtresi --------------------
#define EMA_ALPHA           0.2f    // Exponential Moving Average katsayısı
#define SPIKE_THRESHOLD     2.0f    // °C - ani sıçrama eşiği
#define SENSOR_READ_INTERVAL 2000   // ms

// -------------------- Nem Kontrolü --------------------
#define HUMIDIFIER_OFF_DELAY 10000  // ms - gecikmeli kapatma
#define HUMIDIFIER_MIN_ON    5000   // ms - minimum çalışma süresi

// -------------------- Fan Kontrolü --------------------
#define FAN_MIN_PWM         80      // Minimum fan hızı (asla durmaz)
#define FAN_MAX_PWM         255     // Maksimum fan hızı
#define FAN_TEMP_LOW        36.0f   // Fan haritalaması alt sıcaklık
#define FAN_TEMP_HIGH       38.0f   // Fan haritalaması üst sıcaklık

// -------------------- Alarm Eşikleri --------------------
#define ALARM_THRESHOLD_TEMP_HIGH  38.5f   // °C
#define ALARM_THRESHOLD_TEMP_LOW   36.0f   // °C
#define ALARM_THRESHOLD_HUM_HIGH   85.0f   // %
#define ALARM_THRESHOLD_HUM_LOW    30.0f   // %
#define ALARM_COOLDOWN      60000   // ms - tekrar alarm arası bekleme

// -------------------- Güvenlik --------------------
#define SAFETY_TEMP_MAX     40.0f   // °C - acil kapatma sıcaklığı
#define SAFETY_TEMP_MIN     20.0f   // °C - sensör hatası şüphesi
#define SAFETY_SENSOR_FAIL_COUNT 5  // Ardışık hata sayısı

// -------------------- Sistem --------------------
#define LOOP_INTERVAL       2000    // ms - ana döngü aralığı
#define SERIAL_BAUD         115200
#define WATCHDOG_TIMEOUT    8       // saniye

// -------------------- Web Server --------------------
#define WEB_SERVER_PORT     80
#define API_PREFIX          "/api"

// -------------------- OTA --------------------
#define OTA_HOSTNAME        "KuluckaMakinesi"
#define OTA_PASSWORD        ""      // Boş = şifresiz

// -------------------- Veri Loglama --------------------
#define LOG_INTERVAL        60000   // ms - log kayit araligi (1 dk)

#endif // CONFIG_H
