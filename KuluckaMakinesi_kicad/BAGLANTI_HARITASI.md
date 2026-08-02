---
date: 2026-06-18
project: KuluckaMakinesi
tags: [kicad, schematic, baglanti-haritasi, esp32, incubator]
status: active
---

# Kuluçka Makinesi — Bağlantı Haritası

**Şematik:** `KuluckaMakinesi.kicad_sch` (rev 2.0)
**Ana kontrol:** ESP32-3248S032 TFT (3.2" 240×320 rezistif touch)
**Çevre birimleri:** TCA9548A I2C mux · DS3231 RTC · SHT40 sıcaklık/nem · PCF8574 IO genişletici · DRV8825 step sürücü · PAM8403 ses · 4-kanal röle

---

## I2C Bus

| Master | → | Slave | Notlar |
|---|---|---|---|
| ESP32 I2C konnektör (IO27=SDA, IO22=SCL) | → | TCA9548A (ana I2C bus) | `SDA_MAIN` / `SCL_MAIN` |
| ESP32 I2C konnektör 3V3 | → | TCA9548A.VIN + tüm I2C slave VCC'leri | `+3V3` |
| TCA9548A CH0 (SD0/SC0) | → | SHT40 (J2.2=SDA / J2.3=SCL) | `SDA_CH0` / `SCL_CH0` — sıcaklık/nem, kablolu sensör |
| TCA9548A CH1 (SD1/SC1) | → | DS3231 (RTC) | `SDA_CH1` / `SCL_CH1` |
| TCA9548A CH2 (SD2/SC2) | → | PCF8574 (IO genişletici) | `SDA_CH2` / `SCL_CH2` |

### I2C Pull-Up (ana bus → 3V3)

| Ref | Değer | Pin | Not |
|---|---|---|---|
| `R1` | 4.7k | SDA → 3V3 | I2C master bus pull-up |
| `R2` | 4.7k | SCL → 3V3 | I2C master bus pull-up |

### TCA9548A Adres Pinleri (varsayılan 0x70)

| Pin | Bağlantı | Not |
|---|---|---|
| `~RST` | `+3V3` | sürekli aktif, sabit jumper |
| `A0` | `GND` | adres bit 0 |
| `A1` | `GND` | adres bit 1 |
| `A2` | `GND` | adres bit 2 |

---

## PCF8574 IO Atamaları

| PCF8574 Pin | Net | Hedef | İşlev |
|---|---|---|---|
| `P0` | `RLY_IN1` | J4.3 → Röle IN1 | **ISITICI** |
| `P1` | `RLY_IN2` | J4.4 → Röle IN2 | **NEMLENDİRİCİ** |
| `P2` | `RLY_IN3` | J4.5 → Röle IN3 | **FAN** |
| `P3` | `RLY_IN4` | J4.6 → Röle IN4 | **IŞIK** |
| `P4` | `DRV_EN` | DRV8825 EN | motor enable |
| `P5` | `DRV_STEP` | DRV8825 STEP | step pulse |
| `P6` | `DRV_DIR` | DRV8825 DIR | yön |
| `P7` | — | yedek | (kapak switch için önerilir) |

---

## Step Motor (NEMA17 17HS4401)

### DRV8825 Konfigürasyonu

| Pin | Bağlantı | Mod |
|---|---|---|
| `M0` | `GND` | mikroadım = 1 (Full Step) |
| `M1` | `GND` | |
| `M2` | `GND` | |
| `~RST` | `+3V3` | sürekli aktif jumper |
| `~SLP` | `+3V3` | sürekli aktif jumper |
| `VMOT` | `+12V_VMOT` | PS2 (Mini360 #2) çıkışı |
| `GND_MOT` / `GND_LOGIC` | `GND` | ortak toprak |

### Motor Çıkışları

| DRV8825 | Net | J1 (JST 4p) | NEMA17 |
|---|---|---|---|
| `B2` | `MOTOR_B2` | J1.1 | B- (Mavi) |
| `B1` | `MOTOR_B1` | J1.2 | B+ (Kırmızı) |
| `A1` | `MOTOR_A1` | J1.3 | A- (Yeşil) |
| `A2` | `MOTOR_A2` | J1.4 | A+ (Siyah) |

---

## Ses

| ESP32 | → | PAM8403 |
|---|---|---|
| `SPK+` (pin 17) | `AUDIO_L` | `IN_L` (line-level ses) |
| `UART_5V` | `+5V` | `VCC_5V` |
| `UART_GND` | `GND` | `IN_GND` / `GND` |

---

## Röle (4 kanal, 220V AC çıkışlar)

| J4 Pin | Net | Kaynak |
|---|---|---|
| `J4.1` | `+5V` | ESP32 UART_5V |
| `J4.2` | `GND` | ESP32 GND |
| `J4.3` | `RLY_IN1` | PCF8574 P0 |
| `J4.4` | `RLY_IN2` | PCF8574 P1 |
| `J4.5` | `RLY_IN3` | PCF8574 P2 |
| `J4.6` | `RLY_IN4` | PCF8574 P3 |

**Röle COM/NO/NC** → 220V şebeke: ısıtıcı / nemlendirici / fan / ışık yükleri.

> ⚠️ **PCB güvenliği**: Röle modülünün 220V tarafı ile düşük voltaj devresi arasında **min. 8mm creepage** mesafesi bırakılmalı (PCB'de slot/yarık önerilir).

---

## Güç Giriş Zinciri (24V DC → 5V / 12V)

```
J5 (DC_Jack 5.5×2.1, +24V)
  └── F1 (Polyfuse 3A_PTC)
        └── D2 (P6KE24CA TVS shunt, +24V_RAW ↔ GND)
              └── D1 (D_Schottky SS34, ters polarite koruma)
                    └── C6 (CP_THT 100uF/35V bulk)
                          └── +24V_PROT rayı
                                ├── PS1 (Mini360 #1, 24V→5V)
                                │     └── C7 (C_THT 100nF) → +5V
                                │           ├── ESP32 UART_5V
                                │           ├── Röle VCC
                                │           └── PAM8403 VCC
                                └── PS2 (Mini360 #2, 24V→12V)
                                      └── C8 (CP_THT 100uF/25V) → +12V_VMOT
                                            ├── DRV8825 VMOT
                                            └── J3 (opsiyonel harici giriş/çıkış, DNP edilebilir)

J5.sleeve → GND (tüm modüllerin ortak toprağı)
```

### Mini360 Kalibrasyonu (Önemli!)

| Modül | Hedef Çıkış | Trimpot Ayarı |
|---|---|---|
| `PS1` | **5.0 V** | Multimetre ile dikkatli ayarlayın |
| `PS2` | **12.0 V** | DRV8825'i yakmamak için aşmayın |

> 🔧 Fabrika varsayılanı ≈ 17V → ilk takmadan önce yükten ayırıp ayarlayın.

### Akım Bütçesi

| Yük | Tipik | Maks |
|---|---|---|
| ESP32 + TFT (5V) | 400 mA | 800 mA |
| Röle modülü (5V) | 80 mA | 200 mA |
| PAM8403 + hoparlör (5V) | 200 mA | 600 mA |
| **PS1 (5V toplam)** | **~700 mA** | **~1.6 A** (sınır 1.8A) |
| DRV8825 + NEMA17 (12V) | 600 mA | 1.5 A/faz × 2 |
| **PS2 (12V toplam)** | **~600 mA** | **~3 A** (sınırda — Vref'i 1.2A/faz'a kıs!) |

---

## Decoupling Kapasitörleri (100nF her modül VCC ↔ GND)

| Ref | Modül | Konum |
|---|---|---|
| `C1` | TCA9548A | (110, 180) |
| `C2` | PCF8574 | (220, 230) |
| `C3` | DS3231 | (15, 220) |
| `C4` | PAM8403 | (300, 90) |
| `C5` | DRV8825 | (295, 195) |

Bulk kapasitörler:
- `C6` 100uF/35V — `+24V_PROT` bulk
- `C7` 100nF — `+5V` çıkış
- `C8` 100uF/25V — `+12V_VMOT` bulk (DRV8825 transient absorpsiyon)

---

## Harici Kablo Bağlantıları (JST-XH)

| Konektör | Pin | Hedef |
|---|---|---|
| **J1** (4p) | A1/A2/B1/B2 | NEMA17 motor kablosu |
| **J2** (4p) | VCC/SDA/SCL/GND | SHT40 kablolu sensör (Kırmızı/Beyaz/Sarı/Siyah) |
| **J3** (2p) | +12V_VMOT / GND | Opsiyonel harici 12V (PS2 paralel — DNP edilebilir) |
| **J4** (6p) | VCC/GND/IN1-IN4 | Röle modül kontrol kablosu |
| **J5** (DC Jack 5.5×2.1) | +24V / GND | Ana güç girişi |

---

## Net Özeti (KiCad ERC için)

### Güç netleri
`+24V_RAW`, `+24V_FUSED`, `+24V_PROT`, `+12V_VMOT`, `+5V`, `+3V3`, `GND`

### Sinyal netleri
`SDA_MAIN`, `SCL_MAIN`, `SDA_CH0`, `SCL_CH0`, `SDA_CH1`, `SCL_CH1`, `SDA_CH2`, `SCL_CH2`,
`MOTOR_A1`, `MOTOR_A2`, `MOTOR_B1`, `MOTOR_B2`,
`RLY_IN1`–`RLY_IN4`, `DRV_EN`, `DRV_STEP`, `DRV_DIR`, `AUDIO_L`

---

## ERC Açıklığı (Beklenen Uyarılar)

- ESP32 yan pinleri (`BAT+/-`, `EXP_IO35/39`, `SPI hattı`, `UART RX/TX`) henüz etiketsiz → `Place → No Connection Flag` ile NC işaretleyin
- DRV8825 `~FLT` çıkışı NC (opsiyonel: PCF8574 `P7`'ye bağlanabilir)
- DS3231 `32K`, `SQW`, `VCC_2`, `GND_2`, `SDA_2`, `SCL_2` ikincil pinleri NC
- PAM8403 `IN_R`, `R+`, `R-` (mono kullanım) NC
- DS3231/PCF8574'ün TCA9548A geçişli alt SDA/SCL'leri yerel hatlardır — slave SCL pinleri input olarak işaretli, master output beklemez → ERC bildirebilir ama elektrik doğru

---

*Bu dosya `KuluckaMakinesi.kicad_sch` içindeki text bloğundan türetilmiştir. Şematik değişirse senkron tutun.*
