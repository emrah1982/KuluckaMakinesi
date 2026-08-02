PROJE: ESP32 TABANLI AKILLI KULUÇKA KONTROL SİSTEMİ (TİCARİ SEVİYE)

AMAÇ:
ESP32 tabanlı, sıcaklık, nem ve zaman kontrollü, fan destekli, ultrasonik nemlendirme ve PID kontrollü ısıtma sistemi içeren tam otomatik kuluçka makinesi yazılımı geliştirilmesi.

---

KULLANILACAK TEKNOLOJİLER:

* Geliştirme ortamı: Arduino IDE 2.x
* Programlama dili: Arduino (C++)
* Donanım: ESP32 Dev Module
* Sensörler:

  * 2 adet SHT31 (I2C)
  * 1 adet DS3231 RTC
* Haberleşme:

  * I2C
  * WiFi AP+STA (hotspot + mevcut ağ eş zamanlı)
* PWM:

  * ESP32 LEDC donanımsal PWM
* Kontrol:

  * PID algoritması (manuel + auto tuning)
* Web:

  * ESP32 AsyncWebServer (dahili)
  * HTML/CSS/JS (SPIFFS üzerinden)

---

KÜTÜPHANE BAĞIMLILIKLARI (Arduino IDE Library Manager):

* Adafruit SHT31 Library
* RTClib (Adafruit)
* Adafruit BusIO
* Adafruit Unified Sensor
* ESPAsyncWebServer
* AsyncTCP

Arduino IDE Kart Ayarı:
* Board: ESP32 Dev Module
* Upload Speed: 921600
* Flash Size: 4MB (default)
* Partition Scheme: Default 4MB with spiffs
* Board Manager URL: https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

---

DONANIM BİLEŞENLERİ:

1. ISITICI SİSTEMİ

* 220V rezistans veya ısıtıcı
* SSR (Solid State Relay) ile kontrol edilecek
* PWM ile sürülecek (ON/OFF yasak)

---

2. NEMLENDİRME SİSTEMİ

* Ultrasonik nemlendirici (mist maker)
* Röle veya MOSFET ile kontrol
* Sürekli çalışmayacak (pulse mantığı)
* Gecikmeli kapanma olacak (yoğuşma önleme)

---

3. FAN SİSTEMİ

* 12V fan
* PWM ile hız kontrolü yapılacak
* Sürekli çalışacak ama hız değişken olacak
* Amaç:

  * Isı homojen dağılım
  * Nem dengesi

---

SİSTEM GEREKSİNİMLERİ:

1. SICAKLIK KONTROLÜ

* PID algoritması kullanılacak
* PWM ile SSR üzerinden ısıtıcı sürülecek
* PID özellikleri:

  * Kp, Ki, Kd parametreleri
  * Auto PID tuning (Ziegler–Nichols)
  * Integral windup koruma

---

2. SENSÖR YÖNETİMİ

* 2 adet SHT31 kullanılacak
* Özellikler:

  * Ortalama alma
  * Sensör arızasında fallback
  * Veri filtreleme:

    * EMA (Exponential Moving Average)
    * Spike (ani sıçrama) engelleme

---

3. NEM KONTROLÜ

* Alt ve üst eşik değerleri olacak
* Ultrasonik nemlendirici:

  * Röle ile kontrol
  * Gecikmeli kapatma (en az 10 saniye)
  * Yoğuşma önleme mantığı

---

4. FAN KONTROLÜ

* PWM ile kontrol edilecek
* Sıcaklığa bağlı hız:

  * düşük sıcaklık → düşük hız
  * yüksek sıcaklık → yüksek hız
* Minimum hız her zaman olacak (fan durmayacak)

---

5. ZAMAN VE EVRE KONTROLÜ

* DS3231 RTC kullanılacak
* Gün bazlı evreler:

  * Gelişim evresi
  * Çıkım evresi
* Her evrede:

  * farklı sıcaklık
  * farklı nem

---

6. AUTO PID TUNING

* İlk çalıştırmada aktif olacak
* 5-6 salınım sonrası:

  * Kp, Ki, Kd hesaplanacak
* Sonrasında PID aktif olacak
* PID değerleri saklanabilir (EEPROM opsiyonel)

---

7. ALARM SİSTEMİ

* WiFi üzerinden uyarı
* Alarm durumları:

  * yüksek sıcaklık
  * düşük sıcaklık
  * sensör hatası
* Minimum:

  * Serial log
* Opsiyonel:

  * HTTP / MQTT / Telegram

---

8. GÜÇ VE GÜVENLİK

* SSR izolasyon
* 220V ve düşük voltaj ayrımı
* Fail-safe:

  * sensör hatasında sistemi kapatma
  * aşırı sıcaklıkta ısıtıcıyı kesme

---

9. KOD YAPISI

* Geliştirme ortamı: Arduino IDE
* Giriş noktası: KuluckaMakinesi.ino
* Modüler katmanlı mimari:

```
KuluckaMakinesi/
├── KuluckaMakinesi.ino              ← Ana giriş (setup + loop)
├── docker-compose.yml               ← Docker test ortamı
├── docker/
│   ├── Dockerfile                   ← Node.js Alpine imaj
│   └── mock-server.js               ← Dosya tabanlı test sunucu
├── data/                            ← SPIFFS web dosyaları
│   ├── index.html
│   ├── style.css
│   └── app.js
├── docs/
│   └── mimari.md
└── src/
    ├── config/
    │   ├── Config.h                 ← Tüm sabitler
    │   └── AnimalProfiles.h         ← 9 hayvan profili
    ├── hal/                         ← Donanım sürücüleri
    │   ├── SensorManager.h/.cpp
    │   ├── HeaterDriver.h/.cpp
    │   ├── FanDriver.h/.cpp
    │   ├── HumidifierDriver.h/.cpp
    │   └── RTCManager.h/.cpp
    ├── control/                     ← Kontrol algoritmaları
    │   ├── PIDController.h/.cpp
    │   ├── HumidityController.h/.cpp
    │   ├── FanController.h/.cpp
    │   └── PhaseManager.h/.cpp
    └── service/                     ← İş mantığı
        ├── IncubationService.h/.cpp ← Ana orkestrasyon
        ├── AlarmService.h/.cpp
        ├── SafetyService.h/.cpp
        ├── StorageService.h/.cpp    ← NVS + SPIFFS veri depolama
        └── WebService.h/.cpp        ← Web arayüz servisi
```

* Tüm sabitler config bölümünde
* Okunabilir ve genişletilebilir yapı

---

10. PERFORMANS

* loop süresi max: 2 saniye
* delay yerine millis tercih edilecek
* watchdog uyumlu yapı

---

11. HAYVAN SEÇMELİ AKILLI MENÜ

* ESP32 üzerinde hayvan seçim menüsü
* Desteklenen hayvanlar:

  * 🐔 Tavuk
  * 🐤 Bıldırcın
  * 🦢 Kaz
  * 🦆 Ördek
  * 🦃 Hindi
  * 🐦 Güvercin
  * 🦜 Papağan
  * 🐓 Sülün
  * 🦩 Deve Kuşu

* 📊 Her hayvana göre otomatik parametre yükleme:

  * Kuluçka süresi (gün)
  * Evre bazlı sıcaklık hedefleri (°C)
  * Evre bazlı nem aralıkları (%)
  * Yumurta çevirme programı
  * Çıkım evresi parametreleri

* Profiller AnimalProfiles.h dosyasında tanımlı
* Menüden hayvan seçildiğinde tüm parametreler otomatik yüklenir

---

EK ÖZELLİKLER:

12. WEB ARAYÜZ (ESP32 AsyncWebServer)

* ESP32 dahili WiFi ile web sunucu
* SPIFFS üzerinden statik dosya sunumu (HTML/CSS/JS)
* Responsive tasarım (mobil uyumlu - telefon/tablet)
* WiFi AP+STA modu (eş zamanlı çalışır):
  * AP (Hotspot): ESP32 kendi WiFi ağını açar → "KuluckaMakinesi" (192.168.4.1)
  * STA: Mevcut WiFi ağına bağlanır → router'dan IP alır
  * Telefon doğrudan ESP32 hotspot'a bağlanıp erişebilir (WiFi ağı gerekmez)
  * Hem AP hem STA aynı anda aktif, web arayüz her iki IP'den erişilebilir
  * Bağlı cihaz sayısı header'da görüntülenir
* Sayfalar:

  * Ana panel: Anlık sıcaklık, nem, evre, gün bilgisi
  * Kontrol: Profil seçimi, başlat/duraklat/durdur
  * PID ayarları: Kp, Ki, Kd değiştirebilme
  * Alarm geçmişi: Son alarm kayıtları
  * Sistem durumu: Sensör, uptime bilgileri

* REST API Endpoint'leri:

  * GET  /api/status           → Sistem durumu (JSON)
  * GET  /api/alarm            → Alarm bilgisi (JSON)
  * GET  /api/profiles         → Dahili profil listesi
  * POST /api/profile          → Profil seçimi (dahili veya özel)
  * POST /api/control          → Başlat/Duraklat/Durdur
  * POST /api/pid              → PID parametre güncelleme
  * POST /api/humidity         → Nem eşikleri güncelleme
  * POST /api/safety           → Güvenlik sıfırlama
  * GET  /api/custom-profiles  → Özel profil listesi
  * POST /api/custom-profile   → Özel profil ekle/güncelle
  * DEL  /api/custom-profile   → Özel profil sil
  * POST /api/save-settings    → Ayarları ESP hafızasına kaydet
  * GET  /api/load-settings    → Kayıtlı ayarları yükle
  * POST /api/wifi             → WiFi bilgilerini kaydet
  * GET  /api/log              → CSV log indir
  * DEL  /api/log              → Log temizle

* Otomatik yenileme: 2 saniyede bir AJAX ile veri çekme

* Docker Test Ortamı:

  * ESP32 donanımı olmadan web arayüzü geliştirme/test
  * Dosya tabanlı gerçek veri depolama (JSON + CSV)
  * PID benzeri sensör simülasyonu (hedef değere yaklaşan gerçekçi davranış)
  * Tüm API endpoint'leri dosyaya yazarak kalıcı çalışır
  * Docker Compose ile tek komutla çalıştırma:

```
docker-compose up --build
→ http://localhost:8080 adresinden erişim
```

  * Docker veri depolama:
    - db/settings.json: PID, nem, profil, WiFi ayarları
    - db/custom_profiles.json: Kullanıcı özel profilleri
    - db/log.csv: Veri logları
  * Docker named volume ile veriler kalıcı (container silinse bile korunur)
  * Volume mount: data/ klasörü canlı bağlı (hot-reload)
  * Geliştirme akışı: data/ içindeki HTML/CSS/JS dosyalarını düzenle → tarayıcıda yenile

---

13. OTA GÜNCELLEME

* Arduino OTA kütüphanesi ile kablosuz yazılım güncelleme
* Hostname: KuluckaMakinesi

---

14. VERİ LOGLAMA

* SPIFFS üzerinde CSV formatında log
* Log aralığı: 1 dakika
* Kaydedilen veriler: sıcaklık, nem, heater PWM, fan PWM, evre
* Maksimum log boyutu: 500KB (otomatik döngüsel)
* Web arayüzünden CSV indirme ve log temizleme

---

15. VERİTABANI VE VERİ DEPOLAMA (StorageService)

* ESP32 NVS (Preferences kütüphanesi):
  * PID parametreleri (Kp, Ki, Kd)
  * Seçili profil index'i
  * Hedef sıcaklık ve nem eşikleri
  * WiFi SSID ve şifre
  * Cihaz yeniden başlatıldığında otomatik yüklenir

* SPIFFS dosya sistemi:
  * /profiles.json: Kullanıcının oluşturduğu özel hayvan profilleri (maks 10)
  * /log.csv: Periyodik veri logları
  * /settings.json: Yedek ayar dosyası

* Kullanıcı Özel Profil Sistemi:
  * Web arayüzünden yeni hayvan profili oluşturma
  * Ad, toplam gün, 1-4 evre tanımı
  * Her evre: başlangıç/bitiş günü, sıcaklık, nem esikleri, çevirme
  * Profilleri kullanma, güncelleme ve silme
  * Tüm profiller ESP32 flash hafızasına kalıcı olarak kaydedilir

---

TESLİMAT:

* Çalışan Arduino IDE projesi (KuluckaMakinesi.ino + src/ modüler yapı)
* SPIFFS web arayüz dosyaları (data/ klasörü)
* Docker test ortamı (docker-compose + dosya tabanlı veri depolama)
* Kullanılan kütüphaneler listesi (Arduino IDE Library Manager uyumlu)
* Bağlantı şeması açıklaması
* PID dokümantasyonu
* 9 dahili + 10 kullanıcı özel hayvan profili desteği
* NVS + SPIFFS veritabanı ile kalıcı veri depolama
* REST API ile tam CRUD özel profil yönetimi

---

NOT:
Bu sistem ticari kuluçka makinesi olarak kullanılacaktır.
Stabilite, hassasiyet ve güvenlik en kritik unsurlardır.
