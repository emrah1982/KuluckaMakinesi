# Kuluçka Makinesi — ESP32 Akıllı Kontrol Sistemi

ESP32 tabanlı, 11 hayvan profili destekleyen çok evreli kuluçka kontrol sistemi.
3.2" TFT dokunmatik ekran, web arayüzü ve OTA güncelleme içerir.

> Ayrıntılı kullanım için: [`KuluckaMakinesi_Esp32/KULLANIM_KILAVUZU.md`](KuluckaMakinesi_Esp32/KULLANIM_KILAVUZU.md)

---

## Donanım Bağlantıları

### ESP32 Pin Haritası

| Fonksiyon | Pin | Açıklama |
|-----------|-----|----------|
| I2C SDA | **IO32** | Tüm I2C cihazları (MUX üzerinden) |
| I2C SCL | **IO25** | Tüm I2C cihazları (MUX üzerinden) |
| Fan PWM | **IO18** | L298N `ENA` — 25 kHz (opsiyonel, aşağıya bak) |
| TFT arka ışık | IO27 | Ekran parlaklığı |
| Dokunmatik CS | IO33 | XPT2046 |
| Dokunmatik IRQ | IO36 | Dokunma algılama |
| SD kart CS | IO5 | MicroSD |
| Hoparlör | IO26 | Alarm sesi |
| Amplifikatör EN | IO4 | Hoparlör güç kontrolü |

### I2C Yapısı — Grove 8 Kanal Multiplexer (TCA9548A, `0x70`)

Tüm I2C cihazları tek bir multiplexer arkasındadır.

```
ESP32 (IO32 / IO25)
  │
  └── Grove 8 Kanal I2C MUX (0x70)
        ├── CH0 : DS3231 RTC        (0x68) + AT24C32 EEPROM (0x57)
        ├── CH1 : SHT40             (0x44)  birincil sıcaklık/nem
        ├── CH2 : SHT30             (0x44)  ikincil (füzyon/yedek)
        ├── CH3 : SCD30             (0x61)  CO2
        ├── CH4 : MLX90614ESF-BCC   (0x5A)  yumurta IR sıcaklık
        ├── CH5 : (boş)
        ├── CH6 : (boş)
        └── CH7 : PCF8574           (0x20)  röle kartı + yardımcı çıkışlar
```

**MUX adresi:** A0/A1/A2 pinleri GND'ye bağlıyken `0x70`. Adres değişse bile
firmware 0x70–0x77 arasını otomatik tarar; `MUX_ADDR` elle değiştirilmesi
gerekmez.

### PCF8574 Çıkışları (MUX CH7)

| Bit | Uç | Bağlantı | Not |
|-----|-----|----------|-----|
| P0 | Röle 1 | Isıtıcı | Active LOW |
| P1 | Röle 2 | Nemlendirici | Active LOW |
| P2 | Röle 3 | Çevirme motoru güç | Active LOW |
| P3 | Röle 4 | Çevirme motoru yön | Active LOW |
| P4 | — | A4988 `STEP` | Yalnızca `TURNER_TYPE=1` |
| P5 | — | A4988 `DIR` | Yalnızca `TURNER_TYPE=1` |
| P6 | — | A4988 `ENABLE` | Aktif LOW |
| P7 | — | **L298N `IN1`** | Fan enable |

> **Güvenli çıkış durumu `RELAY_ALL_OFF = 0x7F`.** Bit 7'nin **0** olması
> önemlidir: aksi halde "tüm çıkışlar kapalı" denen durumda L298N girişi
> enable konumunda kalır ve fanın durması tek bir sinyale bağlı olurdu.

### Fan Bağlantısı (L298N)

Fan röle üzerinden değil, L298N motor sürücü kartı üzerinden çalışır.

| # | Kaynak | L298N ucu | İşlev |
|---|--------|-----------|-------|
| 1 | PCF8574 **P7** | `IN1` | Enable — **zorunlu** |
| 2 | Sistem **GND** | `IN2` | Yön sabitleme — **zorunlu** |
| 3 | ESP32 **IO18** | `ENA` | Hız PWM — opsiyonel |
| 4 | Sistem **GND** | `GND` | **Ortak toprak — zorunlu** |
| 5 | 12V (+) | `+12V` | Motor beslemesi |
| 6 | 12V (−) | `GND` | Motor beslemesi dönüşü |

**`IN1` LOW iken motor dönmez**, `ENA` ne olursa olsun.

#### Ortak toprak — en sık atlanan nokta

`IN2`'yi PCF8574'ün GND'sine bağlamak **doğrudur**, ama tek başına yetmez:

> **L298N'in kendi `GND` terminali de aynı toprağa bağlanmalıdır.**

Sebebi: `IN1`'e gelen "HIGH" sinyali PCF8574'ün toprağına göre ölçülür.
L298N kendi girişlerini kendi `GND` pinine göre değerlendirir. Bu ikisi
birbirine bağlı değilse sinyalin referansı yoktur ve sürücü girişleri
tanımsız davranır — motor hiç dönmeyebilir veya rastgele davranabilir.

Pratikte tek kural: **ESP32, PCF8574, L298N ve 12V beslemenin eksi ucu —
dördü de aynı toprak noktasında buluşmalı.** Bu sağlandıktan sonra `IN2`'yi
bu topraktan herhangi birine bağlayabilirsiniz; PCF8574'ün GND'si de olur.

#### Sinyal seviyesi

PCF8574 3.3V ile beslenir ve çıkışları *quasi-bidirectional*'dır: HIGH
durumu zayıf bir dahili pull-up ile sağlanır (~100 µA). L298N'in mantık
girişi 3.3V'u HIGH olarak kabul eder, ancak akım marjı dardır.

> **Öneri:** `IN1` ile 3.3V arasına **4.7 kΩ** pull-up direnci ekleyin.
> PCF8574 LOW yazarken güçlü şekilde (25 mA'e kadar) çeker, HIGH yazarken
> dış direnç seviyeyi sağlam tutar. Uzun veya gürültülü kabloda bu fark
> yaratır.

#### İki kurulum seçeneği

`Config.h` içindeki `FAN_SPEED_CONTROL` buna göre ayarlanır:

| | Sadece P7 (`= 0`) | P7 + IO18 (`= 1`) |
|---|---|---|
| Sinyal kablosu | 1 | 2 |
| L298N `ENA` jumper'ı | **Takılı kalır** | **Çıkarılır** |
| Fan hızı | Tek hız (tam) | 0–255 arası |
| Sıcaklığa bağlı hız eğrisi | İşlevsiz | Çalışır |

> `ENA` jumper'ını çıkarmayı unutursanız `ENA` sürekli 5V'ta kalır ve
> IO18'den gelen PWM hiçbir etki yapmaz — fan hep tam hızda döner.

> **P7 ile hız ayarı yapılamaz.** P7 bir I2C genişletici pinidir; her seviye
> değişimi bir I2C işlemidir (100 kHz'de ~1 ms). Ulaşılabilecek frekans birkaç
> yüz Hz mertebesinde kalır — fan için gereken 25 kHz'in çok altında. Üstelik
> aynı hat RTC, sensörler ve röleleri de taşıdığı için bus tıkanır. Hız
> kontrolü ancak ESP32'nin donanım PWM çıkışı ile mümkündür.

---

## Yapılandırma (`Config.h`)

Sık değiştirilen anahtarlar:

| Sabit | Varsayılan | Açıklama |
|-------|-----------|----------|
| `DEBUG_ENABLED` | `1` | Seri monitörde periyodik `[STATUS]` satırı |
| `FAN_SPEED_CONTROL` | `1` | `0` = ENA kablosu yok, tek hız |
| `FAN_DUTY_MODE` | `0` | `1` = fan kesintili çalışır |
| `RELAY_TEST_ENABLED` | `1` | Röle test modu (devreye alma sonrası `0` yapın) |
| `TURNER_TYPE` | `0` | `0` = DC/röle, `1` = step motor |
| `SD_ENABLED` | `1` | SD kart veri kaydı |

### Fan kesintili çalışma

`FAN_DUTY_MODE = 1` yapıldığında fan her `FAN_DUTY_PERIOD_SEC` (5 dk) içinde
`FAN_DUTY_ON_SEC` (1 dk) çalışır, kalan sürede durur.

> **Kuluçkaya özgü uyarı:** Fan yalnızca soğutma değil, aynı zamanda
> **homojenlik** aracıdır. Uzun süre durursa kabin içinde katmanlaşma olur
> (üst sıcak, alt soğuk); sensör kendi bölgesini ölçer ve tepsinin geri kalanı
> hedeften sapabilir. Bu yüzden kesinti "tamamen kapalı" değil, **her periyotta
> garanti sirkülasyon** olarak tasarlandı. Ayrıca sıcaklık hedefi
> `FAN_DUTY_FORCE_DELTA` kadar aşarsa fan kesintisiz çalışmaya döner.

---

## Derleme

Arduino IDE ayarları:

- **Kart:** ESP32 Dev Module
- **Partition Scheme:** menüden seçmeye gerek yok — sketch klasöründeki
  `partitions.csv` kullanılır (2 × 1.9 MB app + OTA destekli). Menüden
  seçilecekse *Minimal SPIFFS (1.9MB APP with OTA)*.
  **"Huge APP" seçmeyin**, OTA bölümü yoktur.

Gerekli kütüphaneler:

```
RTClib (Adafruit)          TFT_eSPI
ESPAsyncWebServer          AsyncTCP  (mathieucarbou sürümleri)
```

---

## Güvenlik Katmanları

| Katman | Eşik / Koşul | Aksiyon |
|--------|--------------|---------|
| Aşırı sıcaklık | ≥ 40 °C | Acil kapatma |
| Kritik düşük sıcaklık | < 25 °C, 2 dk | Acil kapatma |
| Sensör arızası | 5 ardışık hata | Acil kapatma |
| Röle yazma hatası | 3 ardışık hata | `ALARM_IO_FAIL` + kaçış modu |
| **Termal kaçış** | Kapatmadan 90 sn sonra sıcaklık düşmüyor | Fan tam güç + kalıcı alarm |
| I2C kilitlenme | 3 ardışık kanal hatası | Otomatik bus kurtarma |

> **Termal kaçış**, röle kontağının yapışması durumudur. Yazılımın yapabileceği
> bir şey kalmaz — ısıtıcı beslemesini elle kesmek gerekir. Bu senaryoya karşı
> gerçek koruma, yazılımdan bağımsız bir **donanım termostatı** takmaktır.

---

## Depo Yapısı

```
KuluckaMakinesi/
├── KuluckaMakinesi_Esp32/
│   ├── KuluckaMakinesi/            Ana firmware
│   ├── KuluckaMakinesi_Yumurta_Esp32/  Uzak yumurta IR cihazı (opsiyonel)
│   ├── I2CScanner/                 I2C tanılama eskizi
│   └── KULLANIM_KILAVUZU.md        Ayrıntılı kullanım kılavuzu
├── KuluckaMakinesi_kicad/          Devre şeması ve PCB
├── docker/                         Geliştirme için sahte sunucu
├── data/ · db/                     Web arayüzü varlıkları, örnek veri
└── docs/                           Tasarım notları
```
