gy906 kızılötesi sıcaklık sensörü
ESP32-C3 Mini WiFi Bluetooth Board
--------veri tabanı -------------
-veritabanı docker ortamında çalışacak
-veritabanı olarak mysql kullanılacak
-çoklu kullanıcı olacak ve yetiklerine göre verileri görebilecek üç tane rol olacak superadmin,admin ve kullanıcı olacak 
-bir kişinin birden fazla cihazı olabilir.
-kullanıcı login için tabloları oluşturulacak
Veritabanı için tutulması gereken tüm bilgileri kategorize ettim. Önerilen schema şöyle:

📊 Veritabanı Schema Önerisi
1. devices — Cihaz Kimlik Bilgileri
Alan	Tip	Açıklama
device_id (PK)	TEXT	KM-A4C138F2 (MAC tabanlı)
device_name	TEXT	Kullanıcı tanımlı isim
mac_address	TEXT	ESP32 MAC
firmware_version	TEXT	"3.0.0"
created_at	DATETIME	İlk kayıt
last_seen	DATETIME	Son heartbeat
location	TEXT	Opsiyonel (lat,lng veya açıklama)
2. incubation_batches — Kuluçka Oturumları
Alan	Tip	Açıklama
batch_id (PK)	INTEGER	Auto-increment
device_id (FK)	TEXT	→ devices
profile_index	INT	0-10 (Tavuk, Bıldırcın, vb.)
profile_name	TEXT	Snapshot (profil değişse de korunur)
egg_count	INT	Yumurta sayısı
start_unix	BIGINT	Başlangıç timestamp
expected_hatch_unix	BIGINT	Tahmini çıkım
actual_complete_unix	BIGINT	Gerçek tamamlanma
hatched_count	INT	Çıkan civciv (manuel girdi)
success_rate	FLOAT	hatched/egg_count
state	TEXT	running/paused/completed/aborted
notes	TEXT	Kullanıcı notu
3. sensor_readings — Sensör Verileri (Time-Series)
Alan	Tip	Açıklama
id (PK)	BIGINT	Auto-increment
device_id (FK)	TEXT	
batch_id (FK)	INT	NULL olabilir (cihaz açıkken kuluçka yoksa)
ts	BIGINT	Unix timestamp
temp_c	FLOAT	Sıcaklık (fuzyon)
temp1_c	FLOAT	SHT30 #1 ham
temp2_c	FLOAT	SHT30 #2 ham
humidity	FLOAT	Nem (fuzyon)
humidity1	FLOAT	SHT30 #1 ham
humidity2	FLOAT	SHT30 #2 ham
co2_ppm	INT	CO2
egg_temp_c	FLOAT	IR yumurta sıcaklığı
rtc_temp_c	FLOAT	DS3231 modül sıcaklığı
Index:		(device_id, ts)
4. control_outputs — Kontrol Çıkışları (Time-Series)
Alan	Tip	Açıklama
id (PK)	BIGINT	
device_id (FK)	TEXT	
batch_id (FK)	INT	
ts	BIGINT	
heater_pwm	INT	0-255
fan_pwm	INT	0-255
humidifier_on	BOOL	
turner_on	BOOL	
turner_direction	INT	0/1
cool_spray_on	BOOL	
pid_setpoint	FLOAT	Hedef sıcaklık
pid_p	FLOAT	P term
pid_i	FLOAT	I term
pid_d	FLOAT	D term
pid_saturated	BOOL	
Index:		(device_id, ts)
5. phase_log — Faz Geçiş Geçmişi
Alan	Tip	Açıklama
id (PK)	BIGINT	
batch_id (FK)	INT	
phase_index	INT	0,1,2...
phase_name	TEXT	"Erken Gelisim"
start_unix	BIGINT	
end_unix	BIGINT	NULL = aktif
start_day	INT	
end_day	INT	NULL = aktif
duration_sec	BIGINT	Bitince hesaplanır
target_temp	FLOAT	Bu fazın hedef sıcaklığı
target_hum_low	FLOAT	
target_hum_high	FLOAT	
turning_enabled	BOOL	
transition_reason	TEXT	"auto" / "manual" / "lockdown"
6. alarms — Alarm Geçmişi
Alan	Tip	Açıklama
id (PK)	BIGINT	
device_id (FK)	TEXT	
batch_id (FK)	INT	NULL olabilir
triggered_at	BIGINT	
cleared_at	BIGINT	NULL = aktif
alarm_type	TEXT	TEMP_HIGH, CANDLING_DUE, vb.
alarm_code	INT	enum değeri
message	TEXT	
severity	TEXT	info/warning/critical
temp_at_trigger	FLOAT	Snapshot — tetiklenme anındaki sıcaklık
hum_at_trigger	FLOAT	
co2_at_trigger	INT	
user_action	TEXT	NULL/ack/snooze/dismiss
action_at	BIGINT	Aksiyon zamanı
7. candling_events — Döl Kontrol Olayları
Alan	Tip	Açıklama
id (PK)	BIGINT	
batch_id (FK)	INT	
check_index	INT	0,1,2,3
check_label	TEXT	"1.Kontrol (%25)"
check_day	INT	
check_unix	BIGINT	
notification_sent_at	BIGINT	
user_completed_at	BIGINT	NULL = yapmadı
eggs_fertile	INT	Manuel girdi: kaç fertil
eggs_infertile	INT	Manuel girdi: kaç boş
eggs_dead	INT	Manuel girdi: kaç ölü
notes	TEXT	Kullanıcı gözlem notu
photo_url	TEXT	Opsiyonel kamera fotosu
8. system_events — Sistem Olayları
Alan	Tip	Açıklama
id (PK)	BIGINT	
device_id (FK)	TEXT	
ts	BIGINT	
event_type	TEXT	boot/power_recovery/cleaning_start/cleaning_end/pause/resume/emergency/ota_start/ota_complete/wifi_disconnect/wdt_event
details	JSON	Event'e özel detaylar
free_heap_kb	INT	O anki RAM durumu
uptime_sec	BIGINT	
reset_reason	TEXT	Boot eventleri için
9. calibrations — Sensör Kalibrasyon
Alan	Tip	Açıklama
id (PK)	BIGINT	
device_id (FK)	TEXT	
applied_at	BIGINT	
temp1_offset	FLOAT	
hum1_offset	FLOAT	
temp2_offset	FLOAT	
hum2_offset	FLOAT	
reference_device	TEXT	"Testo 175H1 vb."
applied_by	TEXT	"user" / "factory"
10. pid_history — PID Parametre Geçmişi
Alan	Tip	Açıklama
id (PK)	BIGINT	
device_id (FK)	TEXT	
batch_id (FK)	INT	
applied_at	BIGINT	
kp	FLOAT	
ki	FLOAT	
kd	FLOAT	
ku	FLOAT	Auto-tune sonucu
tu	FLOAT	
tuning_rule	TEXT	classic/pessen/tyreus_luyben/no_overshoot
source	TEXT	autotune/manual/factory
11. profile_overrides — Kullanıcı Profil Düzenlemeleri
Alan	Tip	Açıklama
id (PK)	BIGINT	
device_id (FK)	TEXT	
profile_index	INT	
phase_index	INT	
field_name	TEXT	"temperature","humidityLow","turningInterval"
original_value	FLOAT	
override_value	FLOAT	
applied_at	BIGINT	
12. relay_wear — Röle Aşınma (Cycle Counter)
Alan	Tip	Açıklama
id (PK)	BIGINT	
device_id (FK)	TEXT	
batch_id (FK)	INT	
relay_name	TEXT	heater/humidifier/turner_power/turner_direction
on_cycles	BIGINT	Toplam ON sayısı
total_on_sec	BIGINT	Toplam çalışma süresi
recorded_at	BIGINT	
13. network_log — WiFi Geçmişi
Alan	Tip	Açıklama
id (PK)	BIGINT	
device_id (FK)	TEXT	
event	TEXT	connect/disconnect/scan
ssid	TEXT	
ip_address	TEXT	
rssi_dbm	INT	Sinyal gücü
ts	BIGINT	
14. rtc_health — RTC İzleme
Alan	Tip	Açıklama
id (PK)	BIGINT	
device_id (FK)	TEXT	
ts	BIGINT	
status	TEXT	OK/BATTERY_LOW/INVALID/I2C_FAIL/FALLBACK
module_temp_c	FLOAT	
ms_since_sync	INT	
ntp_synced	BOOL	NTP'den senkronize mi
15. user_settings — Cihaz Ayarları
Alan	Tip	Açıklama
device_id (PK)	TEXT	
egg_temp_sensor_ip	TEXT	IR sensor IP
egg_temp_sensor_enabled	BOOL	
time_zone_offset_min	INT	Türkiye için 180
language	TEXT	tr/en
notification_telegram_token	TEXT	(encrypted)
notification_telegram_chat_id	TEXT	
notification_email	TEXT	
theme	TEXT	dark/light
🗂️ Aggregation Tabloları (Performans İçin)
Time-series veriler hızla şişer. Aggregation öneriler:

sensor_readings_5min — 5 Dakikalık Özet

ts_bucket, batch_id,
temp_avg, temp_min, temp_max, temp_stddev,
hum_avg, hum_min, hum_max,
co2_avg, co2_max
sensor_readings_hourly — Saatlik Özet
sensor_readings_daily — Günlük Özet (uzun vadeli analiz için)
🔑 Kritik İlişkiler

devices (1) ─── (N) incubation_batches
incubation_batches (1) ─── (N) sensor_readings
incubation_batches (1) ─── (N) control_outputs
incubation_batches (1) ─── (N) phase_log
incubation_batches (1) ─── (N) alarms
incubation_batches (1) ─── (N) candling_events
devices (1) ─── (N) system_events
devices (1) ─── (N) calibrations
devices (1) ─── (N) pid_history
📈 Önerilen Retention (Disk Tasarrufu)
Tablo	Tutma Süresi
sensor_readings (raw 5sn)	30 gün
sensor_readings_5min	1 yıl
sensor_readings_hourly	Sınırsız
alarms, phase_log, candling_events	Sınırsız
incubation_batches	Sınırsız
network_log, rtc_health	90 gün
system_events	1 yıl
💾 Toplam Veri Tahmini
Bir kuluçka (21 gün) için:

Sensor readings: ~362K satır (5 sn × 21 gün) ≈ 30 MB
Control outputs: ~362K satır ≈ 20 MB
Phase log: 2-4 satır
Alarms: ~10-50 satır
Candling: 4 satır
Toplam: ~50 MB / kuluçka
5 dakikalık aggregation ile: ~1.5 MB / kuluçka (efficient!)

🔌 Mevcut API'lardan DB Mapping
Mevcut Endpoint	Hedef Tablo
GET /api/status	sensor_readings + control_outputs (snapshot)
GET /api/history	sensor_readings (60 okuma)
GET /api/phase-log	phase_log
GET /api/alarm	alarms
GET /api/calibration	calibrations
