#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================
// ESP32 TABANLI AKILLI KULUÇKA KONTROL SİSTEMİ - KONFIGÜRASYON
// ============================================================

// -------------------- FIRMWARE SURUMU --------------------
// OTA karsilastirmasi icin semantic versioning (MAJOR.MINOR.PATCH)
// Her yayinda FW_VERSION_PATCH/MINOR/MAJOR artirilir.
// v3.0.0 (2026-04-26): 3 yeni profil (Keklik/Tavus/Kugu), endustri kalibrasyonu,
//                       profil override (web+TFT), dinamik alarm, hoparlor.
#define FW_VERSION_MAJOR    3
#define FW_VERSION_MINOR    1
#define FW_VERSION_PATCH    0
#define FW_VERSION_STRING   "3.1.0"

// Otomatik olarak derleme sirasinda doldurulur
#define FW_BUILD_DATE       __DATE__
#define FW_BUILD_TIME       __TIME__

// -------------------- DONANIM KIMLIGI --------------------
#define HW_MODEL            "ESP32-WROOM-32E"
#define HW_BOARD            "KuluckaMakinesi v3.0.0"  // Kullaniciya gosterilen model ismi (TFT, web, OTA)

// Cihaz ismi / AP SSID prefixi (MAC ile birleserek "KM-A4C138F2" gibi olur)
#define DEVICE_ID_PREFIX    "KM"
#define DEVICE_NAME_MAX_LEN 31      // NVS'de saklanacak kullanici tanimli isim maks uzunlugu

// -------------------- DEBUG AYARLARI --------------------
// 1 = Seri monitorde periyodik [STATUS] satiri ve tum tani ciktisi gorunur
// 0 = DEBUG_* makrolari bosa cikar (flash tasarrufu, sahaya cikarken kullanin)
//
// NOT: Bu bayrak 0 iken periyodik sicaklik/nem verisi seri porta HIC basilmaz;
// sadece modullerin kendi Serial.print satirlari (MUX/RELAY/SENSOR/EGG-IR
// baslangic mesajlari ve hatalar) gorunur.
#define DEBUG_ENABLED       1

#if DEBUG_ENABLED
    #define DEBUG_PRINT(x)    Serial.print(x)
    #define DEBUG_PRINTLN(x)  Serial.println(x)
    #define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
    #define DEBUG_PRINTF(...)
#endif

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

// -------------------- I2C + Grove 8 Kanal I2C Multiplexer --------------------
// Modul: Grove - 8 Channel I2C Multiplexer / IIC Hub (TCA9548A tabanli)
// I2C BUS: IO25 = SCL, IO32 = SDA (ESP32 kart üzerindeki I2C Peripheral Interface konektörü)
#define I2C_SDA_PIN         32
#define I2C_SCL_PIN         25
#define I2C_CLOCK_SPEED     100000  // 100kHz
#define MUX_ADDR            0x70    // Grove 8 kanal MUX varsayılan adres (A0=A1=A2=GND)
#define MUX_CH_RTC          0       // DS3231 RTC         -> Kanal 0 (SD0/SC0)
#define MUX_CH_SHT40        1       // SHT40 sicaklik/nem -> Kanal 1 (SD1/SC1)  [Birincil]
#define MUX_CH_SHT30        2       // SHT30 sicaklik/nem -> Kanal 2 (SD2/SC2)  [Ikincil]
#define MUX_CH_CO2          3       // CO2 sensoru (SCD30)-> Kanal 3 (SD3/SC3)
#define MUX_CH_EGG_IR       4       // MLX90614 IR termo   -> Kanal 4 (SD4/SC4)
#define MUX_CH_RELAY        7       // PCF8574 4'lü röle  -> Kanal 7 (SD7/SC7)
#define MUX_CH_SENSOR       MUX_CH_SHT40   // Birincil sensor kanali  (SHT40)
#define MUX_CH_SENSOR_2     MUX_CH_SHT30   // Ikincil sensor kanali   (SHT30)
// Kanal 5, 6 boş - gelecekte eklenecek cihazlar için ayrılmıştır
// Projedeki tum I2C cihazlari: DS3231, SHT40, SHT30, SCD30, MLX90614, PCF8574

// -------------------- DS3231 RTC (Gerçek Zamanlı Saat) --------------------
// Bağlantı: DS3231 RTC kartı, Grove 8 kanal MUX'un CH0 (Kanal 0, SD0/SC0) çıkışına bağlıdır.
// Zaman bilgisi bu kanaldan okunur.
// DS3231 varsayılan I2C adresi: 0x68
// Donanım Notu: RTC modülü doğrudan I2C hattına veya MUX kanalına bağlanmalıdır.
// Kablo bağlantısı: SDA -> IO32, SCL -> IO25

// -------------------- SICAKLIK / NEM SENSÖRLERI --------------------
// Kanal 1: SHT40  (Birincil) — Kutuphane: Sensirion_I2C_SHT4x (SensirionI2CSht4x.h)
// Kanal 2: SHT30  (Ikincil)  — Kutuphane: Wire ile direkt I2C
// Fuzyon: iki sensorun ortalamasi alinir; fark toleransi asarsa uyari verilir.
#define SENSOR_NAME       "SHT40+SHT30"   // Sistemin genel sensor tanimi (baslik/log)
#define SENSOR_BUS_NAME   "I2C"
// Sensor yuvalarinin TEKIL adlari. Ekranda iki yuva ayri ayri gosterildiginde
// hangisinin hangi sensor oldugu belli olmali; eskiden yuva 1 "SHT40+SHT30",
// yuva 2 "I2C" olarak etiketleniyordu ve ikisi de sensoru tanimlamiyordu.
#define SENSOR1_NAME      "SHT40"         // Birincil (MUX CH1)
#define SENSOR2_NAME      "SHT30"         // Ikincil  (MUX CH2)

// -------- SHT40 (Birincil, MUX CH1) --------
// Kutuphane: #include <SensirionI2CSht4x.h>
#define SHT40_ADDR              0x44    // ADDR=GND -> 0x44 | ADDR=VCC -> 0x45
#define SHT40_CMD_MEAS_HI       0xFD    // High precision measurement
#define SHT40_CMD_MEAS_MED      0xF6    // Medium precision measurement
#define SHT40_CMD_MEAS_LO       0xE0    // Low precision measurement
#define SHT40_CMD_SOFT_RESET    0x94    // Soft reset
#define SHT40_CMD_READ_SERIAL   0x89    // Seri numarasi okuma
#define SHT40_MEAS_DELAY_HI_MS  10      // High precision bekleme suresi (ms)
#define SHT40_MEAS_DELAY_MED_MS  5      // Medium precision bekleme suresi (ms)
#define SHT40_MEAS_DELAY_LO_MS   2      // Low precision bekleme suresi (ms)
// CRC-8 (poly=0x31, init=0xFF)
#define SHT40_CRC_POLY          0x31
#define SHT40_CRC_INIT          0xFF

// -------- SHT30 (Ikincil, MUX CH2) --------
// Kutuphane: Wire ile direkt I2C (veya SHT3x uyumlu herhangi bir kutuphane)
#define SHT30_ADDR              0x44    // ADDR=GND -> 0x44 | ADDR=VCC -> 0x45
#define SHT30_CMD_MEAS_HI       0x2C06  // High repeatability, clock stretching ON
#define SHT30_CMD_MEAS_HI_NCS   0x2400  // High repeatability, clock stretching OFF
#define SHT30_MEAS_DELAY_MS     20      // Yuksek hassasiyet olcum bekleme suresi (ms)
// Eski SHT31 uyumluluk (ayni Sensirion protokol)
#define SHT31_ADDR              SHT30_ADDR
#define SHT31_CMD_MEAS_HI       0x2400
#define SHT31_MEAS_DELAY_MS     16

// -------- Sensor Fuzyon Parametreleri --------
#define FUSION_TEMP_TOLERANCE  0.5f  // C - iki sensor arasi max kabul edilebilir fark
#define FUSION_HUM_TOLERANCE   1.5f  // % - iki sensor arasi max kabul edilebilir fark
#define FUSION_DIVERGE_COUNT   5     // Uyumsuzluk sayaci esigi -> uyari

// -------------------- PCF8574 I2C GPIO Expander (4'lü Röle Kartı) --------------------
#define PCF8574_ADDR        0x20    // PCF8574 varsayılan adres (A0=A1=A2=GND)
// Röle kartı bağlantısı: Grove 8 kanal MUX Kanal 7 (SD7/SC7) üzerinden I2C
// PCF8574 pin atamaları (Active LOW - LOW=Röle AÇIK, HIGH=Röle KAPALI)
#define RELAY_BIT_HEATER        0   // P0 - Isıtıcı rölesi
#define RELAY_BIT_HUMIDIFIER    1   // P1 - Nemlendirici rölesi
#define RELAY_BIT_TURNER_POWER  2   // P2 - Yumurta çevirme motor güç rölesi
#define RELAY_BIT_TURNER_DIR    3   // P3 - Yumurta çevirme motor yön rölesi
#define RELAY_ALL_OFF           0xFF // Tüm röleler kapalı (Active LOW)
// Donanım Notu: Röle kartı, PCF8574 üzerinden kontrol edilir. Röleler aktif olduğunda ilgili pin LOW olmalıdır.
// Bağlantı örneği: P0=Isıtıcı, P1=Nemlendirici, P2=Motor Güç, P3=Motor Yön
// Röle kartı, MUX'un 7. kanalına (SD7/SC7) bağlanmalıdır.

// -------------------- Isitici Kontrol Modu --------------------
// 0 = PID + Time-Proportional (rolu HEATER_WINDOW_MS pencerede sik anahtarlar)
// 1 = BANG-BANG (basit termostat: sicaklik dustugunde ON, ulasinca OFF)
//     Klasik kulucka davranisi — role cok daha az tiklar, mekanik omur uzar.
//     PID hesaplanmaya devam eder (auto-tune icin) ama cikis yok sayilir.
#define HEATER_BANG_BANG_MODE   1

// Bang-bang histerezisi (°C):
//   temp <= setpoint - HYST_LO  -> ON
//   temp >= setpoint + HYST_HI  -> OFF
//   arasi: onceki durum korunur (rolenin az tiklamasi icin sart)
#define HEATER_HYST_LO_C        0.2f   // ON esigi (setpoint'in altinda)
#define HEATER_HYST_HI_C        0.0f   // OFF esigi (setpoint uzerinde)

// -------------------- Temizlik (Bakim) Modu --------------------
// Kullanici temizlik/test icin tum otomatik kontrolu bypass edip
// heater/fan/humidifier'i manuel ayarlar. Guvenlik icin otomatik timeout.
#define CLEANING_TIMEOUT_MS     (30UL * 60UL * 1000UL)  // 30 dakika
// Default manuel cikislar (baslangicta hepsi kapali — kullanici gerekli
// olani acar). Isitici kasten 0 birakildi (guvenli varsayilan).
#define CLEANING_DEFAULT_HEATER 0
#define CLEANING_DEFAULT_FAN    200    // Fan genelde havalandirma icin acik
#define CLEANING_DEFAULT_HUM    false
#define CLEANING_DEFAULT_TURNER false

// -------------------- Isitici Time-Proportional Kontrol --------------------
// (Yalnizca HEATER_BANG_BANG_MODE=0 iken kullanilir)
#define HEATER_WINDOW_MS    10000   // ms - role pencere suresi (10 sn)

// -------------------- Röle Aşınma Koruması --------------------
// Mekanik röle ömrü: ~100.000 anahtarlama. Koruma olmadan yılda ~500.000 geçiş olabilir.
#define RELAY_MIN_ON_MS     500     // ms - minimum röle AÇIK süresi (daha kısa -> atla)
#define RELAY_MIN_OFF_MS    500     // ms - minimum röle KAPALI süresi
#define RELAY_COOLDOWN_MS   2000    // ms - nemlendirici röle anahtarlama arası min bekleme

// -------------------- DS3231 RTC --------------------
// DS3231 varsayılan I2C adresi: 0x68

// RTCManager — gercek dunya davranis ayarlari
#define RTC_CACHE_INTERVAL_MS  1000   // Cached okuma: 1 sn'de bir gercek I2C okuma
#define RTC_RETRY_COUNT        3      // I2C okumada hata alirsa tekrar sayisi
#define RTC_RETRY_DELAY_MS     5      // Tekrarlar arasi bekleme suresi
#define RTC_VALID_YEAR_MIN     2024   // Bu yildan kucuk tarih = gecersiz
#define RTC_VALID_YEAR_MAX     2099   // Bu yildan buyuk tarih = gecersiz
#define RTC_HEALTH_TIMEOUT_MS  10000  // Bu sure boyunca okuma yoksa I2C_FAIL kabul edilir

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

// Ekran VERISI hazirlama araligi (cizimden ayri).
// updateDisplay() DisplayData'yi bastan kurar ve icinde String tahsisleri
// vardir. loop() delay'siz calistigi icin bu, saniyede binlerce kez
// yapiliyordu; ekran ise 500 ms'de bir yenileniyor. Bosuna heap churn,
// uzun calismada (18-21 gunluk kulucka) fragmentasyon riski demekti.
// 50 ms dokunmatik yoklamasi icin fazlasiyla yeterli: TOUCH_DEBOUNCE_MS 250.
#define DISPLAY_DATA_INTERVAL_MS  50

// -------------------- Yumurta Cevirme — Donanim Tipi Secimi --------------------
// 0 = ROLE/DC motor (PCF8574 P2=Guc, P3=Yon, mevcut donanim)
// 1 = STEP MOTOR (PCF8574 P4=STEP, P5=DIR, P6=EN -> A4988/DRV8825 surucu)
// Step motor secilirse `StepperTurnerDriver` modulu kullanilir, gercek aci
// kontrolu mumkun olur (encoder gibi davranir).
#define TURNER_TYPE         0       // 0=Role(DC), 1=Step

// -------------------- Yumurta Cevirme (PCF8574 Role ile) --------------------
// L298N yerine PCF8574 uzerinden 2 role ile motor kontrolu:
//   Role 3 (P2): Motor guc ON/OFF
//   Role 4 (P3): Motor yon degistirme (OFF=Ileri, ON=Geri)
// DONANIM NOTU: Motor + beslemesi 2 roleye H-koprusu seklinde baglanir.
//   Role3=OFF, Role4=X  -> Motor KAPALI (guc yok)
//   Role3=ON,  Role4=OFF -> Motor ILERI
//   Role3=ON,  Role4=ON  -> Motor GERI
#define TURNER_INTERVAL_MS  3600000 // ms - cevirme araligi (1 saat) [eski varsayilan]
#define TURNER_DURATION_MS  5000    // ms - cevirme suresi [profile yoksa fallback]
#define TURNER_PAUSE_MS     1000    // ms - yon degistirme arasi bekleme

// -------------------- Cevirme Aci Kalibrasyonu (DC motor) --------------------
// Profilde "turningAngleDeg" tanimliysa surucu su formulu uygular:
//   durationSec = angleDeg / TURNER_DEG_PER_SEC
// Yumurta tepsiniz tam 90° donmek icin kac saniye surduyse o degerden geri
// hesap yapin: TURNER_DEG_PER_SEC = 90.0 / olculen_sn.
// Ornek: tepsi 90°'ye 15 sn'de doniyorsa -> 6.0 deg/sn.
#define TURNER_DEG_PER_SEC      6.0f    // °/sn - kalibrasyon (DC motor)
#define TURNER_DEFAULT_ANGLE_DEG 90     // ° - profilde tanimlanmamissa varsayilan

// -------------------- Step Motor (TURNER_TYPE=1 secildiginde aktif) --------------
// PCF8574 ekstra pinleri (P4-P7 bos):
#define STEPPER_BIT_STEP        4       // P4 -> A4988/DRV8825 STEP
#define STEPPER_BIT_DIR         5       // P5 -> A4988/DRV8825 DIR
#define STEPPER_BIT_EN          6       // P6 -> A4988/DRV8825 ENABLE (active LOW)
// Motor + mekanik dislisi:
//   stepsPerRev × microstep × gearRatio = step / yumurta tepsisi tam donus (360°)
#define STEPPER_STEPS_PER_REV   200     // NEMA17 standardi
#define STEPPER_MICROSTEP       1       // A4988 MS1/MS2/MS3=GND -> full step
#define STEPPER_GEAR_RATIO      5       // Yumurta tepsisi mekanik dislisi (1=direkt)
// Hareket hizi: PCF8574 I2C bandwidth nedeniyle ~250 step/sn ust limit pratik
#define STEPPER_STEP_PERIOD_US  10000   // µs - step pulse arasi (10 ms = 100 step/sn)
#define STEPPER_PULSE_WIDTH_US  100     // µs - STEP HIGH suresi
#define STEPPER_DISABLE_AT_IDLE 1       // 1 = hareket sonrasi EN=HIGH (motoru sogut)

// -------------------- Fan (PWM) --------------------
// Board'un SPI Peripheral Interface konektoru CLK pini kullanildi:
#define FAN_PIN             18      // Fan PWM pini (SPI CLK konektoru)

// -------------------- PWM Parametreleri --------------------
#define PWM_FREQ_FAN        25000   // 25 kHz (fan icin)
#define PWM_RESOLUTION      8       // 8-bit (0-255)

// -------------------- SD Kart --------------------
#define SD_ENABLED          1       // 0=SD devre disi, 1=SD aktif
#ifndef SD_CS_PIN
#define SD_CS_PIN           5       // MicroSD CS pini
#endif

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

// -------------------- PID Gelismis Davranis --------------------
// Setpoint ramping: ani setpoint degisikliklerinde yumusak gecis (°C/saniye).
// 0 = kapali (anlik atlama). Onerilen: 0.05 (1°C ~20 saniyede degisir)
#define PID_SETPOINT_RAMP_DEG_PER_SEC  0.05
// Output slew rate: PWM saniyede max degisim (role omru icin).
// 0 = limit yok. Onerilen: 100 (0->255 ~2.5 sn'de yapilir)
#define PID_OUTPUT_SLEW_PWM_PER_SEC    100.0
// Derivative low-pass filter: turev terimini yumusatir (sensor noise korumasi).
// 0.0 = LPF kapali, 1.0 = max filtreleme. Onerilen: 0.7 (denge)
#define PID_DERIVATIVE_LPF_ALPHA       0.7
// Conditional anti-windup: cikis sature ise integral biriktirmeyi durdur

// -------------------- Sensör Filtresi --------------------
#define EMA_ALPHA           0.2f    // Exponential Moving Average katsayısı
#define SPIKE_THRESHOLD     2.0f    // °C - ani sıçrama eşiği
#define SENSOR_READ_INTERVAL 2000   // ms

// -------------------- Sensör Kalibrasyon & Doğrulama --------------------
// Geçerli ölçüm aralığı (kuluçka ortamı için sıkı)
#define SENSOR_VALID_TEMP_MIN   15.0f   // °C - bunun altı geçersiz okuma
#define SENSOR_VALID_TEMP_MAX   50.0f   // °C - bunun üstü geçersiz okuma
#define SENSOR_VALID_HUM_MIN     5.0f   // % - bunun altı geçersiz
#define SENSOR_VALID_HUM_MAX   100.0f   // % - bunun üstü geçersiz
// Bayat veri zaman aşımı
#define SENSOR_STALE_TIMEOUT_MS 15000   // ms - 15 sn güncelleme yoksa bayat

// -------- Yumurta IR Termometre - UZAK / YEDEK (EggTempService) --------
// Ayri bir ESP32 uzerindeki IR sensorden HTTP ile cekilir. Yerel MLX90614
// (MUX CH4) gecerli veri verdigi surece kullanilmaz; sadece yedektir.
#define YUMURTA_FETCH_INTERVAL_MS  10000  // ms - Yumurta ESP32'den kac ms'de bir sicaklik cek
// Failover geçiş yumuşatma
#define FAILOVER_BLEND_COUNT    5       // Geçiş sırasında kaç okumada yumuşat

// -------- Yumurta IR Termometre - YEREL / BIRINCIL (MLX90614ESF-BCC) --------
// Modul: "Arduino Kizilotesi Termometre Modulu (GY-906 / MLX90614ESF-BCC)"
// Baglanti: Grove 8 kanal I2C MUX Kanal 4 (SD4/SC4). Harici kutuphane gerekmez,
// Wire ile SMBus protokolu kullanilir (MLX90614 max 100kHz -> I2C_CLOCK_SPEED uyumlu).
#define MLX90614_ADDR           0x5A    // Fabrika varsayilan SMBus adresi
#define MLX90614_REG_TA         0x06    // RAM: ortam (die) sicakligi
#define MLX90614_REG_TOBJ1      0x07    // RAM: nesne 1 sicakligi (yumurta yuzeyi)
#define MLX90614_REG_TOBJ2      0x08    // RAM: nesne 2 (tek bolgeli modelde kullanilmaz)
#define MLX90614_RAW_LSB        0.02f   // Ham deger -> Kelvin katsayisi
#define MLX90614_KELVIN_OFFSET  273.15f // Kelvin -> Celsius
#define MLX90614_ERROR_FLAG     0x8000  // Ham degerde bu bit set ise okuma gecersiz
#define MLX90614_PEC_POLY       0x07    // SMBus PEC CRC-8 polinomu

#define EGG_IR_READ_INTERVAL_MS 2000    // ms - yerel IR sensor okuma araligi
#define EGG_IR_FAIL_LIMIT       5       // Art arda bu kadar hata -> sensor gecersiz
#define EGG_IR_VALID_TEMP_MIN   10.0f   // °C - bunun altindaki okuma gecersiz sayilir
#define EGG_IR_VALID_TEMP_MAX   60.0f   // °C - bunun ustundeki okuma gecersiz sayilir
// Emisivite: sensor fabrika ayari 1.00, yumurta kabugu ~0.95. Kulucka icinde
// yumurta ile ortam sicakligi birbirine cok yakin oldugundan bu farkin etkisi
// kucuktur; kalan sapma asagidaki ofset ile duzeltilir (EEPROM yazilmaz).
#define EGG_IR_TEMP_OFFSET      0.0f    // °C - referans termometreye gore duzeltme

// -------- Yumurta (kabuk) Sicaklik Alarmlari --------
// Kabuk sicakligi embriyonun gercek sicakligina en yakin olcumdur; ortam
// sicakligi bunun sadece dolayli gostergesidir. Asiri isinma embriyoyu
// hizla oldurur, bu yuzden UST sinir alarmi kritiktir.
//
// ONEMLI: Kulucka'nin ilk gunlerinde embriyo henuz isi uretmedigi icin kabuk
// sicakligi hedefin ALTINDA seyreder; son gunlerde metabolik isi nedeniyle
// hedefin USTUNE cikar. Bu yuzden alt sinir toleransi bilerek genis tutuldu.
#define EGG_TEMP_ALARM_ENABLED   1      // 0 = yumurta sicaklik alarmlari kapali
#define EGG_TEMP_TOLERANCE_HIGH  1.0f   // °C - hedefin bu kadar ustu -> alarm
#define EGG_TEMP_TOLERANCE_LOW   2.5f   // °C - hedefin bu kadar alti -> alarm
#define EGG_TEMP_ALARM_MIN       30.0f  // °C - bunun altinda alarm uretme
                                        // (kapak acik / yeni yerlestirilmis yumurta)
#define EGG_SOURCE_LOST_MS       120000 // ms - kaynak bu sure yoksa alarm (2 dk)

// -------------------- Nem Kontrolü --------------------
#define HUMIDIFIER_OFF_DELAY 10000  // ms - gecikmeli kapatma
#define HUMIDIFIER_MIN_ON    5000   // ms - minimum çalışma süresi

// -------------------- Fan Kontrolü --------------------
#define FAN_MIN_PWM         80      // Minimum fan hızı (asla durmaz)
#define FAN_MAX_PWM         255     // Maksimum fan hızı

// FanDriver gercek-dunya davranisi (real-world PWM control)
#define FAN_OFF_THRESHOLD   20      // Bu PWM altinda fan zaten donemez -> tamamen kapat
#define FAN_KICKSTART_MS    250     // Kapalidan acilirken tam PWM uygulama suresi (ms)
#define FAN_RAMP_STEP       8       // Her ramp adiminda PWM degisim miktari (0-255)
#define FAN_RAMP_INTERVAL_MS 20     // Ramp adimlari arasi sure (ms) -> 20ms*8/iter ~ 1sn full sweep
#define FAN_TEMP_LOW        36.0f   // Fan haritalaması alt sıcaklık
#define FAN_TEMP_HIGH       38.0f   // Fan haritalaması üst sıcaklık

// -------------------- Alarm Eşikleri --------------------
// EsKi statik esikler — sadece fallback (profil yokken). v3'te dinamik
// profil-bazli esikler kullanilir (AlarmService.check parametreleri).
#define ALARM_THRESHOLD_TEMP_HIGH  38.5f   // °C
#define ALARM_THRESHOLD_TEMP_LOW   36.0f   // °C
#define ALARM_THRESHOLD_HUM_HIGH   85.0f   // %
#define ALARM_THRESHOLD_HUM_LOW    30.0f   // %
#define ALARM_COOLDOWN      60000   // ms - tekrar alarm arası bekleme

// Profil bazli dinamik esik toleransi (hedef sicakliktan ±delta)
// Kuluckada hedef +/- 0.7C disina cikarsa alarm. Web/TFT'den ayarlanabilir.
#define ALARM_TEMP_TOLERANCE       0.7f    // °C — targetTemp ± bu kadar
// Nem alt/ust profilden direkt gelir; ek tolerans yok (humLow / humHigh).

// Mute (kullanici alarmi sustursa) sonrasi otomatik tekrar acilana kadar sure.
// Durum normale donerse alarm kalkar; bozulursa bu suredan sonra yeniden calar.
#define ALARM_MUTE_DURATION_MS     (10UL * 60UL * 1000UL)  // 10 dakika

// Dol kontrolu (candling) alarm "ERTELE" butonuna basildiginda susturma suresi
#define CANDLING_SNOOZE_DURATION_MS (60UL * 60UL * 1000UL)  // 1 saat

// -------------------- Pause Modu Acil Isil Kontrol --------------------
// SYS_PAUSED durumunda PID aktif degildir; bang-bang + histerezis kullanilir.
// Histerezis: role'nin cok sik anahtarlanmasini (bounce) onler.
//   Isitma baslar : temp < targetTemp - PAUSE_HEAT_HYSTERESIS
//   Isitma biter  : temp > targetTemp - 0.1 C
//   Sogutma baslar: temp > targetTemp + PAUSE_COOL_HYSTERESIS
#define PAUSE_EMERGENCY_HEATER_PWM  100   // 0-255: isitma PWM'i (≈39%, overshoot olmadan ısıtır)
#define PAUSE_IDLE_FAN_PWM           60   // 0-255: normal durumda minimum sirkülasyon
#define PAUSE_HEAT_FAN_PWM           25   // 0-255: ISITIRKEN cok dusuk fan (~80 rpm) - sicak noktayi dagitir, ısıyı kaçırmaz
#define PAUSE_HEAT_HYSTERESIS       0.5f  // °C: heaterin ON olacagi alt esik (tgt - bu deger)
#define PAUSE_COOL_HYSTERESIS       0.5f  // °C: fanin max olacagi ust esik (tgt + bu deger)

// -------------------- HOPARLOR (PAM8403 + 1W 8ohm) --------------------
// E32R28T / E32N28T (2.8" ESP32-32E Display, ILI9341 + XPT2046) board:
//   - SPEAKER_PIN     = GPIO26  (Audio DAC output -> PAM8403 IN+)
//   - SPEAKER_AMP_EN  = GPIO4   (Audio enable: LOW=enable, HIGH=disable)
//   Onboard hoparlor konektoru "SPEAKER" silkscreen ile isaretli.
//   Modulu derlerken SPEAKER_ENABLED=0 yaparsan tamamen devre disi.
//
// DIKKAT: GPIO4 ayni zamanda RS485 DE/RE icin kullaniliyor olabilir
// (memory: project_pin_map). E32R28T board'da IO4 amplifikator enable
// signaline baglidir; RS485 kullanmak istersen baska board gerekir.
#ifndef SPEAKER_ENABLED
#define SPEAKER_ENABLED        1
#endif
#ifndef SPEAKER_PIN
#define SPEAKER_PIN            26      // GPIO26 — DAC2 / Audio out (PAM8403 IN+)
#endif
#ifndef SPEAKER_AMP_EN_PIN
#define SPEAKER_AMP_EN_PIN     4       // GPIO4 — Amplifikator enable (LOW=ON)
#endif
#ifndef SPEAKER_ALARM_FREQ
#define SPEAKER_ALARM_FREQ     2400    // Hz — keskin uyari sesi
#endif
#ifndef SPEAKER_BEEP_FREQ
#define SPEAKER_BEEP_FREQ      1800    // Hz — UI feedback (yumusak)
#endif

// -------------------- Güvenlik --------------------
#define SAFETY_TEMP_MAX     40.0f   // °C - acil kapatma sıcaklığı (üst)
#define SAFETY_TEMP_MIN     20.0f   // °C - sensör hatası şüphesi (uyarı)
#define SAFETY_TEMP_CRITICAL_LOW 25.0f // °C - acil kapatma (alt) - ısıtıcı sonsuz çalışma koruması
#define SAFETY_LOW_TEMP_DURATION 120000 // ms - 2 dk boyunca düşükse shutdown
#define SAFETY_SENSOR_FAIL_COUNT 5  // Ardışık hata sayısı

// -------- Shutdown Dogrulama (yapisik role / termal kacis tespiti) --------
// emergencyShutdown() cikislari kapatir, ancak role kontagi yapismissa isitici
// fiziksel olarak calismaya devam eder. Kapatmadan sonra sicaklik gercekten
// dusuyor mu diye kontrol edilir; dusmuyorsa termal kacis kabul edilir.
#define SAFETY_VERIFY_DELAY_MS   90000  // ms - kapatma sonrasi dogrulama suresi
#define SAFETY_VERIFY_TEMP_DROP  0.5f   // °C - bu sure sonunda beklenen min dusus
#define SAFETY_RUNAWAY_FAN_PWM   255    // Termal kacista fan tam guce alinir

// -------------------- I2C Bus Saglik / Kurtarma --------------------
// Roleleri anahtarlayan bir kutuda EMI kaynakli I2C takilmasi gercek bir
// senaryodur. Takilan slave SDA'yi LOW tutarsa bus komple kilitlenir; bu
// durumda SCL'e manuel darbe gonderip slave'in birakmasi saglanir.
#define MUX_FAIL_LIMIT          3    // Art arda kanal secim hatasi -> bus kurtarma
// Iki kurtarma denemesi arasi minimum sure. Kurtarma bus'i yikip yeniden
// kurdugu ve 8 adres taradigi icin pahalidir; TAKILI OLMAYAN bir cihaz
// yuzunden her dongude tetiklenirse sistem kurtarma dongusunde kilitlenir.
#define MUX_RECOVER_COOLDOWN_MS 10000
#define I2C_RECOVER_PULSES      9    // Takilan slave'i birakmak icin SCL darbe sayisi
#define I2C_RECOVER_PULSE_US    5    // Darbe yari periyodu (us) -> ~100kHz
#define RELAY_WRITE_FAIL_LIMIT  3    // Art arda role yazma hatasi -> IO arizasi

// -------------------- Sistem --------------------
#define LOOP_INTERVAL       2000    // ms - ana döngü aralığı
#define SERIAL_BAUD         115200
#define WATCHDOG_TIMEOUT    8       // saniye

// -------------------- Web Server --------------------
#define WEB_SERVER_PORT     80
#define API_PREFIX          "/api"

// -------------------- Captive Portal --------------------
// AP'ye baglanan cihazin tarayicisi otomatik olarak kurulum sayfasina yonlendirilir.
// STA henuz baglanmamissa aktif, baglanir baglanmaz otomatik kapanir.
#define CAPTIVE_DNS_PORT       53
#define CAPTIVE_PORTAL_URL     "/portal"
#define CAPTIVE_CONNECT_TIMEOUT 15000   // ms - kullanicinin secimi sonrasi STA baglanma zaman asimi

// -------------------- OTA --------------------
#define OTA_HOSTNAME        "KuluckaMakinesi"
#define OTA_PASSWORD        ""      // Boş = şifresiz

// -------------------- OTA (Pull-based / GitHub Releases) --------------------
// Cihaz "Guncelleme Kontrol Et" butonuna basildiginda sunucudan version.json
// indirilir, yerel FW_VERSION ile karsilastirilir. Kullanici onaylarsa .bin
// indirilir, SHA256 dogrulanir ve flash edilir.
//
// version.json formati (ornek):
// {
//   "version": "1.1.0",
//   "build_date": "2026-04-21",
//   "url": "https://github.com/kullanici/repo/releases/download/v1.1.0/firmware.bin",
//   "sha256": "64_karakterlik_hex_string",
//   "size": 987654,
//   "changelog": "Yenilikler..."
// }
#define OTA_UPDATE_URL_DEFAULT  ""          // Bos = kullanici /api/ota/url ile ayarlamali
#define OTA_DOWNLOAD_TIMEOUT    30000       // ms - HTTP zaman asimi
#define OTA_DOWNLOAD_BUFFER     1024        // bytes - indirme chunk boyutu
#define OTA_USER_AGENT          "KuluckaMakinesi"
#define OTA_VERSION_JSON_MAX    4096        // bytes - version.json maks boyut

// -------------------- Veri Loglama --------------------
#define LOG_INTERVAL        60000   // ms - log kayit araligi (1 dk)

#endif // CONFIG_H
