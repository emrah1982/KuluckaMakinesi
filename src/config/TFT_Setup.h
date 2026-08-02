// ============================================================
// TFT_eSPI Kutuphane Konfigurasyonu
// ESP32-3248S032 (3.2" ST7789 + XPT2046 Dokunmatik)
// ============================================================
//
// KURULUM:
// Bu dosyanin icerigini TFT_eSPI kutuphanesinin User_Setup.h
// dosyasina kopyalayin:
//   Arduino/libraries/TFT_eSPI/User_Setup.h
//
// VEYA platformio.ini kullaniyorsaniz build_flags ile tanimlayabilirsiniz.
// ============================================================

#define USER_SETUP_ID 99

// -------------------- Surucu Secimi --------------------
#define ST7789_DRIVER

// -------------------- Ekran Boyutu --------------------
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// -------------------- ESP32 SPI Pin Tanimlari --------------------
#define TFT_MOSI  13
#define TFT_SCLK  14
#define TFT_CS    15
#define TFT_DC    2
#define TFT_RST   -1    // ESP32 EN pinine bagli
#define TFT_BL    27    // Arka isik pini

// -------------------- SPI MISO (Touch icin gerekli) --------------------
#define TFT_MISO  12

// -------------------- Dokunmatik Ekran (XPT2046) --------------------
#define TOUCH_CS  33

// -------------------- SPI Frekansi --------------------
#define SPI_FREQUENCY       40000000   // 40 MHz TFT
#define SPI_READ_FREQUENCY  20000000   // 20 MHz okuma
#define SPI_TOUCH_FREQUENCY  2500000   // 2.5 MHz dokunmatik

// -------------------- Renk Sirasi --------------------
// ST7789 icin RGB sirasi (bazi ekranlar BGR kullanir)
// Renkler yanlis gorunurse bu satiri yoruma alin
// #define TFT_RGB_ORDER TFT_RGB

// -------------------- Font (RAM OPTIMIZASYONU) --------------------
// Sadece gerekli fontlar yukleniyor (Flash/RAM tasarrufu)
#define LOAD_GLCD    // Font 1 - Adafruit 8 piksel (temel)
#define LOAD_FONT2   // Font 2 - kucuk 16 piksel (ana font)
#define LOAD_FONT4   // Font 4 - orta 26 piksel (gauge degerleri)
// #define LOAD_FONT6   // Font 6 - buyuk rakamlar (kapatildi)
// #define LOAD_FONT7   // Font 7 - 7 segment (kapatildi)
// #define LOAD_FONT8   // Font 8 - buyuk (kapatildi)
// #define LOAD_GFXFF   // FreeFonts (kapatildi - cok RAM yer)
// #define SMOOTH_FONT  // (kapatildi - RAM tasarrufu)
