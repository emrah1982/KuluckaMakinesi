# ESP32 Kuluçka Makinesi - Kurulum Rehberi

## ⚠️ KRİTİK AYARLAR (Arduino IDE)

### 1. Partition Scheme (ÇOK ÖNEMLİ)
```
Tools → Partition Scheme → "Huge APP (3MB No OTA/1MB SPIFFS)"
```
> Bu ayar olmadan kod sığmayabilir!

### 2. PSRAM (Varsa)
```
Tools → PSRAM → "Enabled"
```

### 3. Board Ayarları
```
Board: ESP32 Dev Module
Upload Speed: 921600
CPU Frequency: 240MHz
Flash Frequency: 80MHz
Flash Mode: QIO
Flash Size: 4MB
```

---

## 📚 Gerekli Kütüphaneler

### Arduino Library Manager'dan:
1. **TFT_eSPI** - Bodmer
2. **ESPAsyncWebServer** - me-no-dev
3. **AsyncTCP** - me-no-dev
4. **RTClib** - Adafruit

### Manuel kurulum:
- ESPAsyncWebServer ve AsyncTCP GitHub'dan indirilmeli

---

## 🔧 TFT_eSPI Konfigürasyonu

**ÖNEMLİ:** `TFT_eSPI` kütüphanesinin `User_Setup.h` dosyasını düzenlemelisin.

### Adımlar:
1. Arduino kütüphane klasörüne git:
   ```
   Documents/Arduino/libraries/TFT_eSPI/
   ```

2. `User_Setup.h` dosyasını aç

3. `src/config/TFT_Setup.h` içeriğini buraya kopyala

**VEYA** `User_Setup_Select.h` içinde sadece şunu aktif et:
```cpp
#include <User_Setups/Setup24_ST7789.h>
```

---

## 📌 Pin Haritası (ESP32-3248S032)

### TFT Ekran (Kart üzerinde sabit)
| Pin | GPIO | Açıklama |
|-----|------|----------|
| TFT_CS | 15 | Chip Select |
| TFT_DC | 2 | Data/Command |
| TFT_RST | -1 | Reset (EN'e bağlı) |
| TFT_BL | 27 | Backlight |
| TFT_MOSI | 13 | SPI MOSI |
| TFT_SCLK | 14 | SPI Clock |
| TFT_MISO | 12 | SPI MISO |

### Dokunmatik (XPT2046)
| Pin | GPIO |
|-----|------|
| TOUCH_CS | 33 |
| TOUCH_IRQ | 36 |

### I2C (TCA9546A MUX + Cihazlar)
| Pin | GPIO |
|-----|------|
| SDA | 21 |
| SCL | 22 |

### RS485 (SHT20 Sensör)
| Pin | GPIO |
|-----|------|
| RX | 16 |
| TX | 17 |
| DE/RE | 4 |

### L298N (Yumurta Çevirme Motoru)
| Pin | GPIO |
|-----|------|
| ENA | 25 |
| IN1 | 23 |
| IN2 | 19 |

### Fan PWM
| Pin | GPIO |
|-----|------|
| FAN | 18 |

---

## 🧠 RAM Optimizasyonu

### Yapılanlar:
- ✅ TFT fontları azaltıldı (sadece GLCD + FONT2)
- ✅ JSON üretimi snprintf ile optimize edildi
- ✅ Heap monitor eklendi (Serial'da görünür)

### Serial Monitor'da kontrol:
```
[STATUS] T=37.5C H=55.0% ... | Heap=180KB
```
> Heap 50KB altına düşerse sorun var demektir!

---

## 🔌 Bağlantı Şeması

```
ESP32-3248S032 (TFT Kart)
│
├── I2C (GPIO21/22)
│   └── TCA9546A MUX (0x70)
│       ├── CH0 → DS3231 RTC
│       └── CH1 → PCF8574 (4'lü Röle)
│                 ├── P0: Isıtıcı
│                 ├── P1: Nemlendirici
│                 ├── P2: Buzzer
│                 └── P3: LED
│
├── RS485 (GPIO16/17/4)
│   └── MAX485 → SHT20 Sensör
│
├── L298N (GPIO25/23/19)
│   └── Yumurta Çevirme Motoru
│
└── Fan PWM (GPIO18)
    └── 12V Fan
```

---

## ⚡ İlk Çalıştırma

1. Partition scheme'i ayarla
2. TFT_eSPI User_Setup.h'ı düzenle
3. Kodu yükle
4. Serial Monitor aç (115200 baud)
5. WiFi AP'ye bağlan: `KuluckaMakinesi` / `kulucka123`
6. Tarayıcıda: `http://192.168.4.1`

---

## 🐛 Sorun Giderme

### "Sketch too large" hatası
→ Partition Scheme'i "Huge APP" yap

### TFT beyaz/siyah kalıyor
→ User_Setup.h pin tanımlarını kontrol et

### Heap düşük uyarısı
→ Web istekleri sırasında normal, sürekli düşükse sorun var

### Watchdog reset
→ Loop içinde blocking kod var mı kontrol et
