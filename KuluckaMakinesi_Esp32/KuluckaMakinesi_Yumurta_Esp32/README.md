# KuluckaMakinesi_Yumurta_Esp32

GY-906 (MLX90614) IR termometre ile yumurta yüzey sıcaklığını okur ve WiFi üzerinden JSON endpoint olarak yayar.

---

## Donanım

| Bileşen | Model |
|---|---|
| Mikrodenetleyici | ESP32-C3 Mini |
| IR Termometre | GY-906 (MLX90614) |

### Bağlantı Şeması

```
GY-906          ESP32-C3 Mini
-------         -------------
VIN    ──────►  3.3V
GND    ──────►  GND
SDA    ──────►  GPIO8
SCL    ──────►  GPIO9
```

> **Not:** MLX90614, 3.3V ile çalışır. 5V bağlamayın!  
> I2C hat dirençleri (4.7kΩ) genellikle GY-906 modülünde zaten bulunur.

---

## Kurulum

### 1. Arduino IDE Kütüphaneleri

Library Manager'dan yükleyin:
- `Adafruit MLX90614 Library` (Adafruit)
- `Adafruit BusIO` (bağımlılık, otomatik gelir)

### 2. Board Ayarı

- Board: **ESP32C3 Dev Module**
- Upload Speed: 921600
- USB CDC On Boot: Enabled (Serial için)

### 3. Config.h

```cpp
#define WIFI_SSID      "WIFI_ADINIZI_YAZIN"
#define WIFI_PASSWORD  "WIFI_SIFRENIZI_YAZIN"
```

---

## API Endpoint'leri

### `GET /api/egg`

Yumurta sıcaklığı (ana endpoint).

**Yanıt:**
```json
{
  "eggTemp"    : 37.45,
  "ambientTemp": 25.10,
  "valid"      : true,
  "uptime"     : 3600,
  "ip"         : "192.168.1.50"
}
```

| Alan | Açıklama |
|---|---|
| `eggTemp` | Yumurta yüzey sıcaklığı (°C), son 10 okumanın ortalaması |
| `ambientTemp` | Sensör ortam sıcaklığı (°C) |
| `valid` | `false` ise sensör hatası var |
| `uptime` | Cihaz çalışma süresi (saniye) |

---

### `GET /api/status`

Cihaz ve bağlantı durumu.

**Yanıt:**
```json
{
  "device"      : "KuluckaMakinesi_Yumurta",
  "sensor"      : "MLX90614",
  "sensorOK"    : true,
  "wifiRSSI"    : -55,
  "ip"          : "192.168.1.50",
  "uptime"      : 3600,
  "readInterval": 500
}
```

---

## Ana Projede Kullanım

`KuluckaMakinesi` projesinde bu endpoint'i HTTP GET ile çağırın:

```cpp
// Örnek: HTTPClient ile yumurta sıcaklığını al
HTTPClient http;
http.begin("http://192.168.1.50/api/egg");  // veya http://yumurta.local/api/egg
int code = http.GET();
if (code == 200) {
    String body = http.getString();
    // JSON parse → eggTemp değerini kullan
}
http.end();
```

---

## Kalibrasyon

`Config.h` içindeki `EGG_EMISSIVITY_CORRECTION` değerini ayarlayın:

```cpp
#define EGG_EMISSIVITY_CORRECTION  -0.3f  // Ölçülen - Gerçek fark
```

Kalibrasyon için:
1. Doğru sıcaklığı termokupul veya referans termometre ile ölçün
2. Farkı bu sabite girin (pozitif veya negatif)

---

## mDNS

Aynı ağdaki diğer cihazlar IP yerine hostname kullanabilir:

```
http://yumurta.local/api/egg
```

> Windows'ta mDNS için **Bonjour** (iTunes ile gelir) gerekebilir.
