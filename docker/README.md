# Docker Test Ortamı

Bu klasör, web arayüzünü (index.html) test etmek için Docker ortamı içerir.
**Arduino kodlarını etkilemez.**

## Hızlı Başlangıç

```bash
cd docker
docker-compose up --build
```

Tarayıcıda aç: **http://localhost:8080**

## Yapı

```
docker/
├── docker-compose.yml   # Docker servisleri
├── Dockerfile           # Mock API container
├── nginx.conf           # Web sunucu config
├── mock-server.js       # ESP32 API simülasyonu
├── package.json         # Node.js bağımlılıkları
└── README.md            # Bu dosya
```

## Servisler

| Servis | Port | Açıklama |
|--------|------|----------|
| web    | 8080 | Nginx - statik dosyalar (data/) |
| api    | 3000 | Mock API - ESP32 simülasyonu |

## API Endpoint'leri

Mock sunucu şu endpoint'leri simüle eder:

- `GET /api/status` - Sistem durumu
- `GET /api/profiles` - Profil listesi
- `POST /api/profile` - Profil seç
- `POST /api/control` - Başlat/Duraklat/Devam/Durdur
- `POST /api/pid` - PID parametreleri
- `POST /api/humidity` - Nem eşikleri
- `POST /api/safety` - Güvenlik sıfırla
- `GET /api/custom-profiles` - Özel profiller
- `POST /api/custom-profile` - Özel profil kaydet
- `DELETE /api/custom-profile` - Özel profil sil
- `POST /api/save-settings` - Ayarları kaydet
- `GET /api/load-settings` - Ayarları yükle
- `POST /api/wifi` - WiFi kaydet
- `GET /api/log` - Log indir
- `DELETE /api/log` - Log temizle

## Durdurma

```bash
docker-compose down
```

## Not

Bu ortam sadece UI geliştirme ve test içindir.
Gerçek ESP32 donanımı ile test için Arduino IDE kullanın.
