#ifndef CONFIG_H
#define CONFIG_H

// =====================================================================
//  WiFi Ayarları
// =====================================================================
#define WIFI_SSID        "WIFI_ADINIZI_YAZIN"
#define WIFI_PASSWORD    "WIFI_SIFRENIZI_YAZIN"

// Bağlantı zaman aşımı (ms)
#define WIFI_TIMEOUT_MS  15000

// =====================================================================
//  Donanım Pinleri  (ESP32-C3 Mini)
// =====================================================================
// GY-906 (MLX90614) I2C bağlantısı
#define PIN_SDA          8    // GPIO8  → GY-906 SDA
#define PIN_SCL          9    // GPIO9  → GY-906 SCL

// Durum LED'i (ESP32-C3 Mini üzerindeki dahili RGB LED - GPIO8,
// ama çakışma varsa GPIO2'ye bağlı harici LED kullanın)
#define PIN_STATUS_LED   10   // GPIO10 → isteğe bağlı durum LED'i

// =====================================================================
//  MLX90614 (GY-906) Ayarları
// =====================================================================
#define MLX90614_ADDR    0x5A  // Varsayılan I2C adresi

// Emissivity: yumurta kabuğu için ≈ 0.95 (1.0 = mükemmel siyah cisim)
// Ham kayıt değeri: emissivity * 65535 / 1.0
// Kütüphane varsayılanı 1.0; burada yazılımsal düzeltme kullanıyoruz
#define EGG_EMISSIVITY_CORRECTION  -0.3f  // °C düzeltme (kalibrasyon sonrası ayarlayın)

// =====================================================================
//  Sıcaklık Okuma Ayarları
// =====================================================================
#define TEMP_READ_INTERVAL_MS  500    // Her 500 ms'de bir oku
#define TEMP_HISTORY_SIZE      10     // Son N okumanın ortalaması (gürültü azaltma)

// Geçerli ölçüm aralığı (IR sensör kalibrasyonu)
#define TEMP_VALID_MIN   5.0f    // °C - bunun altı geçersiz
#define TEMP_VALID_MAX   60.0f   // °C - bunun üstü geçersiz

// =====================================================================
//  HTTP Sunucu
// =====================================================================
#define HTTP_PORT        80

// =====================================================================
//  mDNS Hostname
//  Ana makineden: http://yumurta.local/api/egg
// =====================================================================
#define MDNS_HOSTNAME    "yumurta"

#endif // CONFIG_H
