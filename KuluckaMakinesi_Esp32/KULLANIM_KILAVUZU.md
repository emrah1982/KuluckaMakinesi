
## Icindekiler

1. [Sistem Genel Bakis](#1-sistem-genel-bakis)
2. [Donanim Baglantisi](#2-donanim-baglantisi)
3. [Ilk Kurulum ve Yapilandirma](#3-ilk-kurulum-ve-yapilandirma)
4. [Kulucka Baslatma](#4-kulucka-baslatma)
5. [TFT Ekran Arayuzu](#5-tft-ekran-arayuzu)
6. [Web Arayuzu](#6-web-arayuzu)
7. [Hayvan Profilleri](#7-hayvan-profilleri)
8. [Sicaklik Gradyani (Kademeli Sogutma)](#8-sicaklik-gradyani)
9. [PID Kontrolcu ve Auto-Tuning](#9-pid-kontrolcu-ve-auto-tuning)
10. [Sensor Kalibrasyonu](#10-sensor-kalibrasyonu)
11. [Guc Kesintisi Kurtarma](#11-guc-kesintisi-kurtarma)
12. [Guvenlik Sistemi](#12-guvenlik-sistemi)
13. [Role Asınma Korumasi](#13-role-asinma-korumasi)
14. [Fan PWM Kontrolu](#14-fan-pwm-kontrolu)
15. [RTC (Gercek Zamanli Saat) Yonetimi](#15-rtc-gercek-zamanli-saat-yonetimi)
16. [Alarm Sistemi](#16-alarm-sistemi)
17. [Dol Kontrolu (Candling) Sistemi](#17-dol-kontrolu-candling-sistemi)
18. [Sensor Grafikleri ve Veri Gecmisi](#18-sensor-grafikleri-ve-veri-gecmisi)
19. [OTA Firmware Guncelleme](#19-ota-firmware-guncelleme)
20. [Sorun Giderme](#20-sorun-giderme)
21. [Teknik Referans](#21-teknik-referans)

---

## 1. Sistem Genel Bakis

Bu sistem, ESP32 tabanli tam otomatik bir kulucka makinesi kontrolcusudur. Yumurtanin kulucka makinesine konulmasindan cikim gerceklesene kadar tum sureci yonetir.

**Temel Ozellikler:**
- Cift SHT30 sensor ile hassas sicaklik/nem olcumu (fuzyon + yedekleme)
- PID kontrolcu ile otomatik sicaklik ayarlama (Ziegler-Nichols auto-tuning)
- Faz bazli sicaklik gradyani destegi (kademeli sogutma)
- 11 onceden tanimli hayvan profili + ozel profil olusturma
- 3.2" TFT dokunmatik ekran + web arayuzu (WiFi)
- Guc kesintisi kurtarma (NVS kalici depolama)
- Cok katmanli guvenlik sistemi
- Role asınma korumasi (mekanik omur uzatma)
- Sensor kalibrasyon destegi (referans termometre ile)
- OTA firmware guncelleme — iki mod: yerel `.bin` yukleme + internetten otomatik indirme (SHA256 dogrulamali)
- Cihaz kimligi (deviceId + kullanici tanimli isim) - cok cihazli kurulum destegi

---

## 2. Donanim Baglantisi

### Pin Haritasi

| Fonksiyon | ESP32 Pin | Aciklama |
|-----------|-----------|----------|
| I2C SDA | IO32 | Tum I2C cihazlari (MUX uzerinden) |
| I2C SCL | IO25 | Tum I2C cihazlari (MUX uzerinden) |
| Fan PWM | IO18 | L298N motor surucu (25kHz) |
| TFT Arka Isik | IO27 | Ekran parlakligi |
| Dokunmatik CS | IO33 | XPT2046 dokunmatik panel |
| Dokunmatik IRQ | IO36 | Dokunma algilama |

### I2C Bus Yapisi (Grove - 8 Channel I2C Multiplexer, TCA9548A - 0x70)

```
ESP32 (IO32/IO25)
  |
  +-- Grove 8 Kanal I2C MUX (0x70)
       |
       +-- CH0: DS3231 RTC (0x68) - Gercek zamanli saat
       +-- CH1: SHT40 (0x44) - Birincil sicaklik/nem sensoru
       +-- CH2: SHT30 (0x44) - Ikincil sensor (yedek + fuzyon)
       +-- CH3: SCD30 (0x61) - CO2 sensoru
       +-- CH4: MLX90614ESF-BCC (0x5A) - Yumurta IR sicaklik (yerel, birincil)
       +-- CH7: PCF8574 (0x20) - 4'lu role karti
                 P0 = Isitici rolesi
                 P1 = Nemlendirici rolesi
                 P2 = Yumurta cevirme motor guc
                 P3 = Yumurta cevirme motor yon
```

**MUX adresi hakkinda:** TCA9548A'nin adresi A0/A1/A2 pinleri ile belirlenir.
Hepsi GND'ye bagliyken taban adres **0x70**'tir; A0 = +1, A1 = +2, A2 = +4 ile
0x77'ye kadar cikabilir. Grove kartinda bu pinler varsayilan olarak GND'dedir,
bu yuzden kart uzerinde yazan deger 0x70'tir.

Adres lehimleri degistirilse veya farkli bir kart takilsa bile firmware'i
duzenlemeye gerek yoktur: `I2CMux::begin()` once 0x70'i dener, bulamazsa
0x70-0x77 araligini tarar. Bulunan her aday, kontrol registerine yazip geri
okuyarak gercekten multiplexer oldugu dogrulanir — boylece ayni aralikta
bulunabilecek baska bir cihaz (ornegin 0x76/0x77'deki BMP280/BME280) yanlislikla
MUX sanilmaz. Acilista seri monitorde su satir gorunur:

```
[MUX] Grove 8 kanal MUX bulundu: 0x70
```

Adres 0x70 disinda bir yerde bulunursa uyari satiri basilir ama sistem yine de
calisir. Hicbir sey bulunamazsa `[MUX] HATA: ... bulunamadi` yazar; bu durumda
kablo ve besleme kontrol edilmelidir.

### Isitici Baglantisi
Isitici, PCF8574'un P0 pini uzerinden role ile kontrol edilir. PID cikisi (0-255) **time-proportional** yontemiyle roleye cevirilir: 10 saniyelik pencere icinde ON/OFF orani ayarlanir.

Ornek: PID cikisi = 128 → 5sn ACIK, 5sn KAPALI

### Yumurta Cevirme Motoru
Iki role ile H-koprusu mantigi:
- Role 3 (P2): Motor guc ON/OFF
- Role 4 (P3): Motor yon (OFF=Ileri, ON=Geri)

Varsayilan: Her 1 saatte bir, 5 saniye calisir, yon otomatik degisir.

### Yumurta IR Sicakligi (MLX90614ESF-BCC)

Yumurta yuzey sicakligi temassiz olcum ile takip edilir. Iki kaynak vardir ve
sistem otomatik olarak birinden digerine gecer:

| Oncelik | Kaynak | Baglanti | Modul |
|---------|--------|----------|-------|
| 1 (birincil) | MLX90614ESF-BCC | Grove MUX **CH4**, adres 0x5A | `EggIRSensor.*` |
| 2 (yedek) | Ayri ESP32 uzerindeki IR sensor | WiFi + HTTP (`/api/egg`) | `EggTempService.*` |

Yerel sensor gecerli veri verdigi surece uzak servis okunmaz. Yerel sensor
takili degilse veya art arda `EGG_IR_FAIL_LIMIT` (5) hata verirse, sistem
sessizce WiFi yedegine duser. Ikisi de yoksa ekranda "Baglanti yok" gorunur.

Aktif kaynak durum JSON'unda `eggTempSource` alaninda bildirilir:
`0` = kaynak yok, `1` = yerel MLX90614, `2` = uzak WiFi servisi. Web arayuzundeki
yumurta kartinda rozet olarak "IR Yerel" / "IR Yedek" seklinde gosterilir.

**Baglanti:** Modulun VCC/GND/SDA/SCL uclari MUX'un 4 numarali Grove soketine
baglanir. Sensor yumurtalara 5-10 cm mesafeden, dogrudan yuzeyi gorecek sekilde
konumlandirilmalidir; arada tel kafes veya cam olmamalidir.

**Kalibrasyon:** Sensorun fabrika emisivite ayari 1.00, yumurta kabugu ise ~0.95
emisiviteye sahiptir. Kulucka icinde yumurta ve ortam sicakligi birbirine cok
yakin oldugundan bu farkin etkisi kucuktur. Kalan sapmayi duzeltmek icin
`Config.h` icindeki `EGG_IR_TEMP_OFFSET` degeri kullanilir (sensorun EEPROM'una
yazma yapilmaz). Referans termometreyle karsilastirip farki bu sabite girin.

**Kabuk sicakligi alarmlari:** Kabuk sicakligi embriyonun gercek sicakligina en
yakin olcumdur; ortam sicakligi bunun sadece dolayli gostergesidir. Bu yuzden
ayri alarm esikleri vardir:

| Alarm | Kosul | Varsayilan |
|-------|-------|------------|
| `ALARM_EGG_TEMP_HIGH` | kabuk > hedef + `EGG_TEMP_TOLERANCE_HIGH` | +1.0 C |
| `ALARM_EGG_TEMP_LOW` | kabuk < hedef - `EGG_TEMP_TOLERANCE_LOW` | -2.5 C |
| `ALARM_EGG_SENSOR_LOST` | Her iki kaynak da `EGG_SOURCE_LOST_MS` boyunca yok | 2 dk |

`EGG_TEMP_ALARM_MIN` (30 C) altindaki olcumlerde alarm uretilmez — kapak acik
veya yumurtalar yeni yerlestirilmis olabilir.

> **Neden alt tolerans daha genis?** Kulucka'nin ilk gunlerinde embriyo henuz
> isi uretmedigi icin kabuk sicakligi hedefin altinda seyreder; son gunlerde
> metabolik isi ile hedefin ustune cikar. Dar bir alt tolerans ilk gunlerde
> surekli yanlis alarm uretir. Ust sinir ise dar tutulmustur: asiri isinma
> embriyoyu saatler icinde oldurur.

Kabuk sicakligi su an **alarm ve izleme** amaclidir; PID setpoint'ini dogrudan
etkilemez. Sicaklik kontrolu hala ortam sensorlerinden (SHT40/SHT30) yurutulur.

**Sensor kaybinda ne olur?** Yerel MLX90614 gecersiz olursa WiFi yedegine
dusulur; ikisi de yoksa 2 dakika sonra `ALARM_EGG_SENSOR_LOST` tetiklenir.
Alarm yalnizca daha once calisan bir kaynak kaybolursa uretilir — hic sensor
takilmamis bir sistemde surekli alarm calmaz.

---

## 3. Ilk Kurulum ve Yapilandirma

### Arduino IDE Ayarlari
1. **Kart:** ESP32 Dev Module
2. **Partition Scheme:** Huge APP (3MB No OTA / 1MB SPIFFS)
3. **Kutuphaneler:**
   - RTClib (Adafruit)
   - ESPAsyncWebServer (mathieucarbou)
   - AsyncTCP (mathieucarbou)
   - TFT_eSPI

### TFT_eSPI Yapilandirma
`TFT_Setup.h` dosyasindaki pin tanimlarini TFT_eSPI kutuphanesinin `User_Setup.h` dosyasina kopyalayin veya `User_Setup_Select.h` icinden referans verin.

### WiFi Ayarlari (Captive Portal ile Otomatik Kurulum)

**Onerilen yontem:** Cihazi `Config.h` dosyasini hic degistirmeden yukleyin. Sistem otomatik olarak AP (Hotspot) modunda baslar ve **captive portal** ile yapilandirma sayfasi acilir.

**Kullanici akisi:**
1. ESP32'ye guc verin → AP yayina baslar (`Kulucka-<MAC8>`, sifre: `kulucka123`)
2. Telefon/laptopla bu AP'ye baglanin
3. Tarayici **otomatik olarak** WiFi kurulum sayfasina yonlenir
   - iOS: "Sign in to network" notifikasyonu
   - Android: "Sign in to WiFi network" bildirimi
   - Windows: "Action needed" → tarayici acilir
   - Manuel: `http://192.168.4.1/portal`
4. Kurulum sayfasinda:
   - "Aglari Tara" butonu → etraftaki WiFi'leri listeler
   - Bir agi tiklayin, sifresini girin, "Baglan"
5. ESP32 reboot olmadan baglanir, basarili olunca sayfa otomatik yeni IP'ye yonlenir
6. Bilgi NVS'e kaydedilir → guc kesintisi sonrasi otomatik baglanir

**Manuel Config.h yontemi (opsiyonel):**
```
WIFI_SSID = "EvAgim"
WIFI_PASSWORD = "evsifresi"
```
Bu yontemde portal acilmaz; ESP32 dogrudan belirtilen aga baglanir.

**Alternatif:** `Config.h` icinde varsayilan placeholder degerleri (`"WIFI_ADI"` / `"WIFI_SIFRE"`) bos veya placeholder olarak kalirsa AP+captive portal calisir:
- Ag Adi: `Kulucka-<MAC8>` (ornek: `Kulucka-A4C138F2`) — her cihazda farklidir, yan yana cihazlar cakismaz
- Sifre: `kulucka123` (Config.h `AP_PASSWORD`)
- IP: `192.168.4.1`
- mDNS: `kulucka-<mac8>.local` (ornek: `kulucka-a4c138f2.local`)

WiFi bilgilerini web arayuzunden de degistirebilirsiniz (`POST /api/wifi`).

### mDNS ile Erisim

Cihaz hem AP hem STA modunda mDNS yayini yapar. IP adresi ezberlemek yerine tarayicidan dogrudan:
```
http://kulucka-a4c138f2.local
```
adresini kullanabilirsiniz. Ayni agdaki Windows/macOS/Linux cihazlar bu adresi cozumler.

Ayrica `_kulucka._tcp` mDNS servisi yayinlanir (TXT kayitlari: `id`, `name`, `fw`); merkezi bir panel veya tarama scripti bu servis turunu sorgulayarak agdaki tum kulucka makinelerini otomatik kesfedebilir.

---

## 4. Kulucka Baslatma

### Adim Adim

1. **Profil Secin:** TFT ekranda "Profil" sekmesinden hayvan turunu secin
2. **Baslat'a Basin:** "Kontrol" sekmesinden "BASLAT" butonuna dokunun
3. **Auto-Tune Bekleyin:** Sistem otomatik olarak PID parametrelerini ayarlar (~30 dk)
4. **Calisma Modu:** Auto-tune tamamlaninca sistem otomatik olarak normal calismaya gecer

### Durum Gecisleri

```
INITIALIZING --> AUTOTUNING --> RUNNING --> COMPLETED
      |              |             |
      +--> PAUSED <--+             +--> EMERGENCY
             |                          |
             +-- RESUME --> RUNNING     +-- RESET --> PAUSED
```

- **PAUSED:** Tum cikislar kapatilir. Alarm kontrolu calismaya devam eder.
- **EMERGENCY:** Guvenlik ihlali. Isitici + nemlendirici KAPALI, fan MAKSIMUM. Manuel reset gerekir.
- **COMPLETED:** Kulucka suresi doldu. Tebrikler!

---

## 5. TFT Ekran Arayuzu

### Sekmeler

| Sekme | Icerik |
|-------|--------|
| **Durum** | Sicaklik/nem gosterge, hedef degerler, gun/evre bilgisi, cikis durumlari |
| **Grafik** | Son 5 dakikanin sicaklik ve nem grafigi (canli) |
| **Kontrol** | Baslat / Duraklat / Devam Et / Durdur butonlari |
| **Profil** | Hayvan profili secimi, evre detaylari (sicaklik gradyani dahil) |
| **Ayar** | WiFi bilgileri, IP adresleri, sistem bilgisi |

### Profil Sekmesinde Gradyan Gosterimi
Sicaklik gradyani olan evrelerde TFT'de su sekilde goruntulenir:
```
Sicaklik: 37.8->37.5 C    (gradyanli)
Sicaklik: 37.2 C           (sabit)
```

---

## 6. Web Arayuzu

Tarayicinizdan ESP32'nin IP adresine baglanin. AP modunda: `http://192.168.4.1`

### API Endpointleri

| Endpoint | Yontem | Aciklama |
|----------|--------|----------|
| `/api/status` | GET | Sistem durumu (JSON) |
| `/api/control` | POST | Baslat/Duraklat/Devam/Durdur |
| `/api/pid` | POST | PID parametreleri ayarla (kp, ki, kd) |
| `/api/humidity` | POST | Nem esikleri ayarla (low, high) |
| `/api/profile` | POST | Profil sec (index) |
| `/api/calibration` | GET | Sensor kalibrasyon degerlerini oku |
| `/api/calibration` | POST | Sensor kalibrasyon offset kaydet |
| `/api/customprofiles` | GET | Ozel profilleri listele |
| `/api/customprofile` | POST | Ozel profil ekle/guncelle |
| `/api/customprofile` | DELETE | Ozel profil sil |
| `/api/wifi` | POST | WiFi bilgilerini guncelle |
| `/api/settings` | POST | Tum ayarlari NVS'ye kaydet |
| `/api/log` | GET | Veri logunu CSV olarak indir |
| `/api/log` | DELETE | Log'u temizle |
| `/api/alarm` | GET | Alarm durumu (JSON) |
| `/api/alarm/ack` | POST | Alarmi sustur (10 dakika) |
| `/api/alarm/snooze` | POST | Alarmi ertele (`ms=<sure>` opsiyonel, default: 1 saat) |
| `/api/alarm/dismiss` | POST | Alarmi kapat (CANDLING: 24 sa sustur, diger: temizle) |
| `/api/history` | GET | Sicaklik/Nem/CO2 gecmis verileri (60 okuma, 5 sn aralik) |
| `/api/phase-log` | GET | Faz gecis gecmisi (hangi faza ne zaman girildi, sure) |
| `/api/time` | POST | Cihaz tarih/saat ayarla (form: `date=YYYY-MM-DD&time=HH:MM` veya `unix=<sec>`) |
| `/api/identity` | GET | Cihaz kimligi (deviceId, isim, FW versiyonu, MAC, AP SSID, mDNS) |
| `/api/device-name` | POST | Kullanici tanimli cihaz ismini gunceller (`name` parametresi) |
| `/api/wifi/scan` | GET | Etraftaki WiFi aglarini tara. `?refresh=1` yeni tarama tetikler |
| `/api/wifi/status` | GET | STA baglanti durumu (portal polling icin) |
| `/api/wifi/connect` | POST | Kaydet + bagla (reboot olmadan). Parametreler: `ssid`, `pass` |
| `/portal` | GET | Captive portal kurulum sayfasi (WiFi tarama + secim + baglanma) |
| `/update` | GET | Yerel OTA — `.bin` dosyasi yukleme arayuzu (push) |
| `/ota` | GET | Internet uzerinden OTA — version.json'dan otomatik indirme arayuzu (pull) |
| `/api/ota/status` | GET | Pull-OTA durumu: state, progress, remoteVersion, changelog |
| `/api/ota/check` | POST | version.json'u indirip semver ile kendi surumunu karsilastirir |
| `/api/ota/pull` | POST | .bin'i indirir, SHA256 dogrular, flash + reboot |
| `/api/ota/url` | POST | version.json adresini kalici NVS'e kaydeder (form: `url=...`) |

### Cihaz Kimligi (deviceId / deviceName)

Yan yana birden fazla kulucka makinesi calistiracaksaniz her cihaz benzersiz `deviceId` ile dogar:
- **deviceId:** ESP32 MAC adresinden turetilir, sabit. Format: `KM-A4C138F2`
- **deviceName:** Kullanici tarafindan atanir (NVS'de saklanir). Yoksa `deviceId` kullanilir.
- **AP SSID:** `Kulucka-A4C138F2` (her cihazda farkli — yan yana cakismaz)
- **mDNS:** `kulucka-a4c138f2.local` (IP ezberlemeden tarayicidan erisim)

Cihaz ismi atama:
```
POST /api/device-name
name=Damizlik-Bildircin
```

`/api/status` ve `/api/identity` cevaplari `deviceId`, `deviceName`, `fwVersion` alanlari icerir; bu sayede merkezi bir panel veya tarama scripti her cihazi ayirt edebilir.

---

## 7. Hayvan Profilleri

Sistem 11 onceden tanimli profil icerir. Profil degerleri Japon (Showa Furanki),
Cin (Xinzhou WQ-48, Nanchang HHD) ve Avrupa (Brinsea Ova-Easy, Cimuka HB100)
profesyonel kuluckalarindan referans alinmistir.

| # | Hayvan | Sure | Gelisim | Cikim | Cevirme (dk/sn/aci) | Sogutma | Sprey |
|---|--------|------|---------|-------|----------------------|---------|-------|
| 0 | Tavuk | 21 gun | 37.8 -> 37.5 C | 37.2 C | 60 / 15 / 90° | - | - |
| 1 | Bildircin | 18 gun | 37.8 -> 37.5 C | 37.2 C | 45 / 8 / 90° | - | - |
| 2 | Kaz | 30 gun | 37.6 -> 37.2 C | 37.0 C | 90 / 45 / 45° | 2x/gun 20 dk | 8 sn |
| 3 | Ordek | 28 gun | 37.6 -> 37.2 C | 37.0 C | 60 / 25 / 90° | 1x/gun 15 dk | 5 sn |
| 4 | Hindi | 28 gun | 37.5 -> 37.2 C | 37.0 C | 60 / 20 / 90° | - | - |
| 5 | Sulun | 25 gun | 37.8 -> 37.5 C | 37.2 C | 60 / 10 / 90° | - | - |
| 6 | Guvercin | 19 gun | 37.5 C (sabit) | 37.2 C | 90 / 8 / 90° | - | - |
| 7 | Papagan | 26 gun | 37.0 C (sabit) | 36.7 C | 120 / 5 / 45° | - | - |
| 8 | Devekusu | 42 gun | 36.0 C (sabit) | 36.0 C | 180 / 60 / 45° | 2x/gun 30 dk | - |
| 9 | Ipek Bocegi | 42 gun | 4 evreli ozel | - | - | - | - |
| 10 | Ari Kovani | 21 gun | 34.5 - 35.0 C | - | - | - | - |

> **Aci kalibrasyonu (yazilim, varsayilan donanim):** Klasik DC/role motorda
> aci dogrudan olculmez; yerine **yazilim kalibrasyonu** kullanilir:
> `Config.h`'taki `TURNER_DEG_PER_SEC` (varsayilan **6 °/s**) profilden gelen
> aciya uygulanir ve donus suresi otomatik hesaplanir
> (`durationSec = angleDeg / TURNER_DEG_PER_SEC`).
> Profile `turningDurationSec=0` verilirse aci hesabi devreye girer; aksi halde
> dogrudan sure kullanilir. Kendi mekaniginiza gore `TURNER_DEG_PER_SEC`'i bir
> kere olcup yazin (90°'lik donus saniye olarak / 90).

> **Not (Sogutma/Sprey simulasyonu):** Mevcut 4-role donanim tam dolu oldugu icin
> sogutma ve sprey ek role gerektirmeden mevcut cikislarla simule edilir:
> - **Sogutma:** Isitici 0 + fan %100 PWM (kapak acilmasina esdeger hava akisi)
> - **Sprey:** Nemlendirici zorla ON (cooling sonrasi nem atakligi)
> Slot saatleri: `coolingPerDay=1` -> 12:00, `coolingPerDay=2` -> 08:00 ve 20:00.
> Pencere 5 dakika; RTC o pencerede aktive eder.

### Ozel Profil Olusturma
Web arayuzunden `POST /api/customprofile` ile en fazla 10 ozel profil tanimlanabilir.
Her profilde en fazla 4 evre; her evrede:
- Baslangic/bitis sicakligi (gradyan), nem araligi, cevirme durumu
- **V3 alanlari:** cevirme araligi (dk), cevirme suresi (sn), **cevirme acisi (deg)**,
  sogutma aktif/sure/gunluk slot, sprey aktif/sure

Ornek: Kendi kaz profiliniz (profesyonel sogutma + sprey + 45° aci ile)
```
p1_start=10&p1_end=25&p1_temp=37.4&p1_tempEnd=37.2
  &p1_humLow=55&p1_humHigh=65&p1_turning=1
  &p1_turnIntMin=90&p1_turnDurSec=45&p1_turnAngleDeg=45
  &p1_cool=1&p1_coolMin=20&p1_coolPerDay=2
  &p1_spray=1&p1_spraySec=8
  &p1_name=Sogutma
```

`turnDurSec=0` gonderirseniz ve `turnAngleDeg>0` ise sure aciya gore otomatik
hesaplanir (bkz. yukaridaki kalibrasyon notu).

### NVS Schema Versiyonu
Custom profil formatinin versiyonu `kulucka/cpSchemaVer` NVS anahtarinda tutulur:
- **v1:** Faz 8 alan (sadece turning aktif/pasif)
- **v2:** Faz 14 alan (turning interval/duration + cooling/spray eklendi)
- **v3:** Faz 15 alan (turning **acisi** eklendi — 2026-04-22)

Cihaz eski sürümleri ilk acilista otomatik olarak en güncel sema versiyonuna
tasir — yeni alanlara mantikli varsayilan deger uygular (aci icin
`TURNER_DEFAULT_ANGLE_DEG = 90°`) ve yeniden yazar. Migration idempotent;
tekrar calismaz.

### Yumurta Cevirme Motor Tipi (DC/Role vs Step Motor)

`Config.h` icinde `TURNER_TYPE` derleme zamani anahtariyla iki secim:

| TURNER_TYPE | Surucu | Donanim | Aci hassasiyeti |
|-------------|--------|---------|------------------|
| **0** (varsayilan) | `TurnerDriver` (DC/role) | PCF8574 P2 (guc), P3 (yon) | Yazilim kalibrasyonu (sure × hiz) |
| **1** | `StepperTurnerDriver` | PCF8574 P4=STEP, P5=DIR, P6=EN + A4988/DRV8825 + NEMA17 | Donanim — gercek aci |

**Step motor (Secenek 1) baglantisi:**

```
ESP32 → Grove 8 Kanal I2C MUX (CH7) → PCF8574 (0x20)
PCF8574 P4 → A4988 STEP
PCF8574 P5 → A4988 DIR
PCF8574 P6 → A4988 ENABLE (aktif-LOW: bos vakitte motor enerjisiz)
A4988 VMOT/GND → 12V harici besleme + 100µF kondansator
A4988 MS1/MS2/MS3 → GND (full-step; 200 step/devir)
A4988 1A/1B, 2A/2B → NEMA17 4-pin (bobin sırası dikkat)
A4988 RESET ↔ SLEEP (kisa devre)
A4988 VDD → 3.3V veya 5V (logic guc)
```

**Step motor parametreleri (`Config.h`):**
- `STEPPER_STEPS_PER_REV` = 200 (NEMA17 standart)
- `STEPPER_MICROSTEP` = 1 (full-step, MS pinleri GND)
- `STEPPER_GEAR_RATIO` = 5 (mekanik yumurta tepsi disli orani — kendi mekaniginize göre ayarlayin)
- `STEPPER_STEP_PERIOD_US` = 10 000 (1 ms/adim → ~100 adim/s)
- `STEPPER_PULSE_WIDTH_US` = 100
- `STEPPER_DISABLE_AT_IDLE` = 1 (bos vakitte motor sogur, akim cekmez)

**Hesap ornegi:** 90° donus icin `(200 × 1 × 5) ÷ 360 × 90 = 250 adim`,
adim periyodu 10 ms ile **2.5 saniye** surer. Aci profilden geldigi icin
turner mekanikten bagimsiz tutarli.

**I2C bant genisligi notu:** PCF8574 her STEP icin ~250 µs I2C transaction
maliyetiyle calisir, bu yuzden pratikte ust hiz ~250 adim/s civari. Yumurta
cevirme icin fazlasiyla yeterli; daha hizli motor isteniyorsa GPIO direkt baglanti
gerekir (gelecekteki donanim revizyonu).

**Mevcut donanima geri donus:** `TURNER_TYPE 0` ile derlemek mevcut DC/role
sistemini etkilemez. P4-P7 hala bos kalir (alarm zili, ek isitici, fazladan
fan vb. icin kullanilabilir).

---

## 8. Sicaklik Gradyani

### Neden Gerekli?
Dogada ana kus, kuluckanin ilk gunlerinde yumurtalarin ustune daha sicak oturur, son gunlere dogru sicaklik dogal olarak hafif duser. Bu kademeli dusus embriyo gelisimine olumlu etki eder. Sabit sicaklik da calisir, ancak gradyan daha dogal ve basarili sonuclar verir.

### Nasil Calisir?
Her evrede iki sicaklik degeri tanimlanabilir:
- `temperature`: Evrenin baslangic sicakligi
- `tempEnd`: Evrenin bitis sicakligi (0 = gradyan yok, sabit)

Sistem, bulunulan gune gore bu iki deger arasinda **lineer interpolasyon** yapar.

**Ornek: Tavuk Gelisim Evresi (Gun 1-18)**
```
Gun  1: 37.80 C (baslangic)
Gun  5: 37.73 C
Gun  9: 37.65 C
Gun 13: 37.58 C
Gun 18: 37.50 C (bitis)
```

Bu hesaplama her gun otomatik guncellenir ve PID kontrolcuye hedef olarak verilir.

---

## 9. PID Kontrolcu ve Auto-Tuning

PIDController siniifi, kuluckanin sicaklik kontrolunden sorumludur ve **endustri standardi PID iyilestirmelerini** uygular: time-based hesap, derivative on measurement, conditional anti-windup, setpoint ramping, output slew limit, derivative LPF ve bumpless transfer.

### Auto-Tuning (Ziegler-Nichols)
Kulucka baslatildiginda sistem otomatik olarak isitici karakteristiklerini olcer:
1. Isiticiyi sabit gucte calistirir
2. 6 sicaklik salınım dongusu gozlemler (~30 dakika)
3. Salinim genliginden ve periyodundan optimal Kp, Ki, Kd hesaplar
4. Parametreleri NVS'ye kaydeder (bir sonraki guc kesintisinde tekrar kullanilir)

**Iyilestirilmis periyot olcumu:** Eski yontemde `totalTime / cycles` ile yaklasik bir Tu hesaplaniyordu. Yeni implementasyon her donguyu peak-to-peak olcer ve **ortalamasini** alir → daha hassas Ku/Tu degerleri.

### Tuning Kurali Secimi (4 Profil)

Auto-tune sonrasi Ku/Tu uzerinden hangi formul kullanilacagi secilebilir:

| Kural | Kp | Ki | Kd | Davranis | Onerilen |
|-------|------|------|------|----------|----------|
| `ZN_CLASSIC` | 0.6 Ku | 1.2 Ku/Tu | 0.075 Ku·Tu | Hizli, %20 overshoot | Hizli tepki gerekli |
| `ZN_PESSEN` ✅ | 0.7 Ku | 1.75 Ku/Tu | 0.105 Ku·Tu | Az overshoot (varsayilan) | Cogu durum |
| `TYREUS_LUYBEN` | 0.45 Ku | Kp/(2.2·Tu) | Kp·(Tu/6.3) | Conservative, kararlı | Salınımlı sistem |
| `NO_OVERSHOOT` | 0.2 Ku | 0.4 Ku/Tu | 0.067 Ku·Tu | Hicbir sicrama yok | **Yumurta icin onerilen** ⭐ |

```cpp
pid.setTuningRule(PID_TUNING_NO_OVERSHOOT);   // sonraki auto-tune'da uygulanir
pid.startAutoTune();
```

### Time-Based PID (Gercek dt)

Klasik implementasyon `error * sampleCount` kullanir, loop hizi degisirse davranis bozulur. Yeni kod **gercek dt'yi** (saniye cinsinden) kullanir:

```cpp
double dt = (now - _lastComputeMs) / 1000.0;
_integral += _ki * error * dt;     // gercek zamana orantili
double dInput = (input - _lastInput) / dt;
```

- Loop hizi 1Hz olsa da 10Hz olsa da PID davranisi ayni kalir
- Saat atlamalari (>60 sn) tespit edilip atlanir (saglik onlemi)

### Derivative on Measurement (D-Kick Onleme)

**Sorun:** Klasik formul `D = Kd * d(error)/dt` kullanir. Setpoint ani degisirse `error` siçrar → `D` term spike yapar → PWM patlar.

**Cozum:** `D = -Kd * d(input)/dt` (sadece olcumun turevini al)

```
Setpoint: 30°C -> 38°C (anlik degisim)
Eski D:  +Kd * 8 / dt   = +∞   (kotu!)
Yeni D:  -Kd * 0 / dt   = 0    (iyi: sicaklik henuz degismedi)
```

Sistem responsive ama **sıcrama yapmaz**.

### Conditional Anti-Windup

**Sorun:** Cikis 255'te (sature) ama hata hala pozitif → integral biriktirir → setpoint asilinca PWM gec dusuer (overshoot ve uzun yerlesim suresi).

**Eski cozum:** Klasik clamp (`integral_max = 100`)
**Yeni cozum:** Saturasyon yonu hata yonu ile ayni ise integrali biriktirme:

```cpp
if (output sature && error ayni yonde itiyor) {
    integral'i biriktirme;   // anti-windup proper
} else {
    integral'i guncelle;
}
```

Hizli yerlesim, daha az overshoot.

### Setpoint Ramping (Yumusak Gecis)

Ani setpoint degisimleri (faz gecisi 37.5°C → 37.2°C, kuluckadan cikima) sistemi zorlar. Ramping ile aktif setpoint kademeli yaklasir:

```cpp
#define PID_SETPOINT_RAMP_DEG_PER_SEC   0.05   // 1°C ~20 sn
```

`getActiveSetpoint()` ile anlik (ramped) setpoint okunabilir, `getSetpoint()` ile hedef.

```cpp
pid.setSetpointRampRate(0.1);  // 0.1 °C/sn (hizli)
pid.setSetpointRampRate(0.0);  // ramping kapali
```

### Output Slew Rate Limit (Role Omru)

PWM ani 0→255 atlar ise:
- Role kontaklari ark olusturur (omur kisalir)
- Isitici termal sok yasar
- Akim cekisi spike yapar

Slew limit her saniye max kac PWM degisebilecegini belirler:

```cpp
#define PID_OUTPUT_SLEW_PWM_PER_SEC   100.0   // 0->255 ~2.5 sn
```

```cpp
pid.setOutputSlewRate(50.0);   // daha yumusak
pid.setOutputSlewRate(0.0);    // limit yok (acil durumlar)
```

### Derivative LPF (Sensor Noise Filtresi)

Sicaklik sensoru titriyorsa (±0.1°C oscillation) D term agresif tepki verir → PWM titrer → role klikler. Low-pass filter D'yi yumusatir:

```cpp
#define PID_DERIVATIVE_LPF_ALPHA   0.7   // %70 onceki D korunur
```

```
alpha = 0.0  -> filtreleme yok (orijinal D)
alpha = 0.7  -> %70 onceki + %30 yeni (varsayilan, denge)
alpha = 0.95 -> agresif filtre (cok yumusak ama gec tepki)
```

### Bumpless Transfer (Manuel ↔ Otomatik)

Manuel PWM'den otomatige donuste cikis ani sicrama yapmaz. Integral, mevcut PWM degerine "seed" edilir:

```cpp
// Manuel: kullanici 150 PWM uyguluyor
pid.setManualOutput(150);
pid.setManualMode(true);
// ... 30 dakika boyunca manuel ...

// Otomatige donus
pid.setManualMode(false);
// PID compute eder ama integral=150 ile baslar -> PWM aniden 0'a dusmez
```

### Telemetri (Tanılama)

Sistemin ic durumunu izlemek icin:

```cpp
double iTerm = pid.getIntegralTerm();
double dTerm = pid.getDerivativeTerm();
bool   sat   = pid.isSaturated();           // PWM 0 veya 255 mi
double sp    = pid.getActiveSetpoint();     // ramped setpoint
```

Web/TFT'de gosterilebilir, log'lanabilir.

### Konfigurasyon (Config.h)

```cpp
#define PID_DEFAULT_KP                 20.0
#define PID_DEFAULT_KI                 0.8
#define PID_DEFAULT_KD                 5.0
#define PID_INTEGRAL_MIN              -100.0
#define PID_INTEGRAL_MAX               100.0
#define PID_OUTPUT_MIN                 0
#define PID_OUTPUT_MAX                 255

#define AUTOTUNE_CYCLES                6
#define AUTOTUNE_OUTPUT                200
#define AUTOTUNE_HYSTERESIS            0.2

// Gelismis davranis
#define PID_SETPOINT_RAMP_DEG_PER_SEC  0.05    // 1°C ~20 sn
#define PID_OUTPUT_SLEW_PWM_PER_SEC    100.0   // 0->255 ~2.5 sn
#define PID_DERIVATIVE_LPF_ALPHA       0.7     // %70 LPF
```

### Guc Kesintisinde PID
Guc kesilip geldiginde sistem, daha once kaydedilmis PID parametrelerini NVS'den yukler ve **auto-tuning'i atlar**. Bu sayede 30 dakikalik bekleme suresi olmaz. Yuklenen degerler guvenlik araligi kontrolunden (Kp: 0.1-200, Ki: 0-50, Kd: 0-100) gecirilir; bozuk deger tespit edilirse varsayilanlara donulur.

### Manuel PID Ayari
Web arayuzunden `POST /api/pid` ile kp, ki, kd degerleri manuel olarak ayarlanabilir. Bu degerler de NVS'ye kaydedilir.

### State Diyagrami

```
        +----------+  begin()  +-------------+
        |   IDLE   |---------->| AUTOTUNING  |
        +----------+           +-------------+
              ^                       |
              |                       | 6 dongu sonra
              | reset()               v
              |                +-------------+
              |                |   RUNNING   |<-+
              |                +-------------+  | setManualMode(false)
              |                       |          | (bumpless)
              |                       | setManualMode(true)
              |                       v          |
              |                +-------------+  |
              +----------------|   MANUAL    |--+
                               +-------------+
```

### Faydalari

- 🎯 **%30+ daha az overshoot** (Pessen Integral / No-Overshoot)
- 🛡️ **Sensor noise dayanıklılığı** (Derivative LPF)
- ⚡ **Daha az röle aşınması** (Output slew + setpoint ramping)
- 🔄 **Loop hızına immun** (time-based hesap)
- 📊 **Saturasyon sırasında stabil** (conditional anti-windup)
- 🎮 **Manuel↔otomatik geçişler yumuşak** (bumpless transfer)
- 🔬 **Detayli telemetri** (web'de izlenebilir)

---

## 10. Sensor Kalibrasyonu

### Neden Gerekli?
SHT30 sensorleri fabrikadan +-0.2C hassasiyetle gelir. Ancak montaj pozisyonu, kablo uzunlugu ve ortam kosullari ek sapmalara neden olabilir. Kuluçkada 0.3C fark bile cikim oranini etkileyebilir.

### Kalibrasyon Nasil Yapilir?

1. **Referans termometre** edinin (kalibrasyon sertifikali dijital termometre, +-0.1C hassasiyet)
2. Referans termometreyi kulucka makinesinin icine, sensorlerin yanina yerlestirin
3. Sistemi 15-20 dakika sicaklik dengesine gelmesini bekleyin
4. Web arayuzunden mevcut sensor okumalarini goruntuleyin (`GET /api/status`)
5. Farki hesaplayin:
   - Referans: 37.5C, Sensor 1 okuyor: 37.2C → `tempOffset1 = 0.3`
   - Referans: 37.5C, Sensor 2 okuyor: 37.6C → `tempOffset2 = -0.1`
6. Nem icin de ayni islemi nem referansiyla yapin
7. Kalibrasyon degerlerini kaydedin:

```
POST /api/calibration
tempOffset1=0.3&humOffset1=0&tempOffset2=-0.1&humOffset2=0
```

### Sinirlar
- Sicaklik offset: maks +-5C
- Nem offset: maks +-10%
- Bu araliklarin disindaki degerler reddedilir (bozuk sensoru gizlememek icin)

### Kalibrasyon Nerede Saklanir?
NVS (Non-Volatile Storage) icinde kalici olarak saklanir. Guc kesilse bile korunur. Her boot'ta otomatik yuklenir.

---

## 11. Guc Kesintisi Kurtarma

### Nasil Calisir?
Kulucka calisirken sistem durumunu surekli NVS'ye yazar:
- Secili profil indexi
- Baslangic tarihi (Unix timestamp)
- Calisma durumu (aktif/pasif)
- PID parametreleri (Kp, Ki, Kd)
- Sensor kalibrasyon offset'leri

Guc geldiginde sistem otomatik olarak:
1. NVS'den onceki durumu kontrol eder
2. Timestamp'i dogrular (> 2020, gecmiste, max 60 gun once)
3. Profili geri yukler
4. RTC'den gercek tarihi okur ve kaldigi gundan devam eder
5. PID parametrelerini NVS'den yukler (auto-tuning atlanir)
6. `ALARM_POWER_RECOVERY` alarmi ile kullaniciyi bilgilendirir

### Ornek Senaryo
- Gun 12'de 2 saatlik elektrik kesintisi yasanir
- Guc geldiginde: "Guc kesintisi kurtarma: Gun 12 devam ediyor" alarmi goruntulenir
- Sistem aninda RUNNING moduna gecer, isitici/nemlendirici/cevirme devam eder
- 30 dakikalik auto-tuning beklenmez

### Onemli Not
`DURDUR` butonuna basildiginda NVS'deki kulucka durumu silinir. Bir sonraki acilista kurtarma yapilmaz.

---

## 12. Guvenlik Sistemi

Sistem cok katmanli guvenlik mekanizmalarina sahiptir:

### Ust Sicaklik Acil Kapatma
| Durum | Esik | Aksiyon |
|-------|------|---------|
| Asiri sicaklik | >= 40.0 C | ACIL KAPATMA: Isitici OFF, Fan MAKS |

### Alt Sicaklik Acil Kapatma
| Durum | Esik | Aksiyon |
|-------|------|---------|
| Kritik dusuk (sureli) | < 25.0 C, 2 dk boyunca | ACIL KAPATMA |
| Dusuk sicaklik uyari | < 20.0 C | Uyari (sensor hatasi suphelisi) |

**Neden alt sicaklik korumasi?** Sensor arizalanir ve 0C okursa, PID kontrolcu isiticiyi surekli %100'de calistirir. Bu hem isiticiyi yakabilir hem de yumurtalara zarar verir. 2 dakikalik bekleme, gecici durumlardan (kapak acma vb.) kaynaklanan false positive'leri onler.

### Sensor Guvenlik Kontrolleri
| Durum | Esik | Aksiyon |
|-------|------|---------|
| Tum sensorler ariza | 5 ardisik basarisiz okuma | ACIL KAPATMA |
| Her iki sensor ariza | S1 ve S2 ayri ayri >= 5 hata | ACIL KAPATMA |
| Bayat veri | 15sn guncelleme yok | Alarm |
| Gecersiz okuma araligi | < 15C veya > 50C | Okuma reddedilir |

### Cift Sensor Fuzyon Sistemi
- **Iki sensor uyumlu:** Ortalamalari alinir (en hassas okuma)
- **Sensorler uyumsuz:** Onceki filtreye yakin olan secilir + uyari
- **Bir sensor ariza:** Diger sensore otomatik gecis (failover yumusatma ile)
- **Iki sensor ariza:** ACIL KAPATMA

### Failover Yumusatma
Sensor gecisinde (ornegin S1 olur, S2'ye gecilir) ani sicaklik sicramasi olmamasi icin 5 okuma boyunca EMA filtre katsayisi dusurulur. Normal alpha=0.2 yerine gecis sirasinda alpha=0.06 kullanilir.

### Spike Filtresi
Tek bir okumada 2C'den fazla ani degisim tespit edilirse okuma reddedilir ve onceki deger korunur.

### I/O Ariza Tespiti (Role Yazma Dogrulamasi)

Roleler PCF8574 uzerinden I2C ile kontrol edilir. Onemli bir gercek: **I2C
yazmasi basarisiz olursa PCF8574 son cikis durumunu korur.** Yani "isiticiyi
kapat" komutu kaybolursa isitici ACIK kalir ve yazilim bunu bilmez.

Bu yuzden her role yazmasinin sonucu izlenir:

| Durum | Aksiyon |
|-------|---------|
| Tek yazma hatasi | I2C bus kurtarma + tekrar deneme |
| Ardisik 3 yazma hatasi (`RELAY_WRITE_FAIL_LIMIT`) | `ALARM_IO_FAIL` + acil kapatma + termal kacis modu |
| MUX yanit vermiyor | `ALARM_IO_FAIL` + otomatik bus kurtarma |

### Termal Kacis Tespiti (Yapisik Role)

Mekanik rolelerin bilinen ariza bicimi kontagin yapismasidir. Bu durumda
yazilim isiticiyi "kapatir" ama isitici fiziksel olarak calismaya devam eder.

Acil kapatmadan sonra sistem sicakligi izlemeye devam eder:

- `SAFETY_VERIFY_DELAY_MS` (90 sn) sonunda sicaklik en az
  `SAFETY_VERIFY_TEMP_DROP` (0.5 C) dusmediyse **termal kacis** ilan edilir.
- Sure dolmadan da sicaklik yukselmeye devam ediyorsa erken karar verilir.
- Kacis ilan edilince: fan tam guce alinir ve orada tutulur, roleler tekrar
  kapatilmaya calisilir, `ALARM_THERMAL_RUNAWAY` tetiklenir ve seri porta
  "FIZIKSEL MUDAHALE GEREKLI" uyarisi basilir.

> **Uyari:** Termal kacis durumunda yazilimin yapabilecegi baska bir sey
> kalmamistir. Isitici beslemesini elle kesmeniz gerekir. Bu senaryoya karsi
> gercek koruma, yazilimdan tamamen bagimsiz bir donanim termostati
> (bimetal/kapiler asiri isinma kesici) takmaktir.

Tani icin durum JSON'unda `io` nesnesi bulunur:

```json
"io": { "relayOK": true, "relayFails": 0, "muxOK": true, "muxRecover": 0, "runaway": false }
```

### I2C Bus Kurtarma

MUX'un arkasinda RTC, iki sicaklik sensoru, CO2, IR sensoru **ve roleler**
vardir. Bus kilitlenirse sistem hem kor kalir hem de cikislari anahtarlayamaz.
Roleleri anahtarlayan bir kutuda EMI kaynakli takilma gercek bir senaryodur.

Ardisik `MUX_FAIL_LIMIT` (3) kanal secim hatasindan sonra otomatik kurtarma
calisir:

1. `Wire.end()` ile surucu kapatilir
2. SCL'e 9 adet manuel darbe gonderilir — SDA'yi LOW tutan slave serbest kalir
3. Gecerli bir STOP kosulu uretilir
4. `Wire.begin()` ile bus yeniden baslatilir
5. MUX adresi tekrar bulunur ve dogrulanir

Kurtarma sayisi `io.muxRecover` alaninda raporlanir. Bu sayi surekli artiyorsa
kablo/ekranlama/pull-up direnclerini gozden gecirin.

### Acil Durumdan Kurtulma
Acil durumda TFT ekranda veya web arayuzunde "Guvenlik Sifirla" butonuna basin. Sistem PAUSED moduna gecer. Sorunu cozdugunuzden emin olduktan sonra tekrar baslatabilirsiniz.

Acil durumdayken sistem tamamen durmaz: sensor okumasi, I/O saglik denetimi,
termal kacis dogrulamasi ve alarm sesi calismaya devam eder.

---

## 13. Role Asinma Korumasi

### Neden Gerekli?
Mekanik roleler yaklasik 100.000 anahtarlama omrune sahiptir. Koruma olmadan:
- Dusuk PID cikislarinda isitici rolesi saniyede birkac kez acilip kapanir
- Nem esiginde nemlendirici hizla ON/OFF dongusu yapar
- **Yilda ~500.000 anahtarlama** → role 2-3 ayda bozulur

### Isitici Korumasi (HeaterDriver)
Time-proportional pencere (10sn) icinde:
- Hesaplanan ON suresi < 500ms → bu pencere ATLANIR (role tiklamaz)
- Hesaplanan OFF suresi < 500ms → bu pencere TAM ACIK tutulur

**Ornek:**
```
PID = 5   → ON suresi = 196ms < 500ms → ATLANDI (role kapali kalir)
PID = 10  → ON suresi = 392ms < 500ms → ATLANDI
PID = 13  → ON suresi = 510ms >= 500ms → Calisir (510ms ON, 9490ms OFF)
PID = 250 → ON suresi = 9804ms, OFF = 196ms < 500ms → TAM ACIK
```

Bu mantik sayesinde cok dusuk veya cok yuksek PID degerlerinde gereksiz role tiklamasi onlenir.

### Nemlendirici Korumasi (HumidifierDriver)
- **Minimum ON suresi:** 5 saniye (kapatma istegi bu sure dolmadan reddedilir)
- **Gecikmeli kapatma:** Kapatma isteginden 10 saniye sonra role kapanir
- **Cooldown:** Role kapatildiktan sonra 2 saniye icinde tekrar acilamaz

### Bakim Takibi
Her iki surucu de `getCycleCount()` fonksiyonu ile toplam anahtarlama sayisini tutar. Web arayuzunden veya debug log'dan takip edilebilir. 80.000'e yaklastiginda role degisimi planlanmalidir.

---

## 14. Fan PWM Kontrolu

Fan, kuluckanin ic sirkulasyonunu sagliyan en kritik bilesendir. Yeterli hava akimi olmazsa sicaklik dagilimi homojensiz olur, yumurtalar zarar gorur. Bu sistem **gercek dunya senaryolarina** gore optimize edilmis bir PWM kontrol mekanizmasi kullanir.

### Donanim

| Ozellik | Deger |
|---------|-------|
| **Pin** | IO18 (FAN_PIN) |
| **PWM Frekansi** | 25 kHz (insan kulagi duyamaz, sessiz) |
| **Cozunurluk** | 8-bit (0-255) |
| **Surucu** | L298N motor surucu (12V) veya direkt 4-pin PC fan |

### Calisma Mantigi (Real-World Behavior)

Sistemde fan kontrolu 4 onemli ozellige sahiptir:

#### 1. Kickstart (Yumusak Baslangic)

Fan **tamamen duraganken** dusuk PWM (ornegin 100) verirseniz, motor donmek icin yetersiz tork uretir ve donmaz. Bunun onune gecmek icin sistem **kapaliydan acilirken otomatik olarak 250 ms boyunca tam PWM (255)** uygular. Bu kisa darbe rotorun donmesini garanti eder, sonra hedef PWM'e gecer.

```
Senaryo: Fan kapali, PID 150 PWM istiyor
  T=0ms     setPWM(150)
  T=0ms     -> Kickstart aktif, PWM=255 (tam guc)
  T=250ms   Kickstart bitti, ramp basliyor
  T=400ms   Hedef 150 PWM'e ulasildi
```

#### 2. Soft Ramp (Kademeli Gecis)

Ani PWM degisimleri:
- Fan motorunu mekanik olarak zorlar
- Akim sokuna sebep olur (rolelerde ark)
- Akustik gurultu ve titresim olusturur

Sistem her **20 ms'de PWM degerini ±8 birim** degistirerek hedefe yumusak gecis yapar. 0'dan 255'e tam gecis ~640 ms surer.

```
Mevcut: 100 PWM, Hedef: 200 PWM
T=0:    100
T=20:   108
T=40:   116
...
T=240:  192
T=260:  200 (hedef)
```

#### 3. Off Threshold (Kapama Esigi)

Bazi PWM degerleri, fanin donmesine yeterli akim saglamaz ama yine de motor sargilarinda akim akar:
- Titremeye neden olur
- Inilti/vinilti sesleri cikarir
- Bobin isinmasi ve verim kaybi

Cozum: PWM degeri **20'nin altinda ise fan tamamen kapatilir** (`FAN_OFF_THRESHOLD = 20`).

```
setPWM(15)  -> 15 < 20 -> Fan tamamen KAPALI
setPWM(0)   -> Fan tamamen KAPALI (FAN_MIN_PWM bypass)
setPWM(50)  -> 50 < FAN_MIN_PWM(80) -> 80'e zorlanir
setPWM(150) -> Aynen uygulanir (ramp ile)
```

#### 4. Sifir = Tam Kapali

Mevcut kod `FAN_MIN_PWM`'i (80) zorla uyguluyordu (fan asla durmaz prensibi). Yeni mantik:
- `setPWM(0)` -> **Fan tamamen kapanir** (temizlik modu, manuel kontrol icin)
- `setPWM(>0)` -> En az `FAN_MIN_PWM` ile calisir

### Konfigurasyon (Config.h)

```cpp
#define FAN_PIN              18      // GPIO pini
#define PWM_FREQ_FAN         25000   // 25 kHz (sessiz)
#define PWM_RESOLUTION       8       // 8-bit (0-255)
#define FAN_MIN_PWM          80      // Minimum aktif hiz
#define FAN_MAX_PWM          255     // Maksimum hiz
#define FAN_OFF_THRESHOLD    20      // Bu altinda kapat
#define FAN_KICKSTART_MS     250     // Kickstart suresi
#define FAN_RAMP_STEP        8       // Adim basina PWM degisimi
#define FAN_RAMP_INTERVAL_MS 20      // Adimlar arasi sure
```

### API (FanDriver.h)

```cpp
class FanDriver {
public:
    void begin();                        // Init: PWM pinine attach, 0'dan basla
    void setPWM(uint8_t value);          // Hedef PWM ayarla (kickstart/ramp ile)
    void stop();                         // Hizli kapat (bypass kickstart/ramp)
    void update();                       // Loop'tan cagrilmali (ramp tick)
    uint8_t getCurrentPWM() const;       // Anlik gercek PWM
    uint8_t getTargetPWM()  const;       // Istenen hedef PWM
    void setSoftBehavior(bool enabled);  // Soft mod ac/kapa (test icin)
};
```

**Kritik:** `update()` metodu IncubationService'in her loop iterasyonunda otomatik cagrilir. Manuel kullanim icin de loop()'tan cagrilmalidir.

### Kullanim Ornekleri

**Normal kullanim (PID kontrol):**
```cpp
fan.setPWM(180);  // Hedef PWM ayarla, sistem ramp ile yumusakca ulasir
// loop'ta fan.update() cagrilmali
```

**Acil durdurma:**
```cpp
fan.stop();  // Hemen kapat, ramp/kickstart yok
```

**Temizlik modu (anlik kontrol):**
```cpp
fan.setSoftBehavior(false);  // Soft modu kapat
fan.setPWM(255);             // Anlik tam guc
fan.setSoftBehavior(true);   // Geri ac
```

### Davranis Senaryolari

| Senaryo | Eylem | Sonuc |
|---------|-------|-------|
| Cihaz acildi | `begin()` | PWM=0 (kapali) |
| PID 150 istedi (kapaliydi) | `setPWM(150)` | Kickstart 250ms tam guc -> ramp -> 150 |
| PID 200 istedi (zaten 150) | `setPWM(200)` | Kickstart YOK, direkt ramp 150->200 |
| Kullanici "fan kapat" dedi | `setPWM(0)` | Ramp 150->0 (kapanir) |
| Acil durum | `stop()` | Anlik 0, ramp yok |
| PID 5 istedi | `setPWM(5)` | 5 < 20 -> kapanir |
| PID 50 istedi | `setPWM(50)` | 50 < 80 -> 80'e zorlanir |

### Faydalari

- 🔇 **%30+ daha sessiz** (25 kHz PWM + soft transitions)
- 🔧 **Mekanik omur uzar** (ani gerilim yok, ramp ile akıcı)
- ⚡ **Daha az akim soku** (kickstart sayesinde stall yok, ramp ile yavaş artış)
- 🎯 **Fan asla "stuck"** kalmaz (kickstart garantili donme saglar)
- 💡 **Verim artar** (FAN_OFF_THRESHOLD ile gereksiz akim cekimi yok)

---

## 15. RTC (Gercek Zamanli Saat) Yonetimi

DS3231 RTC modulu, kuluckanin baslangic tarihi ve gun sayimini hassas sekilde takip eder. Pil destekli oldugu icin guc kesintisi sonrasinda dahi dogru zamani korur. RTCManager surucusu **gercek dunya senaryolarina** karsi guclendirilmistir.

### Donanim

| Ozellik | Deger |
|---------|-------|
| **Modul** | DS3231 (Adafruit RTClib) |
| **I2C Adresi** | 0x68 |
| **MUX Kanali** | CH0 (Grove 8 kanal I2C MUX uzerinden) |
| **Pil** | CR2032 (~3-5 yil omur) |
| **Hassasiyet** | ±2 ppm (0-40°C) |
| **Sicaklik Sensoru** | Var (±3°C, dahili kompanzasyon) |

### Calisma Mantigi (Real-World Behavior)

#### 1. Cached Read (Performans)

`now()` metodu her cagrildiginda I2C trafigi yapmaz. Bunun yerine:
- Son okunan zaman cache'lenir
- Sonraki cagri millis() farkini ekleyerek hesaplar
- Sadece **1 saniyede bir gercek I2C okumasi** yapilir

```
1000 kez now() çağrılırsa:
  Eski yontem: 1000 I2C okuma  -> bus tikanikligi
  Yeni yontem: 1 okuma + 999 cache hit  -> %99 az trafik
```

`nowForced()` cache'i bypass eder (kritik anlarda kullanilir).

#### 2. Retry Mekanizmasi

EMI (electromagnetic interference) durumunda DS3231 bazen `0xFF` dolu byte donebilir → year() taşar (2155 vb.). RTCManager bunu tespit edip otomatik **3 kez yeniden okur** (5 ms aralikla).

```cpp
#define RTC_RETRY_COUNT      3
#define RTC_RETRY_DELAY_MS   5
```

#### 3. Tarih Validasyonu

Her okumadan sonra tarih makul mu kontrol edilir:

```cpp
static bool isValidDate(const DateTime &dt) {
    return (dt.year() >= 2024 && dt.year() <= 2099 &&
            dt.month() >= 1 && dt.month() <= 12 &&
            dt.day()   >= 1 && dt.day()   <= 31 &&
            dt.hour()  <= 23 && dt.minute() <= 59 && dt.second() <= 59);
}
```

#### 4. Saglik Durumu (Health Status)

RTCManager 6 farkli durumda olabilir:

| Durum | Kod | Aciklama |
|-------|-----|----------|
| `RTC_STATE_OK` | 1 | Calisiyor, zaman gecerli |
| `RTC_STATE_BATTERY_LOW` | 2 | Pil zayif (lostPower flag set) |
| `RTC_STATE_INVALID` | 3 | Tarih makul degil (year < 2024) |
| `RTC_STATE_I2C_FAIL` | 4 | I2C iletisim hatasi |
| `RTC_STATE_FALLBACK` | 5 | RTC yok, millis tabanli |
| `RTC_STATE_UNKNOWN` | 0 | Henuz init edilmedi |

**Watchdog:** 10 saniye boyunca okuma yapilamazsa otomatik `RTC_STATE_I2C_FAIL` olur.

```cpp
if (!rtc.isHealthy()) {
    Serial.printf("[RTC] Sorun: %s\n", rtc.getStatusString());
    // Alarm tetikle, kullaniciya bildir
}
```

#### 5. Manuel Zaman Ayarlama (Web UI / NTP)

WiFi varsa NTP'den, yoksa **web arayuzundeki "Tarih/Saat Ayari" kartindan** elle ayarlanabilir.

**Web UI Akisi (Ayar tabi → Tarih/Saat Ayari):**

```
+--------------------------------------------------+
| 📅 Tarih / Saat Ayari                            |
+--------------------------------------------------+
| Cihaz Saati                                      |
|         14:35  07.05.2026                        |
+--------------------------------------------------+
| Tarih: [2026-05-07 ▼]    Saat: [14:35 ▼]         |
+--------------------------------------------------+
| [📱 Tarayicidan Al]      [✓ Cihaza Yaz]          |
+--------------------------------------------------+
```

- **Tarih input:** HTML5 `<input type="date">` — browser'in native tarih picker'i (dropdown)
- **Saat input:** HTML5 `<input type="time">` — browser'in native saat picker'i
- **Tarayicidan Al:** `new Date()` ile bilgisayarinizin saatini input'lara yansitir (kaydetmez)
- **Cihaza Yaz:** `POST /api/time` ile cihaza yazar, NVS'e de seed eder

**Backend API:**

```cpp
bool setUnixTime(uint32_t unixSec);   // NTP'den / programatik (Unix timestamp)
bool setDateTime(const DateTime &dt); // Direkt DateTime
// IncubationService::setSystemTime — RTC + NVS tek seferde
bool setSystemTime(uint32_t unixSec); // setUnixTime + saveLastKnownTime
```

`POST /api/time` endpoint'i:
- Form: `date=YYYY-MM-DD&time=HH:MM` (web UI formati)
- veya `unix=<unixSec>` (programatik)
- Validate eder (year 2024-2099), RTC'ye yazar, **NVS'i guncel zamanla seed eder** (elektrik kesintisi sonrasi kalir)
- Donus: `{"ok":true,"unix":1715089800}`

#### 6. Fallback Mode (RTC Pilsiz)

Eger DS3231 pili tamamen bittiyse veya modul iletisim kuramiyorsa:
1. NVS'den son `elapsedDays` okunur
2. `setElapsedDaysFallback(savedDays)` cagrilir
3. Sistem millis() tabanli sahte zaman uretir
4. `getElapsedDays()` dogru gun sayisini doner
5. `now()` sahte ama mantikli bir DateTime doner (candling icin gerekli)

### Konfigurasyon (Config.h)

```cpp
#define RTC_CACHE_INTERVAL_MS   1000   // 1 sn cache
#define RTC_RETRY_COUNT         3      // 3x retry
#define RTC_RETRY_DELAY_MS      5      // 5ms aralik
#define RTC_VALID_YEAR_MIN      2024
#define RTC_VALID_YEAR_MAX      2099
#define RTC_HEALTH_TIMEOUT_MS   10000  // 10sn timeout
```

### API (RTCManager.h)

```cpp
// Lifecycle
bool begin();

// Zaman okuma
DateTime now() const;          // Cached (hizli)
DateTime nowForced() const;    // I2C zorla okur (kritik anlar)
uint32_t getUnixTime() const;

// Gun sayimi
int  getElapsedDays() const;
void setStartDate(const DateTime &date);
DateTime getStartDate() const;

// Manuel zaman ayarlama
bool setUnixTime(uint32_t unixSec);    // NTP/UI'dan
bool setDateTime(const DateTime &dt);

// Fallback (RTC pilsiz)
void setElapsedDaysFallback(uint16_t days);

// Format
String getFormattedTime() const;       // "HH:MM:SS"
String getFormattedDate() const;       // "DD/MM/YYYY"

// Saglik
float       getModuleTemperature();    // DS3231 dahili sicaklik
RTCStatus   getStatus() const;
const char* getStatusString() const;
bool        isHealthy() const;
unsigned long getMsSinceLastSync() const;

// Static helper
static bool isValidDate(const DateTime &dt);
```

### Kullanim Ornekleri

**Hizli zaman okuma (loop icinde):**
```cpp
DateTime t = rtc.now();          // cached, ~1us
Serial.println(t.unixtime());
```

**Kritik nokta (begin sonrasi):**
```cpp
DateTime fresh = rtc.nowForced(); // I2C zorla, +5ms ama kesin doğru
```

**NTP entegrasyonu (WebService'te):**
```cpp
if (WiFi.isConnected()) {
    configTime(0, 0, "pool.ntp.org");
    time_t ntp = time(nullptr);
    if (ntp > 1700000000) {       // 2023+ ise gecerli
        rtc.setUnixTime((uint32_t)ntp);
    }
}
```

**Saglik kontrolu (alarm icin):**
```cpp
if (!rtc.isHealthy() || rtc.getMsSinceLastSync() > 30000) {
    triggerAlarm("RTC sorunu: " + String(rtc.getStatusString()));
}
```

### Davranis Senaryolari

| Senaryo | Sonuc |
|---------|-------|
| Cihaz acildi, RTC OK | `STATE_OK`, baslangic tarihi cached |
| RTC pili bitmis | `STATE_BATTERY_LOW`, derleme zamani fallback |
| RTC tamamen yok | `STATE_FALLBACK`, millis tabanli sayim |
| EMI gurultusu | 3x retry, basarisizsa `STATE_I2C_FAIL` |
| 10 sn okuma yok | `STATE_I2C_FAIL` otomatik set |
| NTP'den zaman geldi | `setUnixTime()` -> `STATE_OK` |
| Tarih < 2024 | `STATE_INVALID` |

### Faydalari

- 🚀 **%99 daha az I2C trafiği** (cache sayesinde)
- 🛡️ **EMI dayanıklılığı** (retry + validation)
- 📊 **Detaylı sağlık durumu** (web/TFT'de gosterilebilir)
- 🌐 **NTP-ready** (`setUnixTime` ile WiFi'den senkronize)
- 🔄 **Daha güvenilir fallback** (RTC pilsiz çalışır)
- 🎯 **Hassas tarih validasyonu** (2024-2099 arasi)

### Sorun Giderme (RTC)

| Belirti | Sebep | Cozum |
|---------|-------|-------|
| `STATE_BATTERY_LOW` | CR2032 pili bitmis | Pili degistir, NTP'den zamani set et |
| `STATE_INVALID` | Year < 2024 | Pil degistir veya NTP/UI'dan zamani set et |
| `STATE_I2C_FAIL` | MUX/I2C kablo sorunu | Lehimleri ve baglantilari kontrol et |
| `STATE_FALLBACK` | DS3231 modulu yok/bozuk | Modulu degistir |
| Tarih atliyor | Cache + millis cakismasi | `nowForced()` kullan |

---

## 16. Alarm Sistemi

### Alarm Turleri

| Kod | Alarm | Aciklama |
|-----|-------|----------|
| 1 | TEMP_HIGH | Sicaklik ust esigin (38.5C) uzerinde |
| 2 | TEMP_LOW | Sicaklik alt esigin (36.0C) altinda |
| 3 | HUM_HIGH | Nem %85 uzerinde |
| 4 | HUM_LOW | Nem %30 altinda |
| 5 | CO2_HIGH | CO2 ust limit asildi |
| 6 | CO2_CRITICAL | CO2 kritik seviye |
| 7 | SENSOR_FAIL | Sensor okuma hatasi veya bayat veri |
| 8 | SENSOR_MISMATCH | Iki sensor uyumsuz |
| 9 | SAFETY_SHUTDOWN | Guvenlik acil kapatma |
| 10 | TURNING_STOPPED | Cikim fazinda cevirme durduruldu (bilgi) |
| 11 | INCUBATION_COMPLETE | Kulucka tamamlandi |
| 12 | CANDLING_DUE | Dol kontrolu (candling) zamani geldi |
| 13 | POWER_RECOVERY | Guc kesintisi sonrasi kurtarma yapildi |
| 14 | PHASE_TRANSITION | Faz gecisi (Gelisim->Cikim/Lockdown vb.) |
| 15 | CANDLING_TOMORROW | Yarin dol kontrolu var (T-1 hatirlatma) |
| 16 | IO_FAIL | Role kartina yazilamiyor - cikislar kontrol edilemiyor |
| 17 | THERMAL_RUNAWAY | Kapatildi ama sicaklik dusmuyor (role yapismis) |
| 18 | EGG_TEMP_HIGH | Yumurta kabuk sicakligi cok yuksek |
| 19 | EGG_TEMP_LOW | Yumurta kabuk sicakligi cok dusuk |
| 20 | EGG_SENSOR_LOST | Yumurta IR kaynagi kayboldu (yerel + uzak yok) |

> **16 ve 17 numarali alarmlar donanim arizasi bildirir**, ortam kosulu degil.
> Alarmi susturmak sorunu cozmez; 17 (termal kacis) durumunda isitici
> beslemesini fiziksel olarak kesmeniz gerekir. Detay: Bolum 12.

### Pause Modunda Alarm
Sistem duraklatilmis (PAUSED) durumda bile sensor okumaya ve alarm kontrolune devam eder. Ornegin makine duraklatilmis halde odada sicaklik 15C'ye duserse alarm verilir.

### Faz Gecis Alarmi (PHASE_TRANSITION)

Sistem **otomatik olarak** faz degisikliklerini tespit eder ve `ALARM_PHASE_TRANSITION` alarmini tetikler. Boylece kullanici onemli bir gecisi kacirmaz.

**Lockdown Tespiti (Ozel Vurgu):**

Yeni faz `turningEnabled = false` ise (yani onceki fazda cevirme acikti, yeni fazda kapali) **bu Lockdown** olarak isaretlenir ve mesaj kalin yazilir:

```
LOCKDOWN! Gelisim -> Cikim | Cevirme DURDU, nem yukseltildi
```

**Diger Faz Gecisleri (Bilgi):**

```
Faz Gecisi: Erken Gelisim -> Gec Gelisim (Gun 12)
```

**Mekanizma:**

```cpp
// IncubationService::updatePhase()
if (yeniFaz != onceFaz && onceFaz != nullptr) {
    bool isLockdown = !yeniFaz->turningEnabled && onceFaz->turningEnabled;
    snprintf(msg, "...", onceFaz->phaseName, yeniFaz->phaseName, day);
    _alarm.triggerAlarm(ALARM_PHASE_TRANSITION, msg);
}
```

**Kullanici Aksiyonlari:**

- **Lockdown gecisinde:** Yumurtalari acmayin, kapagi kaldirmayin, nem yuksek tutun
- **Cikim fazinda:** Yumurtalar 1-3 gun icinde caticar
- **Faz gecisi sirasinda:** PID hedef sicakligi otomatik guncellenir, nem esikleri profilden alinir

### Faz Gecis Gecmisi (Phase Log)

Sistem her faz gecisinde **otomatik kayit** tutar: hangi faza ne zaman girildi, ne kadar surdu. Bu sayede kuluckanizin tam takvim gecmisini izleyebilirsiniz.

**Saklanan Veriler:**
- Faz adi (ornegin "Erken Gelisim", "Gec Gelisim", "Cikim/Lockdown")
- Baslangic Unix timestamp + 1-based gun
- Bitis Unix timestamp + 1-based gun (aktif faz icin 0)
- Sure (saniye cinsinden, hesaplanir)
- Aktif/Tamamlandi flag

**Kapasite:** RAM'de max **8 entry** (yeterli — bir kuluckada genelde 2-4 faz olur).

**API:** `GET /api/phase-log`

```json
{
  "count": 3,
  "entries": [
    {"name":"Erken Gelisim","startUnix":1746547200,"endUnix":1746979200,
     "startDay":1,"endDay":5,"durationSec":432000,"active":false},
    {"name":"Gec Gelisim","startUnix":1746979200,"endUnix":1748275200,
     "startDay":5,"endDay":18,"durationSec":1296000,"active":false},
    {"name":"Cikim","startUnix":1748275200,"endUnix":0,
     "startDay":18,"endDay":0,"durationSec":86400,"active":true}
  ]
}
```

**Web UI:** Profil sekmesinde "Faz Gecis Gecmisi" karti otomatik 30 sn'de bir guncellenir.

```
+------------------------------------------+
| 📝 Faz Gecis Gecmisi                     |
+------------------------------------------+
| 1. Erken Gelisim         [TAMAMLANDI]    |
|    Baslangi: 06.05.2026 14:00 (Gun 1)    |
|    Bitis:    11.05.2026 14:00 (Gun 5)    |
|    Sure:     5.0 gun                     |
+------------------------------------------+
| 2. Gec Gelisim           [TAMAMLANDI]    |
|    Baslangi: 11.05.2026 14:00 (Gun 5)    |
|    Bitis:    24.05.2026 14:00 (Gun 18)   |
|    Sure:     13.0 gun                    |
+------------------------------------------+
| 3. Cikim                 [AKTIF]         |
|    Baslangi: 24.05.2026 14:00 (Gun 18)   |
|    Sure:     1.0 gun (devam ediyor)      |
+------------------------------------------+
```

**Renk Kodlama:**
- 🟢 **Yesil border + AKTIF badge** — su an icindeki faz
- ⚫ **Gri border + TAMAMLANDI badge** — gecmis fazlar

**Kuluckanin Yeniden Baslamasi:**
- `start()` cagrildiginda log sifirlanir
- Ilk faz (gun 1) otomatik olarak entry #0 olarak kaydedilir
- Sonraki gecislerde entry eklenir

### Alarm Gecmisi
Son 20 alarm kaydi bellekte tutulur. Web arayuzunden `GET /api/alarm` ile JSON formatinda okunabilir.

### Alarm Modal Butonlari (SUSTUR / ERTELE / KAPAT)

Tam ekran alarm modal'inda kullanicinin secebilecegi 3 farkli aksiyon vardir:

| Buton | Renk | Sure | Aciklama |
|-------|------|------|----------|
| **SUSTUR** | Yesil | 10 dakika | Speaker durur, modal kapanir. Durum hala kotuse 10 dk sonra tekrar acilir. |
| **ERTELE** | Turuncu | 1 saat | Daha uzun susturma. Durum kotu kalirsa 1 saat sonra tekrar tetiklenir. |
| **KAPAT** | Gri | Tip-bagimli | Ozel davranis (asagida) |

**KAPAT butonu davranis farklari:**

| Alarm Tipi | KAPAT davranisi |
|------------|-----------------|
| **Standart** (sicaklik/nem/CO2) | Alarm temizlenir. **Durum hala bozuksa kontrol mantigi tarafindan hemen tekrar tetiklenir** (guvenlik). Bu nedenle kullanici alarmi gozetip nedenini cozmedikce KAPAT etkisiz olur. |
| **CANDLING** (dol kontrolu) | 24 saat susturulur, ayrica `_candlingLastDay` ayni gune set edilir → bugun icinde tekrar tetiklenmez. "Kontrolu zaten yaptim" demek icin uygundur. |

**CANDLING modal'da SUSTUR yok!** CANDLING'de sadece ERTELE ve KAPAT vardir cunku 10 dakika tekrar acmak anlamli degildir (kullanici aktif olarak yumurta inceleyecek).

**API Endpoint'leri:**
- `POST /api/alarm/ack` — SUSTUR (10 dk)
- `POST /api/alarm/snooze` — ERTELE. Opsiyonel `ms=<sure>` parametresi (default: candling=1sa, diger=10dk)
- `POST /api/alarm/dismiss` — KAPAT (tip-bagimli davranis)

**Ornek (1 saat ertele):**
```bash
curl -X POST http://192.168.1.50/api/alarm/snooze -d "ms=3600000"
```

---

## 17. Dol Kontrolu (Candling) Sistemi

Dol kontrolu (candling), yumurtalarin gelisim asamalarini izah etmek icin yumurtaya kuvvetli isik tutarak yapilan denetim islemidir. Sistem otomatik olarak tur bazli kontrol gunlerini hesaplar, kullaniciya hatirlatir ve takvimi gosterir.

### Otomatik Kontrol Gunleri

Sistem her hayvan profili icin **4 otomatik kontrol gunu** hesaplar:

| Kontrol | Hesaplama | Amac |
|---------|-----------|------|
| **1. Kontrol** | Toplam surenin %25'i | Embriyo damarlarinin olusumu kontrolu |
| **2. Kontrol** | Toplam surenin %50'si | Embriyo gelisiminin orta noktasi |
| **3. Kontrol** | Toplam surenin %75'i | Hava bosllugu ve hareket kontrolu |
| **Lockdown** | Cikim fazi baslangici | Cevirme durdurulur, nem yukseltilir |

**Ornek (Tavuk - 21 gun):**
- 1. Kontrol: Gun 5 (%25)
- 2. Kontrol: Gun 10 (%50)
- 3. Kontrol: Gun 15 (%75)
- Lockdown: Gun 18

**Ornek (Guvercin - 19 gun):**
- 1. Kontrol: Gun 4
- 2. Kontrol: Gun 9
- 3. Kontrol: Gun 14
- Lockdown: Gun 16

### Profil Sekmesinde Tarih Gosterimi

Profil sekmesinde her kontrol gunu icin **gercek tarih** ve **kalan gun** bilgisi gosterilir:

```
+----------------------------------------+
| Dol Kontrolu                           |
| 1.Kontrol: 11.05.2026     5 gun kaldi  |
| 2.Kontrol: 16.05.2026    10 gun kaldi  |
| 3.Kontrol: 21.05.2026    15 gun kaldi  |
| Lockdown:  25.05.2026    19 gun kaldi  |
+----------------------------------------+
```

**Renk Kodlamasi:**
- 🟡 **Sari:** Bugunun kontrolu (BUGUN!)
- 🔵 **Mavi (cyan):** Gelecek kontroller
- ⚫ **Soluk:** Gecmis kontroller

**Onemli:** Tarih gosterimi icin **kuluckanin baslatilmis** olmasi gerekir. Henuz baslatilmamissa tarih yerine "Gun 5", "Gun 10" gibi gun numaralari gosterilir.

### Tarih Hesaplama Mantigi

Tarihler NVS'deki Unix timestamp'ten guvenli sekilde hesaplanir:

```
Kontrol Tarihi = Baslangi Tarihi + (Kontrol Gunu - 1)
Kalan Gun = Kontrol Gunu - Suanki Gun
```

Eger RTC pili bittiyse veya yanlis tarih donerse, sistem fallback olarak gun numarasi gosterir (yıl 2020-2099 arasi gecerli kabul edilir).

### Otomatik Alarm Tetiklenmesi

Kontrol gunu geldiginde sistem otomatik olarak `ALARM_CANDLING_DUE` alarmini tetikler:

1. **Sesli Alarm:** Hoparlor `startAlarmPattern()` ile ses calar
2. **Tam Ekran Modal:** Mavi/lacivert tonlu ozel ekran acilir
3. **Banner Uyarisi:** Tum sekmelerin ustunde yanip sonen bant (kirmizi/sari)
4. **Tekrar Onleme:** `_candlingLastDay` ile ayni gun tekrar tetiklenmez

### Ozel Alert Modal'i (Diger Alarmlardan Farkli)

Dol kontrolu alarmi diger alarmlardan **gorsel olarak ayrilir**:

```
+----------------------------------------+
|         * DOL KONTROLU *               |  Mavi banner
+----------------------------------------+
|       Bugun Kontrol Gunu!              |  Sari vurgu
|                                        |
|  1.Kontrol: 11.05.2026      BUGUN!     |
|  2.Kontrol: 16.05.2026       5 gun     |
|  3.Kontrol: 21.05.2026      10 gun     |
|  Lockdown:  25.05.2026      14 gun     |
|                                        |
|         [   ANLADIM   ]                |  Yesil buton
|          (10 dakika)                   |
+----------------------------------------+
```

**Standart Alarm (Sicaklik/Nem) ile Karsilastirma:**

| Ozellik | Dol Kontrolu | Standart Alarm |
|---------|--------------|----------------|
| **Renk** | Mavi/Lacivert | Kirmizi |
| **Banner** | "* DOL KONTROLU *" | "! ALARM !" |
| **Icerik** | Tum kontrol tarihleri | Sicaklik/Nem degerleri |
| **Buton** | "ANLADIM" | "SUSTUR" |

Bu sayede kullanici tek bakista hangi alarm tipi oldugunu anlar.

### Banner Uyari (Ust Bant)

Bugun kontrol gunu ise tum sekmelerin ustunde animasyonlu bant gorunur:

```
🔔 DOL KONTROLU ZAMANI! 1.Kontrol (%25)    Lockdown: G18
```

**Ozellikler:**
- Kirmizi <-> Sari yanip sonen efekt (500 ms periyot)
- Buyuk font (Font 2)
- Sag tarafta lockdown gunu bilgisi
- Temizlik modu banner'i ile cakisirsa temizlik onceliklenir

### Kontrol Sirasinda Ne Yapilmali?

**1. Kontrol (Embriyo Damarlari):**
- Yumurtayi kuvvetli isikla kontrol et
- Damar agi ve embriyo govdesi gorunmeli
- Sari/kirmizi noktalar (kalp atisi) gorunmeli
- Gorunmuyorsa: Kismet (gelisigmemis) — atın

**2. Kontrol (Yari Yol):**
- Embriyo cogalan, hareket etmeli
- Hava bosslugu olusmaya basladi
- Yumurta yarisindan fazlasi karanlik

**3. Kontrol (Son Asama):**
- Hava bosslugu net gorunmeli
- Embriyo aktif hareket etmeli
- Yumurta neredeyse tamamen karanlik

**Lockdown (Cikim Hazirligi):**
- Cevirme motoru otomatik DURUR
- Nem yukseltilir (genelde +%10)
- Yumurtayi acmayin, kapagi kaldirmayin
- Cikim 1-3 gun icinde gerceklesir

### Teknik Detaylar

**CandlingScheduler.h** dosyasi tum hesaplamalari yapar:

```cpp
struct CandlingSchedule {
    const char* speciesName;
    uint8_t     totalDays;
    uint8_t     lockdownDay;
    uint8_t     checkCount;
    uint8_t     checkDays[CANDLING_MAX_CHECKS];   // Kontrol gunleri
    const char* labels[CANDLING_MAX_CHECKS];
    bool        triggered[CANDLING_MAX_CHECKS];
};
```

**Ana fonksiyonlar:**
- `buildCandlingSchedule(profile, &out)` - Programi olustur
- `isCandlingDay(sched, day)` - Bugun kontrol gunu mu?
- `getCandlingLabel(sched, day)` - Etiket al ("1.Kontrol (%25)" vb.)
- `nextCandlingDay(sched, currentDay)` - Bir sonraki kontrol gunu

### JSON API Cikisi

`GET /api/status` cevabinda candling bilgisi:

```json
{
  "candling": {
    "isToday": false,
    "label": "",
    "lockdownDay": 18,
    "nextCandlingDay": 10
  }
}
```

---

## 18. Sensor Grafikleri ve Veri Gecmisi

Web arayuzunde sicaklik, nem ve CO2 sensorlerinden gelen veriler **anlik grafik** olarak izlenebilir. Bu bolum hem gorsel takip icin hem de gelecekte veritabani entegrasyonu icin altyapi saglar.

### Genel Bakis

- **Tip:** Canvas tabanli (vanilla JS, harici kutuphane yok — flash tasarrufu)
- **Veri Kaynagi:** Backend RAM buffer (60 okuma)
- **Okuma Araligi:** Her 5 saniye (`HISTORY_INTERVAL = 5000` ms)
- **Toplam Sure:** ~5 dakika gecmis veri (60 × 5 = 300 sn)
- **Web UI Yenileme:** Her 5 saniyede `GET /api/history`

### Web Arayuzu - Sensor Grafikleri Karti

Dashboard sekmesinde alarm kartinin altinda "Sensor Grafikleri" karti yer alir.

```
+----------------------------------------------------+
| 📊 Sensor Grafikleri        Son 5 dk (5sn aralik)  |
+----------------------------------------------------+
| [● Sicaklik 37.5°C] [● Nem %55] [● CO2 3200]       |
+----------------------------------------------------+
|  38.0 ┤  ╭─╮                                       |
|  37.5 ┤ ╱   ╲___╱╲___                              |
|  37.0 ┤ ──────────────                             |
|  36.5 ┤                                            |
|  36.0 ┤                                            |
|       -5dk      -2.5dk         simdi               |
+----------------------------------------------------+
| MIN: 36.5  | MAX: 38.0  | ORT: 37.4  | SON: 37.5   |
+----------------------------------------------------+
```

### Sensor Sekmeleri (Tabs)

Uc farkli sensor verisi icin tab'lar:

| Sekme | Renk | Birim | Ondalik |
|-------|------|-------|---------|
| **Sicaklik** | Kirmizi (#ef4444) | °C | 1 |
| **Nem** | Mavi (#3b82f6) | % | 1 |
| **CO2** | Yesil (#10b981) | ppm | 0 |

Her sekme uzerinde **anlik son okuma degeri** gosterilir, sekmeye tiklandiginda grafik otomatik o sensorle degisir.

### Grafik Ozellikleri

- **Otomatik Olcek:** Y ekseni veriye gore dinamik ayarlanir (min-%10 ile max+%10)
- **Grid Cizgileri:** 4 yatay seviyede deger etiketleri
- **Zaman Ekseni:** Sol "-5dk", orta "-2.5dk", sag "simdi"
- **Dolgu Efekti:** Cizginin altinda yari saydam alan
- **Son Nokta Vurgusu:** Yuvarlak isaret ile vurgulanir
- **HiDPI Destegi:** Yuksek cozunurluklu ekranlarda net cizim (devicePixelRatio)
- **Responsive:** Pencere yeniden boyutlandiginda otomatik yeniden cizilir

### Istatistik Paneli

Grafik altinda 4 istatistik gosterilir:

| Metric | Aciklama |
|--------|----------|
| **MIN** | Son 5 dakikadaki en dusuk deger |
| **MAX** | Son 5 dakikadaki en yuksek deger |
| **ORT** | Son 5 dakikanin ortalamasi |
| **SON** | En son okunan deger |

### API Endpoint: `GET /api/history`

**Cevap formati:**
```json
{
  "interval": 5,
  "count": 60,
  "unixTime": 1746547200,
  "temp": [37.4, 37.5, 37.6, ...],
  "hum": [55.0, 56.1, 57.2, ...],
  "co2": [3200, 3250, 3300, ...]
}
```

**Alanlar:**
- `interval` — Saniye cinsinden okuma araligi
- `count` — Dizilerdeki eleman sayisi
- `unixTime` — Mevcut zaman damgasi (en son okumanin yaklasik zamani)
- `temp[]` — Sicaklik degerleri (°C, float)
- `hum[]` — Nem degerleri (%, float)
- `co2[]` — CO2 degerleri (ppm, integer)

### Backend Implementation

**RAM Buffer:**
```cpp
#define GRAPH_HISTORY_SIZE 60
float    _tempHistory[GRAPH_HISTORY_SIZE];   // 240 byte
float    _humHistory[GRAPH_HISTORY_SIZE];    // 240 byte
uint16_t _co2History[GRAPH_HISTORY_SIZE];    // 120 byte
```

Toplam RAM kullanimi: ~600 byte (FIFO buffer).

**FIFO Mantigi:**
- Buffer dolmadiginda yeni veriler sona eklenir
- Buffer dolduğunda en eski deger atilir, yenisi sona eklenir (`memmove`)
- Boylece her zaman son 60 okuma korunur

### Veritabani Entegrasyonu Icin Hazir

Sistem ileride veritabanina baglandiginda gerekli tum bilgiler `getHistoryJSON()` cevabinda mevcut:

**Her okumanin gercek zamanini hesaplama:**
```javascript
// JavaScript ornek
function getRecordTime(history, index) {
    return history.unixTime - (history.count - index - 1) * history.interval;
}
```

**Python ornegi (DB aktarimi):**
```python
import requests
import sqlite3
from datetime import datetime

r = requests.get('http://192.168.1.50/api/history').json()
conn = sqlite3.connect('kulucka.db')
cur = conn.cursor()

for i in range(r['count']):
    timestamp = r['unixTime'] - (r['count'] - i - 1) * r['interval']
    cur.execute(
        "INSERT INTO sensor_log (ts, temp, hum, co2) VALUES (?, ?, ?, ?)",
        (timestamp, r['temp'][i], r['hum'][i], r['co2'][i])
    )
conn.commit()
```

**Onerilen DB Schema:**
```sql
CREATE TABLE sensor_log (
    ts          INTEGER PRIMARY KEY,    -- Unix timestamp
    device_id   TEXT,                   -- KM-A4C138F2
    temp        REAL,                   -- Sicaklik (°C)
    hum         REAL,                   -- Nem (%)
    co2         INTEGER,                -- CO2 (ppm)
    target_temp REAL,                   -- Hedef sicaklik
    phase       TEXT,                   -- Aktif faz
    INDEX idx_device_ts (device_id, ts)
);
```

### Ileride Eklenebilecek Ozellikler

- **Daha uzun gecmis:** SD karta veya uzak DB'ye log alma
- **Hedef cizgileri:** Grafiğe hedef sicaklik/nem cizgileri ekleme
- **Yakinlasma (zoom):** Belirli zaman araligini secebilme
- **CSV export:** Grafik verisini CSV olarak indirme
- **Cesitli grafik tipleri:** Bar/area/scatter
- **Karsilastirma:** Birden fazla cihazi ayni grafikte gosterme

### Onemli Notlar

- Veriler **RAM'de tutulur**, guc kesintisinde silinir (yalnizca son 5 dk)
- Cihaz reboot oldugunda buffer sifirlanir, yeni baslar
- 5 sn'lik aralik debug yuku acisindan optimum (1 sn aralikla cok IO, 30 sn ile cok az veri)
- CO2 sensoru bagli degilse `co2[]` dizisinde 0 deger gorunur

---

## 19. OTA Firmware Guncelleme

Iki farkli OTA yontemi destekleniyor:

| Yontem | Adres | Kim icin? | Akis |
|--------|-------|-----------|------|
| **Yerel (push)** | `/update` | Gelistirme — yerel agdan `.bin` yukle | Tarayicidan dosya sec, ESP32'ye yukle |
| **Internet (pull)** | `/ota` | Son kullanici — tek tikla guncelle | ESP32 GitHub'dan indirir, SHA256 dogrular |

### 15.1. Yerel OTA (push) — `/update`

1. Tarayicinizdan `http://<IP>/update` adresine gidin
2. Arduino IDE'den derlediginiz `.bin` dosyasini secin
3. "Guncelle" butonuna basin
4. Yukleme tamamlaninca ESP32 otomatik yeniden baslar

### 15.2. Internet Uzerinden OTA (pull) — `/ota`

Kullanicinin yapmasi gereken **sadece "Guncelleme Kontrol Et" butonuna basmak.** Cihaz, internet uzerinden version dosyasini ceker, kendi surumuyle karsilastirir, gerekirse `.bin` indirir, SHA256 ile dogrular ve flash'lar.

**Akis (cihaz tarafi):**

```
[Kullanici "Kontrol Et" basar]
        |
        v
GET <updateUrl>/version.json   (HTTPS)
        |
        v
remote.version > 1.0.0  ?
        |--- evet ---> "Yeni surum mevcut" + changelog gosterilir
        |
        v
[Kullanici "Indir ve Kur" basar]
        |
        v
GET firmware.bin   (chunk chunk, SHA256 streaming)
        |
        v
hesaplanan SHA256 == remote.sha256 ?
        |--- evet ---> Update.end(true) + reboot
```

**Sunucu tarafindaki `version.json` formati:**

```json
{
  "version": "1.0.1",
  "url": "https://github.com/kullanici/repo/releases/download/v1.0.1/firmware.bin",
  "sha256": "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08",
  "size": 1180432,
  "buildDate": "Apr 21 2026",
  "changelog": "- PID auto-tune iyilestirildi\n- Captive portal Firefox uyumu duzeltildi"
}
```

| Alan | Zorunlu | Aciklama |
|------|:-------:|----------|
| `version` | Evet | Semver formati: `MAJOR.MINOR.PATCH` (orn `1.0.1`) |
| `url` | Evet | `.bin` dosyasinin direkt indirme adresi (HTTPS onerilir) |
| `sha256` | Evet | `.bin`'in SHA256 ozeti (lowercase, 64 hex karakter). Linux: `sha256sum firmware.bin` |
| `size` | Hayir | Bilgi amacli — UI'de gosterilir |
| `buildDate` | Hayir | Bilgi amacli — UI'de gosterilir |
| `changelog` | Hayir | UI'de gosterilen degisiklik notu |

### 15.3. Tipik kurulum: GitHub Releases

1. Yeni surum icin firmware'i derleyin → `KuluckaMakinesi.ino.bin` olusur (Arduino IDE: `Sketch → Export Compiled Binary`)
2. SHA256 hesaplayin:
   ```bash
   sha256sum KuluckaMakinesi.ino.bin
   ```
3. GitHub repo'da yeni Release olusturun (orn `v1.0.1`), `.bin` dosyasini asset olarak yukleyin
4. Repo'da `version.json` dosyasini guncelleyin (yukaridaki formatta) ve commit edin
5. `version.json`'un raw URL'ini cihaza tanitlayin:
   - Tarayici: `http://<IP>/ota` → "version.json URL" alanina yapistir → "URL'i Kaydet"
   - Veya REST: `POST /api/ota/url` (form-data: `url=https://raw.githubusercontent.com/USER/REPO/main/version.json`)
6. Cihazdan "Guncelleme Kontrol Et" → "Indir ve Kur"

> **Ipucu:** `version.json`'i `main` branch'inde tutun, `.bin`'i Release asset olarak yukleyin. Boylece firmware buyuk dosyalari git geçmisini kirletmez.

### 15.4. REST endpoint'leri (programatik kullanim)

| Method | Path | Aciklama |
|--------|------|----------|
| GET  | `/api/ota/status` | Mevcut durum: `state`, `progress`, `remoteVersion`, `updateUrl`, `changelog`, `error` |
| POST | `/api/ota/check` | `version.json` indirme + semver karsilastirma tetikleyici |
| POST | `/api/ota/pull` | `.bin` indir + SHA256 dogrula + flash + reboot (yalniz `STATE_UPDATE_AVAILABLE` durumunda calisir) |
| POST | `/api/ota/url` | `version.json` adresini NVS'e kaydet — form: `url=...` |

`state` degerleri: `0=IDLE`, `1=CHECKING`, `2=UPDATE_AVAILABLE`, `3=NO_UPDATE`, `4=DOWNLOADING`, `5=VERIFYING`, `6=SUCCESS`, `7=ERROR`

### 15.5. Guvenlik

- **Transport:** HTTPS (WiFiClientSecure) — version.json ve .bin TLS uzerinden indirilir
- **Butunluk:** SHA256 ozeti karsilastirmasi — indirilen .bin sunucudaki ile bit bit ayni degilse flash islemi iptal edilir
- **Onemli sinir:** Bu surum TLS sertifika dogrulamasi yapmaz (`setInsecure`). Bu, **MITM saldirisina karsi tam koruma saglamaz** (saldirgan trafigi sahte sertifika ile dinleyebilir, ancak SHA256 dogrulamasi nedeniyle kotu firmware'i flash ettiremez)
- **Daha siki guvenlik icin onerilen iyilestirme:** `version.json`'u Ed25519 ile imzalayin, public key'i firmware'e gomun, indirmeden once imzayi dogrulayin

### 15.6. Uyarilar (her iki yontem icin)

- Guncelleme sirasinda kulucka durumu NVS'de kayitli oldugundan, yeni firmware basladiktan sonra kurtarma otomatik calisir
- **Yanlis firmware yuklerseniz cihaz bozulabilir** — derleme ortaminizi dogrulayin (board: ESP32 Dev Module, partition: Huge APP)
- Guncelleme sirasinda guc **KESMEYIN** — flash'in yarida kalmasi cihazi brick'leyebilir
- Internet OTA'sinda indirme suresi internet hizina baglidir (5–60 sn). Bu sirada ana donguden web istegi cevaplanmaz; UI ekrani polling ile durumu gosterir
- Yan yana birden cok cihaz varsa **her cihaz icin ayri `version.json`** kullanmaniza gerek yok — `deviceId` (`KM-xxxxxxxx`) firmware'in icine gomulu olmadigi icin tek bir `.bin` tum cihazlarda calisir

---

## 20. Sorun Giderme

### Sicaklik Yukselmiyor
1. Isitici rolesini kontrol edin (PCF8574 P0)
2. Web arayuzunden `heaterPWM` degerini kontrol edin (> 0 olmali)
3. PID parametrelerini kontrol edin (`GET /api/status` → kp, ki, kd)
4. Auto-tune basarisiz olduysa: manuel PID ayari yapin (tavuk icin: Kp=20, Ki=0.8, Kd=5)

### Sensor Okumuyor
1. I2C kablolarini kontrol edin (SDA=IO32, SCL=IO25)
2. MUX (Grove 8 kanal I2C MUX) LED'ini kontrol edin
3. `I2CScanner` projesini yukleyerek tum I2C adreslerini tarayin
4. Tek sensor calismiyorsa diger sensor otomatik devreye girer

### Ekran Dokunmatik Tepki Vermiyor
1. `TOUCH_CS` (IO33) ve `TOUCH_IRQ` (IO36) baglantilarini kontrol edin
2. Config.h'deki kalibrasyon degerlerini ayarlayin (`TOUCH_CAL_*`)

### WiFi Baglanmiyor
1. Telefon/laptopla `Kulucka-<MAC8>` AP'sine baglanin (sifre: `kulucka123`)
2. **Captive portal otomatik acilmazsa** manuel: `http://192.168.4.1/portal`
3. "Aglari Tara" → ev WiFi'nizi secin → sifre girin → "Baglan"
4. Cihaz birkac saniye icinde baglanir, ESP32 bilgileri NVS'e kaydeder
5. Reboot gerekmez; STA bagli kalir, AP+captive portal kapanir

### Captive Portal Acilmiyor
- Bazi Android surumlerinde captive notifikasyon gecikmeli gelir; manuel olarak `http://192.168.4.1/portal` adresine gidin
- Mobil veriyi kapatin (cihaz captive'i atlamak icin mobil veri kullaniyor olabilir)
- Tarayicinizi temizleyin (cache'lenmis 204 cevaplari sorun cikarabilir)

### Yan Yana Birden Fazla Cihaz Var, Hangisi Hangisi Karistim
- AP SSID listesinde her cihaz `Kulucka-<MAC8>` olarak gorunur (MAC ekinden ayirt edilir)
- mDNS ile her cihaz `kulucka-<mac8>.local` adresinden erisilir
- Web arayuzu basliginda ve `/api/identity` cevabinda `deviceName` alani vardir
- `POST /api/device-name` ile her cihaza anlamli bir isim atayin (orn: "Tavuk-1", "Bildircin-2")

### Guc Kurtarma Calismiyor
1. RTC pilini kontrol edin (CR2032) - pil bitmisse tarih sifirlanir
2. NVS'de gecerli veri var mi kontrol edin (Serial Monitor'dan log'lara bakin)
3. Timestamp dogrulamasi basarisiz olabilir (RTC tarihi cok eski/gelecekte)

---

## 21. Teknik Referans

### Varsayilan Konfigürasyon Degerleri

| Parametre | Deger | Aciklama |
|-----------|-------|----------|
| HEATER_WINDOW_MS | 10000 | Isitici role pencere suresi |
| RELAY_MIN_ON_MS | 500 | Min role ACIK suresi |
| RELAY_MIN_OFF_MS | 500 | Min role KAPALI suresi |
| RELAY_COOLDOWN_MS | 2000 | Nemlendirici anahtarlama arasi |
| PID_DEFAULT_KP | 20.0 | Varsayilan Kp |
| PID_DEFAULT_KI | 0.8 | Varsayilan Ki |
| PID_DEFAULT_KD | 5.0 | Varsayilan Kd |
| AUTOTUNE_CYCLES | 6 | Auto-tune salinim sayisi |
| EMA_ALPHA | 0.2 | Sensor filtre katsayisi |
| SPIKE_THRESHOLD | 2.0 C | Ani sicrama esigi |
| SENSOR_VALID_TEMP_MIN | 15.0 C | Gecerli okuma alt siniri |
| SENSOR_VALID_TEMP_MAX | 50.0 C | Gecerli okuma ust siniri |
| SENSOR_STALE_TIMEOUT_MS | 15000 | Bayat veri zamanlayici |
| SAFETY_TEMP_MAX | 40.0 C | Acil kapatma ust sinir |
| SAFETY_TEMP_CRITICAL_LOW | 25.0 C | Acil kapatma alt sinir |
| SAFETY_LOW_TEMP_DURATION | 120000 | Alt sicaklik bekleme (2dk) |
| SAFETY_SENSOR_FAIL_COUNT | 5 | Ardisik sensor hatasi esigi |
| FUSION_TEMP_TOLERANCE | 0.5 C | Cift sensor uyum esigi |
| LOOP_INTERVAL | 2000 | Ana dongu araligi |
| WATCHDOG_TIMEOUT | 8 sn | Watchdog zamanlayici |

### NVS'de Saklanan Veriler

| Namespace | Anahtar | Tur | Aciklama |
|-----------|---------|-----|----------|
| (default) | profile | uint8 | Secili profil indexi |
| (default) | startTime | uint32 | Kulucka baslangic timestamp |
| (default) | running | bool | Aktif mi |
| (default) | valid | bool | NVS verisi gecerli mi |
| (default) | kp, ki, kd | float | PID parametreleri |
| (default) | calT1, calH1 | float | Sensor 1 kalibrasyon offset |
| (default) | calT2, calH2 | float | Sensor 2 kalibrasyon offset |
| (default) | ssid, pass | string | WiFi bilgileri |
| (default) | cpCount, cpData, cpSchemaVer | blob | Ozel profiller (v2 schema) |
| `device`  | name | string | Kullanici tanimli cihaz ismi (DeviceIdentity) |
| `otapull` | url  | string | version.json adresi (internet OTA) — `/api/ota/url` ile yazilir |

### Dosya Yapisi

```
KuluckaMakinesi_Esp32/
  KuluckaMakinesi/
    KuluckaMakinesi.ino    - Ana giris noktasi
    Config.h               - Tum yapilandirma sabitleri
    AnimalProfiles.h       - 11 hayvan profili (gradyanli)
    IncubationService.*    - Ana orkestrator (tum alt sistemleri yonetir)
    SensorManager.*        - SHT40 + SHT30 fuzyon + kalibrasyon
    EggIRSensor.*          - MLX90614 yerel yumurta IR sicakligi (MUX CH4)
    EggTempService.*       - Uzak yumurta IR sicakligi (HTTP, yedek kaynak)
    PIDController.*        - PID + Ziegler-Nichols auto-tuning
    PhaseManager.*         - Evre yonetimi + gradyan interpolasyon
    HeaterDriver.*         - Isitici time-proportional + role korumasi
    HumidifierDriver.*     - Nemlendirici + cooldown korumasi
    FanController.*        - Sicakliga bagli fan hiz kontrolu
    FanDriver.*            - Fan PWM surucusu
    TurnerDriver.*         - Yumurta cevirme motor kontrolu
    RelayBoard.*           - PCF8574 4'lu role yonetimi
    I2CMux.*               - Grove 8 kanal I2C multiplexer (TCA9548A)
    RTCManager.*           - DS3231 gercek zamanli saat
    AlarmService.*         - Alarm yonetimi + gecmis
    SafetyService.*        - Cok katmanli guvenlik
    PersistentStorage.*    - NVS kalici depolama (kurtarma + kalibrasyon)
    StorageService.*       - Ayarlar + ozel profiller + veri log
    DisplayManager.*       - 3.2" TFT dokunmatik ekran
    WebService.*           - WiFi AP/STA + AsyncWebServer
    web_server.*           - PROGMEM HTML/CSS/JS sayfalar
    OTAService.*           - Yerel OTA (push) — tarayicidan .bin yukleme
    OTAUpdater.*           - Internet uzerinden OTA (pull) — version.json + SHA256
    DeviceIdentity.*       - Cihaz kimligi (deviceId, isim, FW versiyonu, AP SSID, mDNS)
  I2CScanner/              - I2C cihaz tarama araci
```

---

*Bu dokuman, KuluckaMakinesi ESP32 firmware v1.0 icin hazirlanmistir.*
