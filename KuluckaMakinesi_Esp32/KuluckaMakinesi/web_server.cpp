#include "web_server.h"
#include "Config.h"

// ============================================================
//  PROGMEM Web Icerikleri - Otomatik olusturuldu
//  data/index.html, data/style.css, data/app.js dosyalarindan
//  SPIFFS gerektirmez, firmware ile birlikte yuklenir
// ============================================================

// -------------------- index.html --------------------
const char PAGE_INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="tr">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0,maximum-scale=1.0,user-scalable=no">
<title>Kulucka</title>
<link rel="stylesheet" href="/style.css">
</head>
<body>

<!-- HEADER -->
<header>
<div class="hdr-left">
    <span class="hdr-icon" id="pb-icon">&#128051;</span>
    <div>
        <div class="hdr-title" id="pb-name">---</div>
        <div class="hdr-sub" id="pb-detail">Gun - / -</div>
    </div>
</div>
<div class="hdr-right">
    <span class="badge" id="pb-state">---</span>
    <span class="dot" id="conn-dot"></span>
</div>
</header>

<!-- TAB BAR (alt nav - dokunmatik uyumlu) -->
<nav class="tab-bar" id="tab-bar">
<button class="tb active" onclick="switchTab('screen')">&#128250;<br><small>Ekran</small></button>
<button class="tb" onclick="switchTab('dash')">&#9670;<br><small>Durum</small></button>
<button class="tb" onclick="switchTab('ctrl')">&#9881;<br><small>Kontrol</small></button>
<button class="tb" onclick="switchTab('prof')">&#128051;<br><small>Profil</small></button>
<button class="tb" onclick="switchTab('set')">&#9878;<br><small>Ayar</small></button>
</nav>

<!-- ANA ICERIK -->
<main>

<!-- ===== EKRAN (Gorsel Dashboard) ===== -->
<div class="tab active" id="tab-screen">

<div class="screen-container">
    <!-- Uyari Banner -->
    <div class="screen-alert" id="screen-alert">
        <span class="alert-icon">&#9888;</span>
        <span class="alert-text" id="screen-alert-text">Sistem Normal</span>
    </div>
    
    <!-- Ana Gosterge Alani -->
    <div class="screen-main">
        <!-- Sicaklik Dairesi -->
        <div class="temp-circle" id="temp-circle">
            <svg class="temp-ring" viewBox="0 0 120 120">
                <circle class="ring-bg" cx="60" cy="60" r="54"/>
                <circle class="ring-fg" id="temp-ring-fg" cx="60" cy="60" r="54"/>
            </svg>
            <div class="temp-inner">
                <div class="temp-val" id="scr-temp">--.-</div>
                <div class="temp-unit">°C</div>
                <div class="temp-target" id="scr-temp-target">37.5°</div>
                <div class="temp-label">HEDEF SICAKLIK</div>
            </div>
        </div>
        
        <!-- Hayvan ve Nem -->
        <div class="screen-side">
            <!-- Hayvan Ikonu -->
            <div class="animal-box">
                <div class="animal-icon" id="scr-animal">&#128019;</div>
                <div class="animal-name" id="scr-animal-name">Tavuk</div>
            </div>
            
            <!-- Nem Gostergesi -->
            <div class="hum-box">
                <div class="hum-icon">&#128167;</div>
                <div class="hum-val" id="scr-hum">--</div>
                <div class="hum-unit">%</div>
            </div>
        </div>
    </div>
    
    <!-- Alt Bilgi Satiri -->
    <div class="screen-footer">
        <div class="scr-info">
            <span class="scr-day" id="scr-day">1</span>
            <span class="scr-day-label">. GUN</span>
        </div>
        <div class="scr-info">
            <span class="scr-phase" id="scr-phase">Gelisim</span>
        </div>
        <div class="scr-info">
            <span class="scr-remaining" id="scr-remaining">20</span>
            <span class="scr-day-label"> gun kaldi</span>
        </div>
        <div class="scr-info">
            <span class="scr-remaining" id="scr-egg">100</span>
            <span class="scr-day-label"> yumurta</span>
        </div>
    </div>
    
    <!-- Yumurta IR Sicaklik Karti -->
    <div class="egg-ir-card" id="egg-ir-card">
        <span class="egg-ir-icon">&#129370;</span>
        <div class="egg-ir-body">
            <div class="egg-ir-label">YUMURTA SICAKLIGI (IR)</div>
            <div class="egg-ir-val"><span id="scr-egg-temp">--.-</span><span class="egg-ir-unit">&#176;C</span></div>
        </div>
        <div class="egg-ir-status">
            <div class="egg-ir-badge nc" id="egg-ir-badge">Bagli degil</div>
            <div class="egg-ir-sub" id="egg-ir-sub">-</div>
        </div>
    </div>

    <!-- Cikis Durumu Ikonlari -->
    <div class="screen-outputs">
        <div class="out-icon" id="out-heater" title="Isitici">
            <span>&#128293;</span>
            <small>Isitici</small>
        </div>
        <div class="out-icon" id="out-fan" title="Fan">
            <span>&#127744;</span>
            <small>Fan</small>
        </div>
        <div class="out-icon" id="out-hum" title="Nemlendirici">
            <span>&#128167;</span>
            <small>Nem</small>
        </div>
        <div class="out-icon" id="out-turner" title="Cevirme">
            <span>&#128260;</span>
            <small>Cevir</small>
        </div>
    </div>
</div>

</div>

<!-- ===== DASHBOARD ===== -->
<div class="tab" id="tab-dash">

<div class="gauge-row">
    <div class="gauge temp">
        <div class="g-val" id="temp">--.-</div>
        <div class="g-lbl">&#176;C Sicaklik</div>
        <div class="g-sub" id="temp-target">Hedef: --.-</div>
    </div>
    <div class="gauge hum">
        <div class="g-val" id="hum">--.-</div>
        <div class="g-lbl">% Nem</div>
        <div class="g-sub" id="hum-target">Hedef: -- - --</div>
    </div>
</div>

<div class="info-row">
    <div class="info-box">
        <span class="i-lbl">Evre</span>
        <span class="i-val" id="phase">---</span>
        <span class="i-sub" id="phase-remaining">Evre bitis: - gun</span>
    </div>
    <div class="info-box">
        <span class="i-lbl">Bitise Kalan</span>
        <span class="i-val" id="remaining-days">-</span>
        <span class="i-sub" id="day-info">Gun - / -</span>
    </div>
    <div class="info-box">
        <span class="i-lbl">Durum</span>
        <span class="i-val" id="sys-state">---</span>
        <span class="i-sub" id="profile-name">---</span>
    </div>
    <div class="info-box">
        <span class="i-lbl">Yumurta</span>
        <span class="i-val" id="egg-count-dash">100</span>
        <span class="i-sub">adet</span>
    </div>
</div>

<div class="card">
    <div class="out-row">
        <span class="o-lbl">Isitici</span>
        <div class="bar"><div class="bar-f heat" id="heater-bar"></div></div>
        <span class="o-val" id="heater-pwm">0</span>
    </div>
    <div class="out-row">
        <span class="o-lbl">Fan</span>
        <div class="bar"><div class="bar-f fan" id="fan-bar"></div></div>
        <span class="o-val" id="fan-pwm">0</span>
    </div>
    <div class="out-row">
        <span class="o-lbl">Nem</span>
        <div class="ind" id="hum-indicator">KAPALI</div>
    </div>
</div>

<div class="card row-between">
    <div class="sns"><span class="s-lbl">)rawliteral" SENSOR_NAME R"rawliteral(</span><span class="s-v" id="sensor1">--</span></div>
    <div class="sns"><span class="s-lbl">)rawliteral" SENSOR_BUS_NAME R"rawliteral(</span><span class="s-v" id="sensor2">--</span></div>
    <div class="sns"><span class="s-lbl">Çalışma Süresi</span><span class="s-v" id="uptime">--</span></div>
    <div class="sns"><span class="s-lbl">Başlangıç Tarihi</span><span class="s-v" id="start-date">--</span></div>
    <div class="sns"><span class="s-lbl">Çıkım Tarihi</span><span class="s-v" id="hatch-date">--</span></div>
    <div class="sns"><span class="s-lbl">Saat</span><span class="s-v" id="rtc-time">--:--</span></div>
    <div class="sns" id="net-info"></div>
</div>

<div class="card" id="alarm-card">
    <div id="alarm-status" class="alarm-txt">Alarm yok</div>
    <div class="btn-grid" style="margin-top:8px">
        <button class="btn red" onclick="resetSafety()">Guvenlik Sifirla</button>
        <button class="btn orange" id="btn-alarm-ack" onclick="ackAlarm()" style="display:none">&#128263; Alarmi Sustur</button>
    </div>
</div>

<!-- ===== SENSOR GRAFIKLERI (Sicaklik / Nem / CO2) ===== -->
<div class="card chart-card">
    <div class="chart-hdr">
        <h2>&#128202; Sensor Grafikleri</h2>
        <div class="chart-controls">
            <span class="chart-info" id="chart-info">Son 5 dk (5 sn aralik)</span>
        </div>
    </div>
    <div class="chart-tabs">
        <button class="chart-tab active" data-chart="temp" onclick="switchChartTab('temp')">
            <span class="chart-tab-color" style="background:#ef4444"></span>
            <span>Sicaklik</span>
            <span class="chart-tab-val" id="chart-val-temp">--</span>
        </button>
        <button class="chart-tab" data-chart="hum" onclick="switchChartTab('hum')">
            <span class="chart-tab-color" style="background:#3b82f6"></span>
            <span>Nem</span>
            <span class="chart-tab-val" id="chart-val-hum">--</span>
        </button>
        <button class="chart-tab" data-chart="co2" onclick="switchChartTab('co2')">
            <span class="chart-tab-color" style="background:#10b981"></span>
            <span>CO2</span>
            <span class="chart-tab-val" id="chart-val-co2">--</span>
        </button>
    </div>
    <div class="chart-wrap">
        <canvas id="sensor-chart" width="600" height="220"></canvas>
        <div class="chart-empty" id="chart-empty">Veri yukleniyor...</div>
    </div>
    <div class="chart-stats" id="chart-stats">
        <div class="chart-stat"><span>Min</span><b id="chart-min">--</b></div>
        <div class="chart-stat"><span>Max</span><b id="chart-max">--</b></div>
        <div class="chart-stat"><span>Ort</span><b id="chart-avg">--</b></div>
        <div class="chart-stat"><span>Son</span><b id="chart-last">--</b></div>
    </div>
</div>

</div>

<!-- ===== KONTROL ===== -->
<div class="tab" id="tab-ctrl">

<div class="card">
    <h2>Sistem Kontrol</h2>
    <div class="btn-grid">
        <button class="btn green big" onclick="sendControl('start')">&#9654; Baslat</button>
        <button class="btn yellow big" onclick="sendControl('pause')">&#10074;&#10074; Duraklat</button>
        <button class="btn blue big" onclick="sendControl('resume')">&#9654; Devam</button>
        <button class="btn red big" onclick="sendControl('stop')">&#9632; Durdur</button>
    </div>
</div>

<div class="card">
    <h2>Profil Sec</h2>
    <select id="profile-select" class="sel"><option>Yukleniyor...</option></select>
    <button class="btn primary full" onclick="setProfile()">Profil Uygula</button>
    <div class="btn-grid" style="margin-top:8px">
        <button class="btn blue" onclick="cloneAndEditSelectedProfile()" title="Hazir profili klonla ve duzenle">&#9998; Profili Duzenle</button>
        <button class="btn red" id="btn-reset-override" onclick="resetSelectedProfileOverride()" style="display:none" title="Fabrika ayarlarina don">&#8634; Fabrika Ayarlari</button>
    </div>
    <p id="override-hint" style="color:#64748b;font-size:.75rem;margin-top:6px;display:none">&#9998; isareti olan profiller kullanici tarafindan duzenlenmistir.</p>
</div>

<div class="card">
    <h2>PID</h2>
    <div class="inp-row">
        <div class="inp-g"><label>Kp</label><input type="number" id="pid-kp" step="0.1" value="20.0"></div>
        <div class="inp-g"><label>Ki</label><input type="number" id="pid-ki" step="0.01" value="0.8"></div>
        <div class="inp-g"><label>Kd</label><input type="number" id="pid-kd" step="0.1" value="5.0"></div>
    </div>
    <button class="btn primary full" onclick="setPID()">PID Guncelle</button>
</div>

<div class="card">
    <h2>Nem Esikleri <small style="color:#64748b;font-weight:normal">(Profil bazli otomatik)</small></h2>
    <div class="inp-row">
        <div class="inp-g"><label>Alt %</label><input type="number" id="hum-low" step="1" value="50" readonly style="background:#e2e8f0;color:#334155;font-weight:bold"></div>
        <div class="inp-g"><label>Ust %</label><input type="number" id="hum-high" step="1" value="55" readonly style="background:#e2e8f0;color:#334155;font-weight:bold"></div>
    </div>
    <p style="color:#64748b;font-size:.75rem;margin:8px 0 0">Nem esikleri secilen profile ve evreye gore otomatik ayarlanir</p>
</div>

</div>

<!-- ===== PROFILLER ===== -->
<div class="tab" id="tab-prof">

<!-- ================ DOL KONTROLU (CANDLING) — Profil sekmesinde, en ustte ================ -->
<div class="card" id="candling-card">
    <h2>&#128270; Dol Kontrolu (Candling)</h2>
    <p style="color:#64748b;font-size:.8rem;margin-bottom:10px">
        Yumurta gelişimini kontrol etmek için ışıkla muayene zamanları.
        Kuluçka başlatılınca otomatik tetiklenir: %25, %50, %75 ve lockdown günlerinde alarm + bildirim.
    </p>
    <div id="candling-status" style="border:1px solid #334155;border-radius:8px;padding:10px 12px;margin-bottom:8px">
        <div style="display:flex;justify-content:space-between;align-items:center">
            <div>
                <div class="toggle-label" id="candling-today-label">Profil secilmemis</div>
                <div class="toggle-sub" id="candling-detail">Kuluçka başlatınca takvim oluşturulur</div>
            </div>
            <div id="candling-dot" style="width:16px;height:16px;border-radius:50%;background:#475569"></div>
        </div>
    </div>
    <div id="candling-schedule" style="font-size:.85rem;color:#94a3b8">
        <!-- JS ile doldurulacak -->
    </div>
</div>

<div class="card">
    <h2>Ozel Profil Olustur</h2>
    <div class="inp-row">
        <div class="inp-g"><label>Ad</label><input type="text" id="cp-name" placeholder="Serce"></div>
        <div class="inp-g"><label>EN</label><input type="text" id="cp-nameEN" placeholder="Sparrow"></div>
    </div>
    <div class="inp-row">
        <div class="inp-g"><label>Gun</label><input type="number" id="cp-days" min="1" max="60" value="21"></div>
        <div class="inp-g"><label>Evre</label><input type="number" id="cp-phases" min="1" max="4" value="3" onchange="updatePhaseForm()"></div>
    </div>
    <div id="phase-forms">
        <div class="ph" id="phase-0"><b>Evre 1</b><div class="ph-g">
            <input type="number" id="cp-p0-start" placeholder="Bas" min="1">
            <input type="number" id="cp-p0-end" placeholder="Bit" min="1">
            <input type="number" id="cp-p0-temp" placeholder="C" step="0.1">
            <input type="number" id="cp-p0-humLow" placeholder="N%" step="1">
            <input type="number" id="cp-p0-humHigh" placeholder="N%" step="1">
            <select id="cp-p0-turning"><option value="1">Cev+</option><option value="0">Cev-</option></select>
            <input type="text" id="cp-p0-name" placeholder="Ad" value="Gelisim-1">
        </div>
        <details class="ph-adv"><summary>Profesyonel ayarlar (cevirme araligi, sogutma, sprey)</summary>
        <div class="ph-g2">
            <div><label>Cev. araligi (dk)</label><input type="number" id="cp-p0-turnIntMin" min="0" max="600" value="60"></div>
            <div><label>Cev. suresi (sn)</label><input type="number" id="cp-p0-turnDurSec" min="0" max="120" value="15"></div>
            <div><label>Cev. acisi (deg)</label><input type="number" id="cp-p0-turnAngleDeg" min="0" max="180" value="90"></div>
            <div><label>Sogutma</label><select id="cp-p0-cool"><option value="0">Kapali</option><option value="1">Acik</option></select></div>
            <div><label>Sog. suresi (dk)</label><input type="number" id="cp-p0-coolMin" min="0" max="60" value="0"></div>
            <div><label>Sog. gunde</label><select id="cp-p0-coolPerDay"><option value="0">-</option><option value="1">1x</option><option value="2">2x</option></select></div>
            <div><label>Sprey</label><select id="cp-p0-spray"><option value="0">Kapali</option><option value="1">Acik</option></select></div>
            <div><label>Sprey suresi (sn)</label><input type="number" id="cp-p0-spraySec" min="0" max="120" value="0"></div>
        </div></details></div>
        <div class="ph" id="phase-1"><b>Evre 2</b><div class="ph-g">
            <input type="number" id="cp-p1-start" placeholder="Bas" min="1">
            <input type="number" id="cp-p1-end" placeholder="Bit" min="1">
            <input type="number" id="cp-p1-temp" placeholder="C" step="0.1">
            <input type="number" id="cp-p1-humLow" placeholder="N%" step="1">
            <input type="number" id="cp-p1-humHigh" placeholder="N%" step="1">
            <select id="cp-p1-turning"><option value="1">Cev+</option><option value="0">Cev-</option></select>
            <input type="text" id="cp-p1-name" placeholder="Ad" value="Gelisim-2">
        </div>
        <details class="ph-adv"><summary>Profesyonel ayarlar (cevirme araligi, sogutma, sprey)</summary>
        <div class="ph-g2">
            <div><label>Cev. araligi (dk)</label><input type="number" id="cp-p1-turnIntMin" min="0" max="600" value="60"></div>
            <div><label>Cev. suresi (sn)</label><input type="number" id="cp-p1-turnDurSec" min="0" max="120" value="15"></div>
            <div><label>Cev. acisi (deg)</label><input type="number" id="cp-p1-turnAngleDeg" min="0" max="180" value="90"></div>
            <div><label>Sogutma</label><select id="cp-p1-cool"><option value="0">Kapali</option><option value="1">Acik</option></select></div>
            <div><label>Sog. suresi (dk)</label><input type="number" id="cp-p1-coolMin" min="0" max="60" value="0"></div>
            <div><label>Sog. gunde</label><select id="cp-p1-coolPerDay"><option value="0">-</option><option value="1">1x</option><option value="2">2x</option></select></div>
            <div><label>Sprey</label><select id="cp-p1-spray"><option value="0">Kapali</option><option value="1">Acik</option></select></div>
            <div><label>Sprey suresi (sn)</label><input type="number" id="cp-p1-spraySec" min="0" max="120" value="0"></div>
        </div></details></div>
        <div class="ph" id="phase-2"><b>Evre 3</b><div class="ph-g">
            <input type="number" id="cp-p2-start" placeholder="Bas" min="1">
            <input type="number" id="cp-p2-end" placeholder="Bit" min="1">
            <input type="number" id="cp-p2-temp" placeholder="C" step="0.1">
            <input type="number" id="cp-p2-humLow" placeholder="N%" step="1">
            <input type="number" id="cp-p2-humHigh" placeholder="N%" step="1">
            <select id="cp-p2-turning"><option value="1">Cev+</option><option value="0" selected>Cev-</option></select>
            <input type="text" id="cp-p2-name" placeholder="Ad" value="Cikim">
        </div>
        <details class="ph-adv"><summary>Profesyonel ayarlar (cevirme araligi, sogutma, sprey)</summary>
        <div class="ph-g2">
            <div><label>Cev. araligi (dk)</label><input type="number" id="cp-p2-turnIntMin" min="0" max="600" value="0"></div>
            <div><label>Cev. suresi (sn)</label><input type="number" id="cp-p2-turnDurSec" min="0" max="120" value="0"></div>
            <div><label>Cev. acisi (deg)</label><input type="number" id="cp-p2-turnAngleDeg" min="0" max="180" value="0"></div>
            <div><label>Sogutma</label><select id="cp-p2-cool"><option value="0">Kapali</option><option value="1">Acik</option></select></div>
            <div><label>Sog. suresi (dk)</label><input type="number" id="cp-p2-coolMin" min="0" max="60" value="0"></div>
            <div><label>Sog. gunde</label><select id="cp-p2-coolPerDay"><option value="0">-</option><option value="1">1x</option><option value="2">2x</option></select></div>
            <div><label>Sprey</label><select id="cp-p2-spray"><option value="0">Kapali</option><option value="1">Acik</option></select></div>
            <div><label>Sprey suresi (sn)</label><input type="number" id="cp-p2-spraySec" min="0" max="120" value="0"></div>
        </div></details></div>
        <div class="ph hide" id="phase-3"><b>Evre 4</b><div class="ph-g">
            <input type="number" id="cp-p3-start" placeholder="Bas" min="1">
            <input type="number" id="cp-p3-end" placeholder="Bit" min="1">
            <input type="number" id="cp-p3-temp" placeholder="C" step="0.1">
            <input type="number" id="cp-p3-humLow" placeholder="N%" step="1">
            <input type="number" id="cp-p3-humHigh" placeholder="N%" step="1">
            <select id="cp-p3-turning"><option value="1">Cev+</option><option value="0">Cev-</option></select>
            <input type="text" id="cp-p3-name" placeholder="Ad" value="Evre-4">
        </div>
        <details class="ph-adv"><summary>Profesyonel ayarlar (cevirme araligi, sogutma, sprey)</summary>
        <div class="ph-g2">
            <div><label>Cev. araligi (dk)</label><input type="number" id="cp-p3-turnIntMin" min="0" max="600" value="0"></div>
            <div><label>Cev. suresi (sn)</label><input type="number" id="cp-p3-turnDurSec" min="0" max="120" value="0"></div>
            <div><label>Cev. acisi (deg)</label><input type="number" id="cp-p3-turnAngleDeg" min="0" max="180" value="0"></div>
            <div><label>Sogutma</label><select id="cp-p3-cool"><option value="0">Kapali</option><option value="1">Acik</option></select></div>
            <div><label>Sog. suresi (dk)</label><input type="number" id="cp-p3-coolMin" min="0" max="60" value="0"></div>
            <div><label>Sog. gunde</label><select id="cp-p3-coolPerDay"><option value="0">-</option><option value="1">1x</option><option value="2">2x</option></select></div>
            <div><label>Sprey</label><select id="cp-p3-spray"><option value="0">Kapali</option><option value="1">Acik</option></select></div>
            <div><label>Sprey suresi (sn)</label><input type="number" id="cp-p3-spraySec" min="0" max="120" value="0"></div>
        </div></details></div>
    </div>
    <button class="btn green full" onclick="saveCustomProfile()">Profili Kaydet</button>
</div>

<div class="card">
    <h2>Kayitli Profiller</h2>
    <div id="custom-profiles-list">Yukleniyor...</div>
    <button class="btn primary full" onclick="loadCustomProfiles()">Yenile</button>
</div>

</div>

<!-- ===== AYARLAR ===== -->
<div class="tab" id="tab-set">

<!-- ================ TARIH / SAAT AYARI ================ -->
<div class="card" id="time-card">
    <h2>&#128197; Tarih / Saat Ayari</h2>
    <p style="color:#64748b;font-size:.78rem;margin-bottom:10px">
        Cihazin saati yanlissa veya RTC pili bittiyse buradan elle ayarlayabilirsiniz.
        Ayarladiginiz zaman kalici hafizaya da kaydedilir → elektrik kesintilerinde korunur.
    </p>
    <div id="time-current" style="background:#1e293b;border-radius:8px;padding:10px;margin-bottom:10px;text-align:center">
        <div style="font-size:.7rem;color:#94a3b8;margin-bottom:2px">Cihaz Saati</div>
        <div id="time-current-val" style="font-family:ui-monospace,monospace;font-size:1.1rem;color:#22c55e;font-weight:700">--:-- --.--.----</div>
    </div>
    <div class="inp-row">
        <div class="inp-g">
            <label>Tarih</label>
            <input type="date" id="time-date">
        </div>
        <div class="inp-g">
            <label>Saat</label>
            <input type="time" id="time-time">
        </div>
    </div>
    <div class="btn-grid" style="margin-top:8px">
        <button class="btn blue" onclick="useBrowserTime()">&#128241; Tarayicidan Al</button>
        <button class="btn green" onclick="setSystemTime()">&#10003; Cihaza Yaz</button>
    </div>
</div>

<div class="card">
    <h2>Ayarlar</h2>
    <div class="btn-grid">
        <button class="btn green big" onclick="saveSettings()">Kaydet</button>
        <button class="btn blue big" onclick="loadSettings()">Yukle</button>
    </div>
</div>

<!-- ================ FIRMWARE GUNCELLEME ================ -->
<div class="card" id="firmware-update-card">
    <h2>&#128194; Firmware Guncelleme</h2>
    <p style="color:#64748b;font-size:.8rem;margin-bottom:10px">
        Cihaza yeni surum yukleyin. Sadece <code>.bin</code> dosyalari kabul edilir.
        Yukleme sirasinda cihaz kullanilamaz, ~30 saniye surer.
    </p>
    <div style="background:#0f172a;border-radius:8px;padding:10px;margin-bottom:10px">
        <div style="font-size:.72rem;color:#94a3b8;margin-bottom:2px">Mevcut Surum</div>
        <div id="fw-current-version" style="font-family:ui-monospace,monospace;font-size:1.05rem;color:#22c55e;font-weight:700">v?.?.?</div>
    </div>
    <div class="btn-grid">
        <button class="btn primary big" onclick="openFirmwareUpdate()">
            &#128190; Firmware Yukle
        </button>
    </div>
    <p style="color:#94a3b8;font-size:.7rem;margin-top:8px;line-height:1.4">
        💡 <b>Nasil:</b> "Firmware Yukle" → yeni sekmede acilir → <b>.bin</b> dosyasi sec → Update bas → bekle → cihaz yeniden baslar.
    </p>
</div>

<div class="card">
    <h2>Kulucka Bilgisi</h2>
    <div class="inp-row">
        <div class="inp-g"><label>Yumurta Sayisi</label><input type="number" id="egg-count" placeholder="100" min="0" step="1" value="100"></div>
    </div>
</div>

<!-- ================ TEMIZLIK / BAKIM MODU ================ -->
<div class="card" id="cleaning-card">
    <h2>&#129529; Temizlik / Bakim Modu</h2>
    <p style="color:#64748b;font-size:.8rem;margin-bottom:10px">
        Otomatik kontrol (PID, alarm, cevirme) gecici kapatilir. Heater/fan/nemlendirici manuel.
        30 dakika sonra otomatik biter.
    </p>

    <!-- Durum satiri -->
    <div class="toggle-row" id="clean-status-row" style="border:1px solid #334155;border-radius:8px;padding:8px 12px;margin-bottom:10px">
        <div>
            <div class="toggle-label" id="clean-status-label">Pasif</div>
            <div class="toggle-sub" id="clean-status-sub">Baslatmak icin asagidaki butona basin</div>
        </div>
        <div id="clean-status-dot" style="width:14px;height:14px;border-radius:50%;background:#475569"></div>
    </div>

    <!-- Manuel kontrol sliderlari (sadece aktifken gorunur) -->
    <div id="clean-controls" style="display:none">
        <div class="inp-g">
            <label>Isitici PWM: <span id="clean-heater-val">0</span></label>
            <input type="range" id="clean-heater" min="0" max="255" value="0" oninput="onCleanSliderChange()">
        </div>
        <div class="inp-g" style="margin-top:8px">
            <label>Fan PWM: <span id="clean-fan-val">200</span></label>
            <input type="range" id="clean-fan" min="0" max="255" value="200" oninput="onCleanSliderChange()">
        </div>
        <div class="btn-grid" style="grid-template-columns:1fr 1fr;gap:6px;margin-top:10px">
            <button class="btn" id="clean-hum-btn" onclick="toggleCleanOutput('hum')">Nemlendirici</button>
            <button class="btn" id="clean-turn-btn" onclick="toggleCleanOutput('turner')">Cevirme Motor</button>
        </div>
    </div>

    <!-- Baslat / Durdur butonu -->
    <button class="btn big full" id="clean-toggle-btn" onclick="toggleCleaningMode()"
            style="margin-top:8px;background:#059669;color:#fff">Temizlik Modunu Baslat</button>
</div>

<!-- Candling karti Profil sekmesine tasindi (daha gorunur yer) -->

<!-- ===== FAZ GECIS GECMISI (Phase Log) ===== -->
<div class="card" id="phase-log-card">
    <h2>&#128221; Faz Gecis Gecmisi</h2>
    <p style="color:#64748b;font-size:.8rem;margin-bottom:10px">
        Hangi faza ne zaman girildi, ne kadar surdu — kuluckanizin tam takvim takibi.
    </p>
    <div id="phase-log-list" style="font-size:.85rem;color:#cbd5e1">
        <div style="color:#64748b;text-align:center;padding:8px">Yukleniyor...</div>
    </div>
</div>

<div class="card">
    <h2>&#129370; Yumurta Termometre</h2>
    <p style="color:#64748b;font-size:.8rem;margin-bottom:10px">GY-906 IR sensor ESP32 cihazinin IP adresi (ornek: 192.168.1.50)</p>

    <!-- Aktif/Pasif Toggle -->
    <div class="toggle-row">
        <div>
            <div class="toggle-label">Servis Durumu</div>
            <div class="toggle-sub" id="egg-en-sub">Aktif iken her 10 sn'de bir okuma yapilir</div>
        </div>
        <label class="switch">
            <input type="checkbox" id="egg-enabled" onchange="toggleEggSensor()">
            <span class="slider"></span>
        </label>
    </div>

    <div class="inp-row">
        <div class="inp-g"><label>IP Adresi</label><input type="text" id="egg-sensor-ip" placeholder="192.168.1.50" maxlength="39"></div>
    </div>
    <div class="btn-grid" style="grid-template-columns:1fr 1fr;gap:6px">
        <button class="btn" onclick="discoverEggSensor()" id="btn-egg-discover">&#128269; Otomatik Bul</button>
        <button class="btn primary" onclick="saveEggSensorIP()">&#128204; IP Kaydet</button>
    </div>
    <p id="egg-ip-status" style="color:#64748b;font-size:.75rem;margin-top:6px">mDNS ile otomatik bulma: ayni WiFi aginda olmali</p>
</div>

<div class="card">
    <h2>WiFi</h2>
    <div class="inp-row">
        <div class="inp-g"><label>SSID</label><input type="text" id="wifi-ssid" placeholder="WiFi adi"></div>
        <div class="inp-g"><label>Sifre</label><input type="password" id="wifi-pass" placeholder="Sifre"></div>
    </div>
    <button class="btn primary full" onclick="saveWiFi()">WiFi Kaydet</button>
</div>

<div class="card">
    <h2>Log</h2>
    <div class="btn-grid">
        <button class="btn primary big" onclick="downloadLog()">CSV Indir</button>
        <button class="btn red big" onclick="clearLog()">Temizle</button>
    </div>
</div>

<div class="card">
    <h2>Firmware Guncelleme</h2>
    <p style="color:#64748b;font-size:.8rem;margin-bottom:10px">Iki yontem destekleniyor:</p>
    <a href="/ota" class="btn primary full" style="display:block;text-align:center;text-decoration:none;margin-bottom:8px">&#127760; Internet Uzerinden Guncelle (Otomatik)</a>
    <p style="color:#94a3b8;font-size:.75rem;margin:4px 0">Cihaz GitHub'dan en son surumu indirir ve SHA256 ile dogrular. Son kullanici icin onerilir.</p>
    <a href="/update" class="btn full" style="display:block;text-align:center;text-decoration:none;background:#e2e8f0;color:#334155;margin-top:10px">&#128260; Yerel .bin Dosyasi Yukle</a>
    <p style="color:#94a3b8;font-size:.75rem;margin:4px 0">Gelistirme amacli. Arduino IDE'den derlenmis .bin dosyasini yukler.</p>
</div>

</div>

</main>

<!-- ===== ALARM MODAL (tum tablari kapatan tam ekran uyari) ===== -->
<div id="alarm-modal" class="alarm-modal" style="display:none">
    <div class="alarm-modal-card" id="alarm-modal-card">
        <div class="alarm-modal-hdr" id="alarm-modal-hdr">&#9888; ALARM</div>
        <div class="alarm-modal-msg" id="alarm-modal-msg">Alarm mesaji</div>
        <!-- Standart alarm icin (sicaklik/nem) -->
        <div class="alarm-modal-ctx" id="alarm-modal-ctx-std">
            <div class="amc-row"><span>Sicaklik</span><b id="amc-temp">-- / --</b></div>
            <div class="amc-row"><span>Nem</span><b id="amc-hum">-- / --</b></div>
        </div>
        <!-- CANDLING (dol kontrolu) icin ozel kontrol takvimi -->
        <div class="alarm-modal-ctx" id="alarm-modal-ctx-candling" style="display:none">
            <div id="amc-candling-list" style="text-align:left;font-size:.9rem"></div>
        </div>
        <!-- TUM ALARMLAR icin uc buton: SUSTUR / ERTELE / KAPAT -->
        <div class="alarm-modal-btns">
            <button class="alarm-modal-btn ack" id="alarm-modal-btn-ack" onclick="ackAlarmModal()">&#128263; SUSTUR (10 dakika)</button>
            <button class="alarm-modal-btn snooze" onclick="snoozeAlarmModal()" id="alarm-modal-btn-snooze">&#9203; ERTELE (1 saat)</button>
            <button class="alarm-modal-btn dismiss" onclick="dismissAlarmModal()" id="alarm-modal-btn-dismiss">&#10004; KAPAT</button>
        </div>
    </div>
</div>

<div id="toast" class="toast hide"></div>

<script src="/app.js"></script>
</body>
</html>
)rawliteral";

// -------------------- style.css --------------------
const char PAGE_STYLE_CSS[] PROGMEM = R"rawliteral(
*{margin:0;padding:0;box-sizing:border-box}
html{font-size:16px;-webkit-tap-highlight-color:transparent}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:#f0f2f5;color:#1e293b;min-height:100vh;padding-bottom:72px;-webkit-user-select:none;-moz-user-select:none;user-select:none;overflow-x:hidden}

/* HEADER */
header{background:#fff;padding:10px 14px;display:flex;justify-content:space-between;align-items:center;border-bottom:2px solid #e2e8f0;position:sticky;top:0;z-index:100;box-shadow:0 1px 4px rgba(0,0,0,.06)}
.hdr-left{display:flex;align-items:center;gap:10px;min-width:0}
.hdr-icon{font-size:2.2rem;line-height:1}
.hdr-title{font-size:1rem;font-weight:700;color:#1e293b;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.hdr-sub{font-size:0.72rem;color:#64748b}
.hdr-right{display:flex;align-items:center;gap:8px;flex-shrink:0}
.badge{font-size:0.68rem;font-weight:700;padding:4px 10px;border-radius:14px;text-transform:uppercase;letter-spacing:.5px;white-space:nowrap;background:#e2e8f0;color:#64748b}
.badge.st-running{background:#16a34a;color:#fff}
.badge.st-paused{background:#d97706;color:#fff}
.badge.st-stopped{background:#94a3b8;color:#fff}
.badge.st-tuning{background:#7c3aed;color:#fff}
.badge.st-done{background:#2563eb;color:#fff}
.badge.st-alarm{background:#dc2626;color:#fff;animation:blink 1s infinite}
.dot{width:10px;height:10px;border-radius:50%;background:#16a34a;flex-shrink:0}
.dot.err{background:#dc2626}
@keyframes blink{50%{opacity:.4}}

/* TAB BAR - ALTTA SABIT */
.tab-bar{position:fixed;bottom:0;left:0;right:0;z-index:100;display:flex;background:#fff;border-top:2px solid #e2e8f0;padding:0;padding-bottom:env(safe-area-inset-bottom);box-shadow:0 -1px 4px rgba(0,0,0,.06)}
.tb{flex:1;padding:8px 4px 6px;background:none;border:none;color:#94a3b8;font-size:1.2rem;text-align:center;cursor:pointer;-webkit-tap-highlight-color:transparent;transition:color .15s}
.tb small{display:block;font-size:0.62rem;margin-top:2px;font-weight:600}
.tb.active{color:#2563eb}
.tb:active{background:rgba(37,99,235,.06)}

/* TAB CONTENT */
main{max-width:480px;margin:0 auto;padding:10px 10px 80px}
.tab{display:none;flex-direction:column;gap:10px}
.tab.active{display:flex}

/* GAUGE ROW - buyuk sicaklik/nem gostergesi */
.gauge-row{display:flex;gap:10px}
.gauge{flex:1;background:#fff;border-radius:14px;padding:18px 12px;text-align:center;border:1px solid #e2e8f0;position:relative;overflow:hidden;box-shadow:0 1px 3px rgba(0,0,0,.04)}
.gauge.temp{border-left:4px solid #dc2626}
.gauge.hum{border-left:4px solid #2563eb}
.g-val{font-size:2.6rem;font-weight:800;color:#0f172a;line-height:1}
.g-lbl{font-size:0.75rem;color:#64748b;margin-top:4px;text-transform:uppercase}
.g-sub{font-size:0.68rem;color:#94a3b8;margin-top:2px}

/* INFO ROW */
.info-row{display:flex;gap:10px}
.info-box{flex:1;background:#fff;border-radius:10px;padding:12px;text-align:center;border:1px solid #e2e8f0;box-shadow:0 1px 3px rgba(0,0,0,.04)}
.i-lbl{display:block;font-size:0.65rem;color:#64748b;text-transform:uppercase}
.i-val{display:block;font-size:1.1rem;font-weight:700;color:#0f172a;margin:2px 0}
.i-sub{display:block;font-size:0.65rem;color:#64748b}

/* CARD */
.card{background:#fff;border-radius:12px;padding:14px;border:1px solid #e2e8f0;box-shadow:0 1px 3px rgba(0,0,0,.04)}
.card h2{font-size:0.82rem;color:#64748b;margin-bottom:10px;text-transform:uppercase;letter-spacing:.5px}
.row-between{display:flex;justify-content:space-around;align-items:center;flex-wrap:wrap;gap:6px}

/* CIKIS BARLARI */
.out-row{display:flex;align-items:center;gap:8px;margin-bottom:8px}
.out-row:last-child{margin-bottom:0}
.o-lbl{width:56px;font-size:0.78rem;color:#475569;flex-shrink:0}
.bar{flex:1;height:22px;background:#f1f5f9;border-radius:11px;overflow:hidden;border:1px solid #e2e8f0}
.bar-f{height:100%;border-radius:11px;transition:width .5s;width:0}
.bar-f.heat{background:linear-gradient(90deg,#f59e0b,#ef4444)}
.bar-f.fan{background:linear-gradient(90deg,#06b6d4,#3b82f6)}
.o-val{width:32px;text-align:right;font-weight:700;font-size:0.8rem;flex-shrink:0;color:#334155}
.ind{flex:1;text-align:center;padding:6px;border-radius:8px;font-size:0.78rem;font-weight:700;background:#f1f5f9;color:#94a3b8}
.ind.on{background:#16a34a;color:#fff}

/* SENSOR SATIRI */
.sns{text-align:center;padding:4px 6px}
.s-lbl{display:block;font-size:0.6rem;color:#64748b;text-transform:uppercase}
.s-v{display:block;font-size:0.78rem;font-weight:600;color:#475569}
.s-ok{color:#16a34a}
.s-err{color:#dc2626}

/* ALARM */
#alarm-card.al{border-color:#dc2626;animation:pulse 1.5s infinite}
@keyframes pulse{0%,100%{border-color:#dc2626}50%{border-color:#e2e8f0}}
.alarm-txt{padding:8px;border-radius:8px;background:#f8fafc;margin-bottom:10px;font-size:0.82rem;color:#334155}

/* BUTONLAR - min 48px dokunma alani */
.btn{padding:14px 18px;border:none;border-radius:10px;font-size:0.9rem;font-weight:700;cursor:pointer;touch-action:manipulation;min-height:48px;transition:transform .1s,box-shadow .15s;box-shadow:0 1px 3px rgba(0,0,0,.1)}
.btn:active{transform:scale(.96);box-shadow:none}
.btn.primary{background:#2563eb;color:#fff}
.btn.green{background:#16a34a;color:#fff}
.btn.yellow{background:#d97706;color:#fff}
.btn.blue{background:#0891b2;color:#fff}
.btn.red{background:#dc2626;color:#fff}
.btn.orange{background:#ea580c;color:#fff}

/* ALARM MODAL — tum sayfayi kaplayan acil uyari */
.alarm-modal{position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(127,29,29,.92);z-index:9999;display:flex;align-items:center;justify-content:center;padding:20px;animation:alarmPulse 1s infinite}
.alarm-modal.candling{background:rgba(30,64,175,.92);animation:candlingPulse 1.5s infinite}
.alarm-modal.candling .alarm-modal-hdr{color:#1d4ed8}
.alarm-modal.candling .alarm-modal-msg{background:#eff6ff;border-left-color:#1d4ed8;color:#1e3a8a}
@keyframes candlingPulse{0%,100%{background:rgba(30,64,175,.92)}50%{background:rgba(59,130,246,.95)}}
@keyframes alarmPulse{0%,100%{background:rgba(127,29,29,.92)}50%{background:rgba(220,38,38,.95)}}
.alarm-modal-card{background:#fff;border-radius:16px;padding:24px;max-width:400px;width:100%;box-shadow:0 10px 40px rgba(0,0,0,.5);text-align:center}
.alarm-modal-hdr{font-size:1.6rem;font-weight:800;color:#dc2626;margin-bottom:14px;letter-spacing:1px}
.alarm-modal-msg{font-size:1.1rem;color:#1e293b;background:#fef2f2;border-left:4px solid #dc2626;padding:14px;border-radius:6px;margin-bottom:16px;text-align:left;word-wrap:break-word}
.alarm-modal-ctx{background:#f8fafc;border-radius:8px;padding:12px;margin-bottom:18px}
.amc-row{display:flex;justify-content:space-between;padding:6px 0;border-bottom:1px dashed #e2e8f0}
.amc-row:last-child{border-bottom:none}
.amc-row span{color:#64748b;font-size:.9rem}
.amc-row b{color:#0f172a;font-size:1rem;font-weight:700}
.alarm-modal-btn{width:100%;padding:14px;background:#16a34a;color:#fff;border:none;border-radius:10px;font-size:1.05rem;font-weight:700;cursor:pointer;transition:all .15s}
.alarm-modal-btn:active{transform:scale(.97);background:#15803d}
.alarm-modal-btns{display:flex;flex-direction:column;gap:8px}
.alarm-modal-btn.ack{background:#16a34a}
.alarm-modal-btn.ack:active{background:#15803d}
.alarm-modal-btn.snooze{background:#f59e0b}
.alarm-modal-btn.snooze:active{background:#d97706}
.alarm-modal-btn.dismiss{background:#475569}
.alarm-modal-btn.dismiss:active{background:#334155}
.btn.full{width:100%;margin-top:10px}
.btn.big{flex:1;min-width:0}
.btn-grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}

/* INPUT - buyuk dokunma hedefi */
input,select{padding:12px;border-radius:10px;border:1px solid #cbd5e1;background:#f8fafc;color:#1e293b;font-size:1rem;outline:none;width:100%;min-height:48px;-webkit-appearance:none;-moz-appearance:none;appearance:none}
input[type="number"]::-webkit-inner-spin-button,input[type="number"]::-webkit-outer-spin-button{-webkit-appearance:none;margin:0}
input[type="number"]{-moz-appearance:textfield}
input:focus,select:focus{border-color:#2563eb;box-shadow:0 0 0 3px rgba(37,99,235,.12)}
.sel{margin-bottom:4px}
label{display:block;font-size:0.7rem;color:#475569;text-transform:uppercase;margin-bottom:3px}

/* INPUT ROW */
.inp-row{display:flex;gap:8px;margin-bottom:8px}
.inp-g{flex:1;min-width:0}

/* EVRE FORMU */
.ph{background:#f8fafc;border:1px solid #e2e8f0;border-radius:8px;padding:10px;margin-bottom:6px}
.ph b{display:block;font-size:0.7rem;color:#7c3aed;text-transform:uppercase;margin-bottom:6px}
.ph-g{display:grid;grid-template-columns:1fr 1fr 1fr;gap:6px}
.ph-g input,.ph-g select{padding:10px 6px;font-size:0.85rem;min-height:42px}
.ph-adv{margin-top:6px}
.ph-adv>summary{font-size:.72rem;color:#475569;cursor:pointer;padding:4px 0}
.ph-adv>summary::marker{color:#7c3aed}
.ph-g2{display:grid;grid-template-columns:1fr 1fr;gap:6px;margin-top:6px}
.ph-g2 label{font-size:.7rem;color:#64748b;display:block;margin-bottom:2px}
.ph-g2 input,.ph-g2 select{padding:8px 6px;font-size:.82rem;min-height:38px;width:100%}

/* OZEL PROFIL LISTESI */
.cp-c{background:#f8fafc;border:1px solid #e2e8f0;border-radius:10px;padding:12px;margin-bottom:8px;display:flex;justify-content:space-between;align-items:center}
.cp-i{flex:1;min-width:0}
.cp-i strong{color:#1e293b;font-size:0.88rem}
.cp-i span{display:block;font-size:0.7rem;color:#64748b;margin-top:1px}
.cp-a{display:flex;gap:6px}
.btn-s{padding:10px 14px;font-size:0.78rem;border:none;border-radius:8px;cursor:pointer;font-weight:700;min-height:40px}
.btn-s.del{background:#dc2626;color:#fff}
.btn-s.use{background:#16a34a;color:#fff}

/* TOAST */
.toast{position:fixed;bottom:80px;left:50%;transform:translateX(-50%);background:#334155;color:#fff;padding:12px 24px;border-radius:10px;font-size:0.88rem;font-weight:600;z-index:300;transition:opacity .3s;white-space:nowrap;max-width:90vw;overflow:hidden;text-overflow:ellipsis;box-shadow:0 4px 12px rgba(0,0,0,.15)}
.toast.hide{opacity:0;pointer-events:none}
.toast.show{opacity:1}
.toast.ok{background:#16a34a;color:#fff}
.toast.err{background:#dc2626;color:#fff}

.hide{display:none!important}

/* SENSOR GRAFIK / CHART */
.chart-card{padding:14px}
.chart-hdr{display:flex;justify-content:space-between;align-items:center;margin-bottom:10px;flex-wrap:wrap;gap:8px}
.chart-hdr h2{margin:0}
.chart-info{font-size:0.72rem;color:#64748b}
.chart-tabs{display:flex;gap:6px;margin-bottom:10px;border-bottom:1px solid #e2e8f0;padding-bottom:8px;overflow-x:auto;-webkit-overflow-scrolling:touch}
.chart-tab{flex:1;min-width:90px;padding:8px 10px;background:#f8fafc;border:1px solid #e2e8f0;border-radius:8px;cursor:pointer;display:flex;align-items:center;gap:6px;font-size:0.78rem;font-weight:600;color:#475569;transition:all .15s;white-space:nowrap}
.chart-tab:active{transform:scale(.97)}
.chart-tab.active{background:#1e293b;color:#fff;border-color:#1e293b}
.chart-tab-color{display:inline-block;width:10px;height:10px;border-radius:50%;flex-shrink:0}
.chart-tab-val{margin-left:auto;font-family:ui-monospace,monospace;font-weight:700;font-size:0.82rem}
.chart-wrap{position:relative;background:#0f172a;border-radius:8px;padding:8px;overflow:hidden}
#sensor-chart{width:100%;height:220px;display:block}
.chart-empty{position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);color:#64748b;font-size:0.85rem;pointer-events:none}
.chart-empty.hide{display:none}
.chart-stats{display:grid;grid-template-columns:repeat(4,1fr);gap:6px;margin-top:10px}
.chart-stat{background:#f8fafc;border:1px solid #e2e8f0;border-radius:8px;padding:8px 6px;text-align:center}
.chart-stat span{display:block;font-size:0.65rem;color:#64748b;text-transform:uppercase;margin-bottom:2px}
.chart-stat b{display:block;font-family:ui-monospace,monospace;font-size:0.95rem;color:#1e293b;font-weight:700}

/* NET INFO (kucuk) */
#net-info{font-size:.6rem;color:#64748b;text-align:center;line-height:1.3}
#net-info .ab{color:#d97706;font-weight:600}
#net-info .sb{color:#2563eb;font-weight:600}

/* ========== EKRAN TAB - GORSEL DASHBOARD ========== */
.screen-container{background:linear-gradient(135deg,#0f172a 0%,#1e293b 100%);border-radius:20px;padding:15px;min-height:400px;display:flex;flex-direction:column;gap:12px}

/* Uyari Banner */
.screen-alert{display:flex;align-items:center;gap:8px;padding:10px 14px;border-radius:12px;background:rgba(22,163,74,0.2);border:1px solid rgba(22,163,74,0.4)}
.screen-alert.warning{background:rgba(234,179,8,0.2);border-color:rgba(234,179,8,0.5)}
.screen-alert.danger{background:rgba(220,38,38,0.25);border-color:rgba(220,38,38,0.6);animation:alertPulse 1.5s infinite}
@keyframes alertPulse{0%,100%{opacity:1}50%{opacity:0.7}}
.alert-icon{font-size:1.4rem}
.screen-alert .alert-icon{color:#22c55e}
.screen-alert.warning .alert-icon{color:#eab308}
.screen-alert.danger .alert-icon{color:#ef4444}
.alert-text{color:#fff;font-size:0.85rem;font-weight:600}

/* Ana Gosterge Alani */
.screen-main{display:flex;align-items:center;justify-content:space-around;gap:15px;flex:1}

/* Sicaklik Dairesi */
.temp-circle{position:relative;width:160px;height:160px;flex-shrink:0}
.temp-ring{width:100%;height:100%;transform:rotate(-90deg)}
.ring-bg{fill:none;stroke:rgba(255,255,255,0.1);stroke-width:8}
.ring-fg{fill:none;stroke:#3b82f6;stroke-width:8;stroke-linecap:round;stroke-dasharray:339.292;stroke-dashoffset:339.292;transition:stroke-dashoffset 1s ease,stroke 0.5s}
.temp-circle.hot .ring-fg{stroke:#ef4444}
.temp-circle.warm .ring-fg{stroke:#f59e0b}
.temp-circle.ok .ring-fg{stroke:#22c55e}
.temp-circle.cold .ring-fg{stroke:#3b82f6}
.temp-inner{position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);text-align:center}
.temp-val{font-size:2.8rem;font-weight:800;color:#fff;line-height:1}
.temp-unit{font-size:1rem;color:rgba(255,255,255,0.6);margin-top:-2px}
.temp-target{font-size:1.1rem;color:#94a3b8;margin-top:6px}
.temp-label{font-size:0.55rem;color:#64748b;text-transform:uppercase;letter-spacing:1px;margin-top:2px}

/* Hayvan ve Nem Kutusu */
.screen-side{display:flex;flex-direction:column;gap:12px;align-items:center}
.animal-box{background:rgba(255,255,255,0.08);border-radius:16px;padding:15px 25px;text-align:center;border:1px solid rgba(255,255,255,0.1)}
.animal-icon{font-size:3.5rem;line-height:1}
.animal-name{color:#fff;font-size:0.85rem;font-weight:700;margin-top:6px;text-transform:uppercase;letter-spacing:1px}
.hum-box{background:rgba(59,130,246,0.15);border-radius:14px;padding:12px 20px;text-align:center;border:1px solid rgba(59,130,246,0.3);display:flex;align-items:center;gap:8px}
.hum-icon{font-size:1.8rem;color:#3b82f6}
.hum-val{font-size:2rem;font-weight:800;color:#fff}
.hum-unit{font-size:1rem;color:#3b82f6;font-weight:700}

/* Alt Bilgi Satiri */
.screen-footer{display:flex;justify-content:space-around;background:rgba(255,255,255,0.05);border-radius:12px;padding:12px}
.scr-info{text-align:center}
.scr-day{font-size:1.6rem;font-weight:800;color:#fff}
.scr-day-label{font-size:0.75rem;color:#94a3b8;font-weight:600}
.scr-phase{font-size:0.9rem;color:#a78bfa;font-weight:700;text-transform:uppercase}
.scr-remaining{font-size:1.4rem;font-weight:800;color:#22c55e}

/* Cikis Durumu Ikonlari */
.screen-outputs{display:flex;justify-content:space-around;gap:8px}
.out-icon{background:rgba(255,255,255,0.05);border-radius:12px;padding:10px 14px;text-align:center;border:1px solid rgba(255,255,255,0.08);opacity:0.4;transition:all 0.3s}
.out-icon.active{opacity:1;background:rgba(34,197,94,0.2);border-color:rgba(34,197,94,0.4)}
.out-icon.active span{color:#22c55e}
/* Yumurta IR sicaklik karti */
.egg-ir-card{display:flex;align-items:center;gap:12px;background:rgba(234,179,8,0.10);border:1px solid rgba(234,179,8,0.30);border-radius:14px;padding:10px 16px}
.egg-ir-icon{font-size:2.2rem;flex-shrink:0}
.egg-ir-body{flex:1;min-width:0}
.egg-ir-label{font-size:0.58rem;color:#94a3b8;text-transform:uppercase;letter-spacing:1px;margin-bottom:2px}
.egg-ir-val{font-size:1.7rem;font-weight:800;color:#fbbf24;line-height:1}
.egg-ir-unit{font-size:0.85rem;margin-left:2px;color:#fbbf24;font-weight:600}
.egg-ir-status{text-align:right;flex-shrink:0}
.egg-ir-badge{display:inline-block;padding:3px 9px;border-radius:20px;font-size:0.68rem;font-weight:700;white-space:nowrap}
.egg-ir-badge.ok{background:rgba(34,197,94,0.2);color:#22c55e;border:1px solid rgba(34,197,94,0.35)}
.egg-ir-badge.err{background:rgba(239,68,68,0.2);color:#ef4444;border:1px solid rgba(239,68,68,0.35)}
.egg-ir-badge.nc{background:rgba(100,116,139,0.15);color:#64748b;border:1px solid rgba(100,116,139,0.25)}
.egg-ir-sub{font-size:0.65rem;color:#64748b;margin-top:3px}
.egg-ir-card.disabled{opacity:0.55;background:rgba(100,116,139,0.08);border-color:rgba(100,116,139,0.25)}
.egg-ir-card.disabled .egg-ir-val,.egg-ir-card.disabled .egg-ir-unit{color:#64748b}
/* Toggle switch */
.toggle-row{display:flex;align-items:center;justify-content:space-between;padding:10px 12px;background:rgba(255,255,255,0.03);border-radius:10px;margin-bottom:12px;border:1px solid rgba(255,255,255,0.06)}
.toggle-label{font-size:0.85rem;font-weight:700;color:#e2e8f0}
.toggle-sub{font-size:0.7rem;color:#64748b;margin-top:2px}
.switch{position:relative;display:inline-block;width:46px;height:24px;flex-shrink:0;margin-left:10px}
.switch input{opacity:0;width:0;height:0}
.slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background:#475569;transition:.3s;border-radius:24px}
.slider:before{position:absolute;content:"";height:18px;width:18px;left:3px;bottom:3px;background:#fff;transition:.3s;border-radius:50%}
.switch input:checked+.slider{background:#22c55e}
.switch input:checked+.slider:before{transform:translateX(22px)}
.out-icon span{font-size:1.5rem;display:block;color:#64748b}
.out-icon small{font-size:0.6rem;color:#94a3b8;display:block;margin-top:3px;text-transform:uppercase}

/* RESPONSIVE */
@media(max-width:360px){
.g-val{font-size:2rem}
.ph-g{grid-template-columns:1fr 1fr}
.btn-grid{grid-template-columns:1fr}
})rawliteral";

// -------------------- app.js --------------------
const char PAGE_APP_JS[] PROGMEM = R"rawliteral(
// ESP32 Akilli Kulucka - Dokunmatik Ekran Arayuz
var SN=['Baslatiliyor','Auto-Tuning','Calisiyor','Duraklatildi','Tamamlandi','ACIL DURUM','Temizlik'];
var pLoaded=false,timer=null;
var IC={Tavuk:'\uD83D\uDC14',Bildircin:'\uD83D\uDC26',Kaz:'\uD83E\uDDA2',Ordek:'\uD83E\uDD86',Hindi:'\uD83E\uDD83',Sulun:'\uD83E\uDD9A',Guvercin:'\uD83D\uDD4A',Papagan:'\uD83E\uDD9C',DeveKusu:'\uD83E\uDD9B','Ipek Bocegi':'\uD83D\uDC1B'};

function gi(id){return document.getElementById(id)}
function pIcon(n){if(!n)return'\uD83D\uDC23';for(var k in IC)if(n.indexOf(k)!==-1)return IC[k];return'\u2B50'}
function sCls(s){var m={0:'st-stopped',1:'st-tuning',2:'st-running',3:'st-paused',4:'st-done',5:'st-alarm',6:'st-tuning'};return m[s]||'st-stopped'}

// TAB
function switchTab(t){
    var tabs=document.querySelectorAll('.tab');
    for(var i=0;i<tabs.length;i++) tabs[i].classList.remove('active');
    var btns=document.querySelectorAll('.tb');
    for(var i=0;i<btns.length;i++) btns[i].classList.remove('active');
    var el=gi('tab-'+t);
    if(el) el.classList.add('active');
    var ab=document.querySelector('.tb[onclick*="'+t+'"]');
    if(ab) ab.classList.add('active');
    if(t==='prof'){loadCustomProfiles();updatePhaseForm()}
}

// INIT
document.addEventListener('DOMContentLoaded',function(){
    loadProfiles();
    loadCustomProfiles();
    updatePhaseForm();
    fetchStatus();
    timer=setInterval(fetchStatus,2000);
    // Grafik: ilk yukleme + 5 sn aralikla guncelle
    fetchHistory();
    setInterval(fetchHistory,5000);
    // Faz gecis log'u: ilk yukleme + 30 sn'de bir guncelle (sik degismez)
    fetchPhaseLog();
    setInterval(fetchPhaseLog,30000);
    // Resize sonrasi grafik yeniden cizilsin
    window.addEventListener('resize',function(){drawChart()});
});

// STATUS
function fetchStatus(){
    fetch('/api/status').then(function(r){return r.json()}).then(updateUI).catch(connErr);
}

function updateUI(d){
    connOK();
    gi('temp').textContent=d.temp.toFixed(1);
    gi('temp-target').textContent='Hedef: '+d.targetTemp.toFixed(1)+'\u00B0C';
    gi('hum').textContent=d.hum.toFixed(1);
    gi('hum-target').textContent='%'+d.targetHumLow.toFixed(0)+' - %'+d.targetHumHigh.toFixed(0);
    gi('phase').textContent=d.phase;
    gi('day-info').textContent='Gun '+d.day+'/'+d.totalDays;
    gi('phase-remaining').textContent='Evre bitis: '+d.phaseRemaining+' gun';
    gi('remaining-days').textContent=d.remainingDays+' gun';
    var st=SN[d.state]||'?';
    gi('sys-state').textContent=st;
    gi('profile-name').textContent=d.profile;
    if(gi('egg-count-dash') && d.eggCount!==undefined)gi('egg-count-dash').textContent=d.eggCount;
    
    // Ayarlar tabindaki yumurta sayisini otomatik guncelle (kullanici yazarken ustune yazma)
    var ec=gi('egg-count');
    if(ec && d.eggCount!==undefined && document.activeElement!==ec)ec.value=d.eggCount;
    
    // ===== EKRAN TAB GUNCELLEME =====
    updateScreenTab(d);

    // Output bars
    var hp=(d.heaterPWM/255*100).toFixed(0);
    gi('heater-bar').style.width=hp+'%';
    gi('heater-pwm').textContent=d.heaterPWM;
    var fp=(d.fanPWM/255*100).toFixed(0);
    gi('fan-bar').style.width=fp+'%';
    gi('fan-pwm').textContent=d.fanPWM;

    var hi=gi('hum-indicator');
    if(d.humidifier){hi.textContent='ACIK';hi.className='ind on'}
    else{hi.textContent='KAPALI';hi.className='ind'}

    // Sensors
    var s1=gi('sensor1');
    s1.textContent=d.sensor1?'OK':'HATA';
    s1.className='s-v '+(d.sensor1?'s-ok':'s-err');
    var s2=gi('sensor2');
    s2.textContent=d.sensor2?'OK':'HATA';
    s2.className='s-v '+(d.sensor2?'s-ok':'s-err');

    // Uptime
    var sec=d.uptime,h=Math.floor(sec/3600),m=Math.floor((sec%3600)/60),s=sec%60;
    gi('uptime').textContent=h+'s '+m+'d '+s+'s';

    // RTC Saat
    if(d.rtcTime){var rt=gi('rtc-time');if(rt)rt.textContent=d.rtcTime}
    // Ayar tabindaki "Cihaz Saati" gostergesi
    updateTimeDisplay(d);

    // Baslangic tarihi
    if(d.startDate){var sd=gi('start-date');if(sd)sd.textContent=d.startDate}

    // Cikim tarihi
    if(d.hatchDate){var hd=gi('hatch-date');if(hd)hd.textContent=d.hatchDate}

    // PID ve Nem Esikleri (her zaman guncelle - profil bazli otomatik)
    gi('pid-kp').value=d.kp.toFixed(1);
    gi('pid-ki').value=d.ki.toFixed(2);
    gi('pid-kd').value=d.kd.toFixed(1);
    gi('hum-low').value=d.targetHumLow.toFixed(0);
    gi('hum-high').value=d.targetHumHigh.toFixed(0);

    // Alarm — kart (her sekmede gorunur, kucuk)
    var ac=gi('alarm-card'),as=gi('alarm-status');
    var ackBtn=gi('btn-alarm-ack');
    if(d.alarm){
        ac.className='card al';
        var statusTxt='ALARM: '+d.alarmMsg;
        if(d.alarmMuted) statusTxt='[SUSTURULDU] '+statusTxt;
        as.textContent=statusTxt;
        as.style.color='#dc2626';
        if(ackBtn) ackBtn.style.display='inline-block';
    } else {
        ac.className='card';
        as.textContent='Alarm yok';
        as.style.color='#16a34a';
        if(ackBtn) ackBtn.style.display='none';
    }

    // Alarm MODAL — tam ekran tum tablari kaplar (alarmAutoShow true ise)
    var modal=gi('alarm-modal');
    if(modal){
        if(d.alarmAutoShow){
            modal.style.display='flex';
            // alarmType: 12 = ALARM_CANDLING_DUE (AlarmService.h enum sirasi)
            var isCandlingAlarm=(d.alarmType===12);
            var hdr=gi('alarm-modal-hdr');
            var btnAck=gi('alarm-modal-btn-ack');
            var btnSnooze=gi('alarm-modal-btn-snooze');
            var btnDismiss=gi('alarm-modal-btn-dismiss');
            var ctxStd=gi('alarm-modal-ctx-std');
            var ctxCand=gi('alarm-modal-ctx-candling');

            if(isCandlingAlarm){
                // CANDLING: mavi tonlu, kontrol takvimi
                modal.classList.add('candling');
                if(hdr) hdr.innerHTML='&#128270; DOL KONTROLU ZAMANI';
                if(btnAck) btnAck.style.display='none';     // candling'de SUSTUR yok
                if(btnSnooze) {btnSnooze.style.display='block';btnSnooze.innerHTML='&#9203; ERTELE (1 saat)';}
                if(btnDismiss) {btnDismiss.style.display='block';btnDismiss.innerHTML='&#10004; KAPAT (Bugun gosterme)';}
                if(ctxStd) ctxStd.style.display='none';
                if(ctxCand) ctxCand.style.display='block';

                // Kontrol takvimini doldur
                var listEl=gi('amc-candling-list');
                if(listEl && d.candling && d.candling.lockdownDay>0){
                    var totalDays=d.totalDays;
                    var currentDay=d.day;
                    var lockdownDay=d.candling.lockdownDay;
                    var d25=Math.max(1,Math.round(totalDays*0.25));
                    var d50=Math.max(1,Math.round(totalDays*0.50));
                    var d75=Math.max(1,Math.round(totalDays*0.75));
                    var checks=[
                        {day:d25, lbl:'1.Kontrol'},
                        {day:d50, lbl:'2.Kontrol'},
                        {day:d75, lbl:'3.Kontrol'},
                        {day:lockdownDay, lbl:'Lockdown'}
                    ];
                    var seen={};
                    var hasDates=(d.startDate && d.startDate!=='-' && d.startDate.length>=10);
                    var html='';
                    for(var i=0;i<checks.length;i++){
                        if(seen[checks[i].day]) continue;
                        seen[checks[i].day]=true;
                        var isCur=(currentDay===checks[i].day);
                        var isPast=(currentDay>checks[i].day);
                        var daysRemaining=checks[i].day-currentDay;
                        var color=isCur?'#1d4ed8':isPast?'#94a3b8':'#1e3a8a';
                        var weight=isCur?'700':'500';
                        var bg=isCur?'#fbbf24':'transparent';
                        var dateStr='';
                        if(hasDates){
                            var t=addDaysToDate(d.startDate, checks[i].day-1);
                            if(t) dateStr=t;
                        }
                        if(!dateStr) dateStr='Gun '+checks[i].day;
                        var dayInfo='';
                        if(isCur) dayInfo=' BUGUN!';
                        else if(isPast) dayInfo=' ('+(-daysRemaining)+' gun once)';
                        else dayInfo=' ('+daysRemaining+' gun kaldi)';
                        html+='<div style="padding:6px 8px;margin-bottom:4px;background:'+bg+';border-radius:4px;color:'+color+';font-weight:'+weight+'">';
                        html+=checks[i].lbl+': '+dateStr+dayInfo+'</div>';
                    }
                    listEl.innerHTML=html;
                }
            } else {
                // STANDART: kirmizi tonlu, sicaklik/nem, 3 buton (SUSTUR/ERTELE/KAPAT)
                modal.classList.remove('candling');
                if(hdr) hdr.innerHTML='&#9888; ALARM';
                if(btnAck) {btnAck.style.display='block';btnAck.innerHTML='&#128263; SUSTUR (10 dakika)';}
                if(btnSnooze) {btnSnooze.style.display='block';btnSnooze.innerHTML='&#9203; ERTELE (1 saat)';}
                if(btnDismiss) {btnDismiss.style.display='block';btnDismiss.innerHTML='&#10004; KAPAT (Temizle)';}
                if(ctxStd) ctxStd.style.display='block';
                if(ctxCand) ctxCand.style.display='none';

                var mt=gi('amc-temp'); if(mt) mt.textContent=d.temp.toFixed(1)+' / '+d.targetTemp.toFixed(1)+' C';
                var mh=gi('amc-hum'); if(mh) mh.textContent='%'+d.hum.toFixed(0)+' / %'+d.targetHumLow.toFixed(0)+'-%'+d.targetHumHigh.toFixed(0);
            }

            var mm=gi('alarm-modal-msg'); if(mm) mm.textContent=d.alarmMsg||'Alarm';
        } else {
            modal.style.display='none';
        }
    }

    // Header banner
    gi('pb-icon').textContent=pIcon(d.profile);
    gi('pb-name').textContent=d.profile;
    gi('pb-detail').textContent='Gun '+d.day+'/'+d.totalDays+' | '+d.phase+' | '+d.remainingDays+'g kaldi';
    var pb=gi('pb-state');
    pb.textContent=st;
    pb.className='badge '+sCls(d.state);

    // Net info
    var ni=gi('net-info');
    if(ni){
        var h='';
        if(d.apActive){h+='<span class="ab">AP</span> '+d.apIP;if(d.apClients!==undefined)h+=' ['+d.apClients+']'}
        if(d.staConnected){if(h)h+='<br>';h+='<span class="sb">W</span> '+d.staIP}
        ni.innerHTML=h;
    }

    // ===== TEMIZLIK MODU GUNCELLEMESI =====
    if(d.cleaning){
        updateCleaningUI(d.cleaning);
    }

    // ===== CANDLING (DOL KONTROLU) GUNCELLEMESI =====
    if(d.candling){
        updateCandlingUI(d.candling, d.day, d.totalDays, d.startDate);
    }
}

// ===== TEMIZLIK MODU UI =====
var _cleanSliderTimer=null;     // slider debounce
var _cleanLocalHum=false;       // local state (server'a gonderilecek)
var _cleanLocalTurn=false;

// ===== CANDLING (DOL KONTROLU) UI =====
// Baslangi tarihinden gun ekleyerek kontrol tarihini hesaplar
// startDate format: "DD/MM/YYYY"
function addDaysToDate(startDate, daysToAdd){
    if(!startDate || startDate.length<10) return null;
    var parts=startDate.split('/');
    if(parts.length!==3) return null;
    var d=parseInt(parts[0],10);
    var m=parseInt(parts[1],10);
    var y=parseInt(parts[2],10);
    if(!d || !m || !y || y<2020 || y>2099) return null;
    var dt=new Date(y, m-1, d);
    dt.setDate(dt.getDate()+daysToAdd);
    var dd=String(dt.getDate()).padStart(2,'0');
    var mm=String(dt.getMonth()+1).padStart(2,'0');
    var yy=dt.getFullYear();
    return dd+'.'+mm+'.'+yy;
}

function updateCandlingUI(c, currentDay, totalDays, startDate){
    var lbl=gi('candling-today-label');
    var det=gi('candling-detail');
    var dot=gi('candling-dot');
    var sched=gi('candling-schedule');
    if(!lbl) return;

    var isToday=!!c.isToday;
    var label=c.label||'';
    var lockdownDay=c.lockdownDay||0;

    if(isToday){
        lbl.textContent='\u{1F514} BUGUN DOL KONTROLU!';
        lbl.style.color='#f59e0b';
        det.textContent=label;
        dot.style.background='#f59e0b';
        dot.style.boxShadow='0 0 12px #f59e0b';
        // Yanip sonen efekt
        dot.style.animation='blink 1s infinite';
    } else if(lockdownDay>0){
        lbl.textContent='Bugun kontrol gunu degil';
        lbl.style.color='';
        // Baslangi tarihi varsa goster
        if(startDate && startDate!=='-'){
            det.textContent='Baslangi: '+startDate+' | Lockdown: Gun '+lockdownDay;
        } else {
            det.textContent='Lockdown: Gun '+lockdownDay+' | Toplam: '+totalDays+' gun';
        }
        dot.style.background='#475569';
        dot.style.boxShadow='none';
        dot.style.animation='';
    } else {
        lbl.textContent='Profil secilmemis';
        lbl.style.color='';
        det.textContent='Profil secildiginde kontrol programi hesaplanir';
        dot.style.background='#475569';
        dot.style.boxShadow='none';
        dot.style.animation='';
    }

    // Kontrol gunleri listesi (TARIH + KALAN GUN)
    if(sched && lockdownDay>0 && totalDays>0){
        var d25=Math.max(1,Math.round(totalDays*0.25));
        var d50=Math.max(1,Math.round(totalDays*0.50));
        var d75=Math.max(1,Math.round(totalDays*0.75));
        var checks=[
            {day:d25, lbl:'1.Kontrol'},
            {day:d50, lbl:'2.Kontrol'},
            {day:d75, lbl:'3.Kontrol'},
            {day:lockdownDay, lbl:'Lockdown'}
        ];
        // Tekrar eden gunleri filtrele
        var seen={};
        var hasDates=(startDate && startDate!=='-' && startDate.length>=10);
        var html='<div style="margin-top:8px;border-top:1px solid #334155;padding-top:8px">';
        html+='<div style="font-weight:600;color:#cbd5e1;margin-bottom:6px">Kontrol Takvimi</div>';
        html+='<div style="display:grid;grid-template-columns:auto 1fr auto;gap:6px 10px;align-items:center">';
        for(var i=0;i<checks.length;i++){
            if(seen[checks[i].day]) continue;
            seen[checks[i].day]=true;
            var isCur=(currentDay===checks[i].day);
            var isPast=(currentDay>checks[i].day);
            var daysRemaining=checks[i].day-currentDay;

            // Renk
            var color=isCur?'#f59e0b':isPast?'#475569':'#cbd5e1';
            var weight=isCur?'700':'400';
            var deco=isPast?'line-through':'none';

            // Sıra ve etiket (sol)
            html+='<span style="color:'+color+';font-weight:'+weight+';text-decoration:'+deco+'">'+checks[i].lbl+'</span>';

            // Tarih (orta) — baslangi tarihi varsa hesapla
            var dateStr='';
            if(hasDates){
                var tarih=addDaysToDate(startDate, checks[i].day-1);
                if(tarih) dateStr=tarih;
            }
            if(!dateStr) dateStr='Gun '+checks[i].day;
            html+='<span style="color:'+color+';font-weight:'+weight+';text-decoration:'+deco+';font-family:monospace">'+dateStr+'</span>';

            // Kalan gun (sag)
            var dayInfo='';
            if(isCur){
                dayInfo='<span style="color:#f59e0b;font-weight:700">BUGUN!</span>';
            } else if(isPast){
                dayInfo='<span style="color:#475569">'+(-daysRemaining)+' gun once</span>';
            } else {
                dayInfo='<span style="color:#22c55e">'+daysRemaining+' gun kaldi</span>';
            }
            html+=dayInfo;
        }
        html+='</div></div>';
        sched.innerHTML=html;
    } else {
        sched.innerHTML='';
    }
}

function updateCleaningUI(c){
    var active=!!c.active;
    var lbl=gi('clean-status-label');
    var sub=gi('clean-status-sub');
    var dot=gi('clean-status-dot');
    var ctrl=gi('clean-controls');
    var btn=gi('clean-toggle-btn');
    if(!btn) return;

    if(active){
        var minRemain=Math.ceil((c.remainMs||0)/60000);
        lbl.textContent='AKTIF';
        sub.textContent='Otomatik bitis: '+minRemain+' dk sonra';
        dot.style.background='#f59e0b';
        dot.style.boxShadow='0 0 8px #f59e0b';
        ctrl.style.display='block';
        btn.textContent='Temizlik Modunu Durdur';
        btn.style.background='#dc2626';

        // Slider degerleri (kullanici elle degistirmiyorsa)
        var hs=gi('clean-heater');
        if(hs && document.activeElement!==hs){hs.value=c.heater||0;gi('clean-heater-val').textContent=c.heater||0}
        var fs=gi('clean-fan');
        if(fs && document.activeElement!==fs){fs.value=c.fan||0;gi('clean-fan-val').textContent=c.fan||0}

        // Buton durumlari
        _cleanLocalHum=!!c.hum;
        _cleanLocalTurn=!!c.turner;
        var hb=gi('clean-hum-btn');
        if(hb){hb.textContent='Nemlendirici: '+(_cleanLocalHum?'ACIK':'KAPALI');
               hb.style.background=_cleanLocalHum?'#0ea5e9':'#334155';hb.style.color='#fff';}
        var tb=gi('clean-turn-btn');
        if(tb){tb.textContent='Cevirme: '+(_cleanLocalTurn?'ACIK':'KAPALI');
               tb.style.background=_cleanLocalTurn?'#0ea5e9':'#334155';tb.style.color='#fff';}
    } else {
        lbl.textContent='Pasif';
        sub.textContent='Baslatmak icin asagidaki butona basin';
        dot.style.background='#475569';dot.style.boxShadow='none';
        ctrl.style.display='none';
        btn.textContent='Temizlik Modunu Baslat';
        btn.style.background='#059669';
    }
}

function toggleCleaningMode(){
    // Mevcut duruma gore start/stop
    var btn=gi('clean-toggle-btn');
    if(!btn) return;
    var isActive=(btn.textContent.indexOf('Durdur')>=0);
    var action=isActive?'stop':'start';
    if(!isActive && !confirm('Temizlik modu baslatilsin mi?\nOtomatik kontrol (PID/alarm/cevirme) duracak.')) return;
    fetch('/api/cleaning',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'action='+action})
        .then(function(r){return r.json()})
        .then(function(d){showToast(d.msg,d.ok?'ok':'err');fetchStatus();})
        .catch(function(){showToast('Baglanti hatasi','err')});
}

function onCleanSliderChange(){
    var h=gi('clean-heater').value, f=gi('clean-fan').value;
    gi('clean-heater-val').textContent=h;
    gi('clean-fan-val').textContent=f;
    if(_cleanSliderTimer) clearTimeout(_cleanSliderTimer);
    _cleanSliderTimer=setTimeout(function(){ sendCleanSet(); }, 250);
}

function toggleCleanOutput(which){
    if(which==='hum')    _cleanLocalHum = !_cleanLocalHum;
    if(which==='turner') _cleanLocalTurn = !_cleanLocalTurn;
    sendCleanSet();
}

function sendCleanSet(){
    var h=gi('clean-heater').value, f=gi('clean-fan').value;
    var body='action=set&heater='+h+'&fan='+f+
             '&hum='+(_cleanLocalHum?'1':'0')+
             '&turner='+(_cleanLocalTurn?'1':'0');
    fetch('/api/cleaning',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body})
        .then(function(r){return r.json()})
        .catch(function(){showToast('Baglanti hatasi','err')});
}

// CONNECTION
function connOK(){var e=gi('conn-dot');if(e)e.className='dot'}
function connErr(){var e=gi('conn-dot');if(e)e.className='dot err'}

// PROFILES
var PROF_OVERRIDE={};  // profileIdx -> {hasOverride, overrideIdx}
function loadProfiles(){
    fetch('/api/profiles').then(function(r){return r.json()}).then(function(data){
        var sel=gi('profile-select');sel.innerHTML='';
        PROF_OVERRIDE={};
        var anyOverride=false;
        data.sort(function(a,b){
            return (a.name||'').localeCompare((b.name||''),'tr',{sensitivity:'base'});
        });
        data.forEach(function(p){
            PROF_OVERRIDE[p.index]={hasOverride:!!p.hasOverride, overrideIdx:p.overrideIdx};
            if(p.hasOverride) anyOverride=true;
            var o=document.createElement('option');
            o.value=p.index;
            // Override varsa profil adina kalem isareti
            var label=p.name+(p.hasOverride?' ✏':'')+' ('+p.nameEN+') - '+p.days+'g';
            o.textContent=label;
            sel.appendChild(o);
        });
        pLoaded=true;
        // Override durumuna gore "Fabrika Ayarlari" dugmesini ve ipucu metnini guncelle
        var hint=gi('override-hint');
        if(hint) hint.style.display=anyOverride?'block':'none';
        updateOverrideButtons();
        sel.onchange=updateOverrideButtons;
    }).catch(function(){showToast('Profiller yuklenemedi','err')});
}

function updateOverrideButtons(){
    var sel=gi('profile-select');
    var btn=gi('btn-reset-override');
    if(!sel || !btn) return;
    var v=sel.value;
    var info=PROF_OVERRIDE[v];
    btn.style.display=(info && info.hasOverride)?'inline-block':'none';
}

function setProfile(){
    var v=gi('profile-select').value;
    if(v==='')return;
    postAPI('/api/profile','index='+v);
}

// Hazir profili klonla, custom slot'a yaz, sonra Profiller tabini ac ve
// editore yukle (kullanici sicaklik/nem degistirip kaydedebilir).
function cloneAndEditSelectedProfile(){
    var sel=gi('profile-select');
    var v=sel?sel.value:'';
    if(v==='') {showToast('Once bir profil sec','err'); return;}
    var info=PROF_OVERRIDE[v];
    if(info && info.hasOverride){
        // Zaten override var: sadece editore yukle
        loadOverrideToEditor(parseInt(v), info.overrideIdx);
        return;
    }
    fetch('/api/profile-override/clone',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'idx='+v})
    .then(function(r){return r.json()})
    .then(function(d){
        if(!d.ok){showToast(d.msg,'err');return}
        showToast(d.msg,'ok');
        loadProfiles();  // override isaretini guncelle
        loadOverrideToEditor(d.profileIdx, d.customIdx);
    })
    .catch(function(){showToast('Klonlama hatasi','err')});
}

// Override CustomProfile'i Profiller sekmesindeki editor formuna yukle
function loadOverrideToEditor(profIdx, custIdx){
    fetch('/api/custom-profiles').then(function(r){return r.json()}).then(function(data){
        if(custIdx<0 || custIdx>=data.length){showToast('Custom profil bulunamadi','err');return}
        var cp=data[custIdx];
        gi('cp-name').value=cp.name||'';
        gi('cp-nameEN').value=cp.nameEN||'';
        gi('cp-days').value=cp.totalDays||21;
        gi('cp-phases').value=cp.phaseCount||2;
        updatePhaseForm();
        for(var p=0;p<(cp.phases||[]).length && p<4;p++){
            var x='cp-p'+p+'-', ph=cp.phases[p];
            if(gi(x+'start')) gi(x+'start').value=ph.startDay;
            if(gi(x+'end')) gi(x+'end').value=ph.endDay;
            if(gi(x+'temp')) gi(x+'temp').value=ph.temp;
            if(gi(x+'humLow')) gi(x+'humLow').value=ph.humLow;
            if(gi(x+'humHigh')) gi(x+'humHigh').value=ph.humHigh;
            if(gi(x+'turning')) gi(x+'turning').value=ph.turning?1:0;
            if(gi(x+'turnIntMin')) gi(x+'turnIntMin').value=ph.turnIntMin||0;
            if(gi(x+'turnDurSec')) gi(x+'turnDurSec').value=ph.turnDurSec||0;
            if(gi(x+'turnAngleDeg')) gi(x+'turnAngleDeg').value=ph.turnAngleDeg||0;
            if(gi(x+'cool')) gi(x+'cool').value=ph.cool?1:0;
            if(gi(x+'coolMin')) gi(x+'coolMin').value=ph.coolMin||0;
            if(gi(x+'coolPerDay')) gi(x+'coolPerDay').value=ph.coolPerDay||0;
            if(gi(x+'spray')) gi(x+'spray').value=ph.spray?1:0;
            if(gi(x+'spraySec')) gi(x+'spraySec').value=ph.spraySec||0;
            if(gi(x+'name')) gi(x+'name').value=ph.name||('Evre-'+(p+1));
        }
        switchTab('prof');
        showToast('Editor profil ile dolduruldu, duzenle ve Kaydet','ok');
    }).catch(function(){showToast('Custom profil okunamadi','err')});
}

// Override'i sil → fabrika ayarlarina don
function resetSelectedProfileOverride(){
    var sel=gi('profile-select');
    var v=sel?sel.value:'';
    if(v==='') return;
    if(!confirm('Bu profili fabrika ayarlarina dondur?\n(Kullanici duzenlemeleri silinir)'))return;
    fetch('/api/profile-override?idx='+v,{method:'DELETE'})
    .then(function(r){return r.json()})
    .then(function(d){
        if(d.ok){showToast(d.msg,'ok');loadProfiles();loadCustomProfiles();}
        else showToast(d.msg,'err');
    })
    .catch(function(){showToast('Sifirlama hatasi','err')});
}

// CONTROL
function sendControl(a){postAPI('/api/control','action='+a)}

// PID
function setPID(){
    postAPI('/api/pid','kp='+gi('pid-kp').value+'&ki='+gi('pid-ki').value+'&kd='+gi('pid-kd').value);
}

// HUMIDITY
function setHumidity(){
    postAPI('/api/humidity','low='+gi('hum-low').value+'&high='+gi('hum-high').value);
}

// SAFETY + ALARM
function resetSafety(){postAPI('/api/safety','')}
function ackAlarm(){postAPI('/api/alarm/ack','')}
// Modal'dan ack — anlik kapat, sonra status refresh
function ackAlarmModal(){
    var modal=gi('alarm-modal');
    if(modal) modal.style.display='none';   // anlik UI feedback
    fetch('/api/alarm/ack',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:''})
        .then(function(r){return r.json()})
        .then(function(d){if(d.ok){showToast(d.msg,'ok');fetchStatus();}else showToast(d.msg,'err');})
        .catch(function(){showToast('Sustur hatasi','err');fetchStatus();});
}

// ERTELE — 1 saat sonra tekrar aciliyor (candling icin 1 saat, digerleri icin de 1 saat)
function snoozeAlarmModal(){
    var modal=gi('alarm-modal');
    if(modal) modal.style.display='none';
    fetch('/api/alarm/snooze',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'ms=3600000'})
        .then(function(r){return r.json()})
        .then(function(d){showToast('Alarm ertelendi (1 saat)','ok');fetchStatus();})
        .catch(function(){showToast('Erteleme hatasi','err');fetchStatus();});
}

// KAPAT —
//   CANDLING: bugun icinde tekrar gosterilmez (24 saat sustur)
//   STANDART: alarm temizlenir; durum hala bozuksa kontrol mantigi yeniden tetikler
function dismissAlarmModal(){
    var modal=gi('alarm-modal');
    if(modal) modal.style.display='none';
    fetch('/api/alarm/dismiss',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:''})
        .then(function(r){return r.json()})
        .then(function(d){showToast('Alarm kapatildi','ok');fetchStatus();})
        .catch(function(){showToast('Kapatma hatasi','err');fetchStatus();});
}

// POST
function postAPI(url,body){
    fetch(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body})
    .then(function(r){return r.json()})
    .then(function(d){showToast(d.msg,d.ok?'ok':'err')})
    .catch(function(){showToast('Baglanti hatasi','err')});
}

// CUSTOM PROFILE
function updatePhaseForm(){
    var c=parseInt(gi('cp-phases').value)||3;
    for(var i=0;i<4;i++){var e=gi('phase-'+i);if(e)e.className=i<c?'ph':'ph hide'}
}

function saveCustomProfile(){
    var name=gi('cp-name').value.trim();
    if(!name){showToast('Ad zorunlu','err');return}
    var ne=gi('cp-nameEN').value.trim()||name;
    var td=gi('cp-days').value,pc=gi('cp-phases').value;
    var b='name='+encodeURIComponent(name)+'&nameEN='+encodeURIComponent(ne)+'&totalDays='+td+'&phaseCount='+pc;
    for(var p=0;p<parseInt(pc);p++){
        var x='cp-p'+p+'-';
        b+='&p'+p+'_start='+(gi(x+'start').value||1);
        b+='&p'+p+'_end='+(gi(x+'end').value||1);
        b+='&p'+p+'_temp='+(gi(x+'temp').value||37.5);
        b+='&p'+p+'_humLow='+(gi(x+'humLow').value||55);
        b+='&p'+p+'_humHigh='+(gi(x+'humHigh').value||65);
        b+='&p'+p+'_turning='+gi(x+'turning').value;
        // Profesyonel ayarlar (opsiyonel — eksikse backend 0 varsayilanini uygular)
        var tim=gi(x+'turnIntMin'); if(tim) b+='&p'+p+'_turnIntMin='+(tim.value||0);
        var tds=gi(x+'turnDurSec'); if(tds) b+='&p'+p+'_turnDurSec='+(tds.value||0);
        var tad=gi(x+'turnAngleDeg'); if(tad) b+='&p'+p+'_turnAngleDeg='+(tad.value||0);
        var co=gi(x+'cool'); if(co) b+='&p'+p+'_cool='+co.value;
        var cm=gi(x+'coolMin'); if(cm) b+='&p'+p+'_coolMin='+(cm.value||0);
        var cpd=gi(x+'coolPerDay'); if(cpd) b+='&p'+p+'_coolPerDay='+cpd.value;
        var sp=gi(x+'spray'); if(sp) b+='&p'+p+'_spray='+sp.value;
        var ss=gi(x+'spraySec'); if(ss) b+='&p'+p+'_spraySec='+(ss.value||0);
        b+='&p'+p+'_name='+encodeURIComponent(gi(x+'name').value||('Evre-'+(p+1)));
    }
    fetch('/api/custom-profile',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b})
    .then(function(r){return r.json()})
    .then(function(d){if(d.ok){showToast(d.msg,'ok');loadCustomProfiles();loadProfiles();}else showToast(d.msg,'err')})
    .catch(function(){showToast('Kayit hatasi','err')});
}

function loadCustomProfiles(){
    fetch('/api/custom-profiles').then(function(r){return r.json()}).then(function(data){
        var c=gi('custom-profiles-list');
        if(data.length===0){c.innerHTML='<p style="color:#64748b;font-size:.82rem">Henuz profil yok</p>';return}
        var h='';
        data.forEach(function(p){
            h+='<div class="cp-c"><div class="cp-i"><strong>'+p.name+'</strong> ('+p.nameEN+')';
            h+='<span>'+p.totalDays+'g - '+p.phaseCount+' evre</span></div>';
            h+='<div class="cp-a"><button class="btn-s use" onclick="useCP('+p.index+')">Sec</button>';
            h+='<button class="btn-s del" onclick="delCP('+p.index+')">Sil</button></div></div>';
        });
        c.innerHTML=h;
    }).catch(function(){gi('custom-profiles-list').innerHTML='<p style="color:#ef4444">Hata</p>'});
}

function useCP(i){postAPI('/api/profile','index=custom_'+i)}
function delCP(i){
    if(!confirm('Profili sil?'))return;
    fetch('/api/custom-profile?index='+i,{method:'DELETE'}).then(function(r){return r.json()})
    .then(function(d){if(d.ok){showToast(d.msg,'ok');loadCustomProfiles()}else showToast(d.msg,'err')})
    .catch(function(){showToast('Silinemedi','err')});
}

// SETTINGS
function saveSettings(){postAPI('/api/save-settings','eggCount='+(gi('egg-count').value||100))}

// ===== TARIH / SAAT AYARI =====
// Tarayicidan al: input'lara su anki tarayici tarih+saatini doldurur
function useBrowserTime(){
    var now=new Date();
    var pad=function(n){return n<10?('0'+n):(''+n)};
    var d=now.getFullYear()+'-'+pad(now.getMonth()+1)+'-'+pad(now.getDate());
    var t=pad(now.getHours())+':'+pad(now.getMinutes());
    var di=gi('time-date'); if(di) di.value=d;
    var ti=gi('time-time'); if(ti) ti.value=t;
    showToast('Tarayici saati alindi (Cihaza Yaz ile kaydedin)','ok');
}

// Cihaza yaz: input degerleri /api/time'a gonderilir
// GET method kullaniliyor — POST body parse hatalarini bypass eder (URL query her zaman calisir)
// ===== FIRMWARE GUNCELLEME =====
// Yeni sekmede /update sayfasini ac — kullanici bin dosyasi yukler
function openFirmwareUpdate(){
    if(!confirm('Firmware guncelleme sayfasi yeni sekmede acilir.\n\nYukleme sirasinda cihaz ~30 saniye kullanilamaz.\nDevam edilsin mi?')){
        return;
    }
    var w=window.open('/update','_blank');
    if(!w){
        showToast('Pop-up engellendi, /update adresine elle gidin','err');
    }
}

function setSystemTime(){
    var d=gi('time-date').value;   // "2026-05-07"
    var t=gi('time-time').value;   // "14:35"
    if(!d || !t){
        showToast('Tarih ve saat seciniz','err');
        return;
    }
    // Unix timestamp en guvenli format (integer parse, %3A vs gibi escape sorunlari yok)
    var dt=new Date(d+'T'+t+':00');
    var unixSec=Math.floor(dt.getTime()/1000);

    // GET request — sadece URL query (en guvenilir, body parse sorunu yok)
    var url='/api/time?unix='+unixSec
          +'&date='+encodeURIComponent(d)
          +'&time='+encodeURIComponent(t);

    fetch(url,{method:'GET'})
      .then(function(r){return r.json().then(function(j){return {status:r.status,body:j}})})
      .then(function(res){
        if(res.body && res.body.ok){
            showToast('Cihaz saati ayarlandi','ok');
            fetchStatus();
        } else {
            var err = (res.body && res.body.err) ? res.body.err : ('HTTP '+res.status);
            showToast('Hata: '+err,'err');
            console.error('[/api/time] response:', res.body);
        }
    }).catch(function(e){
        showToast('Saat ayarlanamadi (baglanti)','err');
        console.error(e);
    });
}

// Mevcut cihaz saatini gosterge guncelle (status'tan rtcDate+rtcTime ile)
function updateTimeDisplay(d){
    var el=gi('time-current-val'); if(!el) return;
    var date=d.rtcDate||'--.--.----';
    var time=d.rtcTime||'--:--';
    el.textContent=time+'  '+date;
}

function loadSettings(){
    fetch('/api/load-settings').then(function(r){return r.json()}).then(function(d){
        gi('pid-kp').value=d.kp;gi('pid-ki').value=d.ki;gi('pid-kd').value=d.kd;
        gi('hum-low').value=d.humLow;gi('hum-high').value=d.humHigh;
        if(d.eggCount!==undefined)gi('egg-count').value=d.eggCount;else gi('egg-count').value=100;
        if(d.ssid)gi('wifi-ssid').value=d.ssid;
        if(d.yumurtaIP!==undefined){var ei=gi('egg-sensor-ip');if(ei)ei.value=d.yumurtaIP;}
        if(d.yumurtaEnabled!==undefined){var ee=gi('egg-enabled');if(ee){ee.checked=d.yumurtaEnabled;updateEggEnSub(d.yumurtaEnabled);}}
        showToast('Yuklendi','ok');
    }).catch(function(){showToast('Yuklenemedi','err')});
}

// YUMURTA IR IP
function saveEggSensorIP(){
    var ip=(gi('egg-sensor-ip').value||'').trim();
    var st=gi('egg-ip-status');
    if(!ip){showToast('IP adresi bos','err');return;}
    fetch('/api/egg-ip',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'ip='+encodeURIComponent(ip)})
    .then(function(r){return r.json()})
    .then(function(d){
        showToast(d.ok?'IP kaydedildi':'Hata',d.ok?'ok':'err');
        if(st&&d.ok)st.textContent='Aktif IP: '+ip;
    }).catch(function(){showToast('Baglanti hatasi','err')});
}
function discoverEggSensor(){
    var btn=gi('btn-egg-discover'),st=gi('egg-ip-status');
    if(btn){btn.disabled=true;btn.textContent='Araniyor...';}
    if(st){st.textContent='mDNS ile yumurta.local araniyor...';st.style.color='#fbbf24';}
    // Asenkron baslat (cihaz hemen 'started' doner, mDNS ayri task'ta calisir)
    fetch('/api/egg-discover',{method:'POST'}).catch(function(){});
    // 500ms araliklarla durumu yokla, max 8 sn bekle
    var tries=0,max=16;
    var iv=setInterval(function(){
        tries++;
        fetch('/api/egg-discover-status').then(function(r){return r.json()}).then(function(d){
            if(d.status==='ok'){
                clearInterval(iv);
                var ei=gi('egg-sensor-ip');if(ei&&d.ip)ei.value=d.ip;
                if(st){st.textContent='Bulundu ve kaydedildi: '+d.ip;st.style.color='#22c55e';}
                showToast('Bulundu: '+d.ip,'ok');
                if(btn){btn.disabled=false;btn.innerHTML='&#128269; Otomatik Bul';}
            }else if(d.status==='failed'){
                clearInterval(iv);
                if(st){st.textContent='yumurta.local bulunamadi - Yumurta cihazini kontrol edin';st.style.color='#ef4444';}
                showToast('Bulunamadi','err');
                if(btn){btn.disabled=false;btn.innerHTML='&#128269; Otomatik Bul';}
            }else if(tries>=max){
                clearInterval(iv);
                if(st){st.textContent='Zaman asimi';st.style.color='#ef4444';}
                showToast('Zaman asimi','err');
                if(btn){btn.disabled=false;btn.innerHTML='&#128269; Otomatik Bul';}
            }
        }).catch(function(){});
    },500);
}
function toggleEggSensor(){
    var cb=gi('egg-enabled');if(!cb)return;
    var en=cb.checked;
    updateEggEnSub(en);
    fetch('/api/egg-ip',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'enabled='+(en?'1':'0')})
    .then(function(r){return r.json()})
    .then(function(d){showToast(en?'Servis aktif':'Servis pasif',d.ok?'ok':'err');})
    .catch(function(){showToast('Baglanti hatasi','err');cb.checked=!en;});
}
function updateEggEnSub(en){
    var s=gi('egg-en-sub');if(!s)return;
    s.textContent=en?'Aktif iken her 10 sn\'de bir okuma yapilir':'Servis devre disi - okuma yapilmiyor';
    s.style.color=en?'#22c55e':'#ef4444';
}

// WIFI
function saveWiFi(){
    var s=gi('wifi-ssid').value.trim(),p=gi('wifi-pass').value;
    if(!s){showToast('SSID zorunlu','err');return}
    postAPI('/api/wifi','ssid='+encodeURIComponent(s)+'&pass='+encodeURIComponent(p));
}

// LOG
function downloadLog(){window.open('/api/log','_blank')}
function clearLog(){
    if(!confirm('Log sil?'))return;
    fetch('/api/log',{method:'DELETE'}).then(function(r){return r.json()})
    .then(function(d){showToast(d.msg,d.ok?'ok':'err')})
    .catch(function(){showToast('Hata','err')});
}

// TOAST
function showToast(msg,type){
    var e=gi('toast');e.textContent=msg;
    e.className='toast show '+(type||'');
    setTimeout(function(){e.className='toast hide'},3000);
}

// ===== EKRAN TAB GUNCELLEME =====
function updateScreenTab(d){
    // Sicaklik degeri
    var scrTemp=gi('scr-temp');
    if(scrTemp)scrTemp.textContent=d.temp.toFixed(1);
    
    // Hedef sicaklik
    var scrTarget=gi('scr-temp-target');
    if(scrTarget)scrTarget.textContent=d.targetTemp.toFixed(1)+'\u00B0';
    
    // Sicaklik halkasi animasyonu
    var tempCircle=gi('temp-circle');
    var ringFg=gi('temp-ring-fg');
    if(tempCircle && ringFg){
        // Sicaklik durumuna gore renk
        var diff=Math.abs(d.temp-d.targetTemp);
        tempCircle.className='temp-circle';
        if(d.temp>d.targetTemp+1){tempCircle.classList.add('hot')}
        else if(d.temp>d.targetTemp+0.3){tempCircle.classList.add('warm')}
        else if(d.temp<d.targetTemp-1){tempCircle.classList.add('cold')}
        else{tempCircle.classList.add('ok')}
        
        // Halka doluluk (30-42C arasi normalize)
        var pct=Math.min(100,Math.max(0,((d.temp-30)/12)*100));
        var circumference=339.292;
        var offset=circumference-(pct/100)*circumference;
        ringFg.style.strokeDashoffset=offset;
    }
    
    // Nem degeri
    var scrHum=gi('scr-hum');
    if(scrHum)scrHum.textContent=d.hum.toFixed(0);
    
    // Hayvan ikonu ve adi
    var scrAnimal=gi('scr-animal');
    var scrAnimalName=gi('scr-animal-name');
    if(scrAnimal)scrAnimal.textContent=pIcon(d.profile);
    if(scrAnimalName)scrAnimalName.textContent=d.profile||'---';
    
    // Gun bilgisi
    var scrDay=gi('scr-day');
    if(scrDay)scrDay.textContent=d.day;
    
    // Evre
    var scrPhase=gi('scr-phase');
    if(scrPhase)scrPhase.textContent=d.phase;
    
    // Kalan gun
    var scrRemaining=gi('scr-remaining');
    if(scrRemaining)scrRemaining.textContent=d.remainingDays;
    
    // Yumurta sayisi
    var scrEgg=gi('scr-egg');
    if(scrEgg && d.eggCount!==undefined)scrEgg.textContent=d.eggCount;
    
    // Uyari banner
    var scrAlert=gi('screen-alert');
    var scrAlertText=gi('screen-alert-text');
    if(scrAlert && scrAlertText){
        scrAlert.className='screen-alert';
        if(d.state===5){
            // ACIL DURUM - Kirmizi
            scrAlert.classList.add('danger');
            scrAlertText.textContent='ACIL DURUM: '+d.alarmMsg;
        }else if(d.alarm){
            // Alarm var - Sari
            scrAlert.classList.add('warning');
            scrAlertText.textContent='UYARI: '+d.alarmMsg;
        }else if(d.temp>d.targetTemp+1 || d.temp<d.targetTemp-1){
            // Sicaklik sapma - Sari
            scrAlert.classList.add('warning');
            if(d.temp>d.targetTemp+1){
                scrAlertText.textContent='Sicaklik yuksek! (+'+((d.temp-d.targetTemp).toFixed(1))+'\u00B0C)';
            }else{
                scrAlertText.textContent='Sicaklik dusuk! ('+((d.temp-d.targetTemp).toFixed(1))+'\u00B0C)';
            }
        }else if(!d.sensor1 && !d.sensor2){
            // Sensor hatasi - Kirmizi
            scrAlert.classList.add('danger');
            scrAlertText.textContent='SENSOR HATASI!';
        }else if(!d.sensor1 || !d.sensor2){
            // Tek sensor hatasi - Sari
            scrAlert.classList.add('warning');
            scrAlertText.textContent='Sensor uyarisi: '+(d.sensor1?'S2':'S1')+' hata';
        }else{
            // Normal - Yesil
            scrAlertText.textContent='Sistem Normal - '+SN[d.state];
        }
    }
    
    // Cikis ikonlari
    var outHeater=gi('out-heater');
    var outFan=gi('out-fan');
    var outHum=gi('out-hum');
    var outTurner=gi('out-turner');
    if(outHeater){outHeater.className='out-icon';if(d.heaterPWM>10)outHeater.classList.add('active')}
    if(outFan){outFan.className='out-icon';if(d.fanPWM>50)outFan.classList.add('active')}
    if(outHum){outHum.className='out-icon';if(d.humidifier)outHum.classList.add('active')}
    if(outTurner){outTurner.className='out-icon';if(d.turningEnabled)outTurner.classList.add('active')}
    // Yumurta IR sicaklik karti
    var eggCard=gi('egg-ir-card'),eggVal=gi('scr-egg-temp'),eggBadge=gi('egg-ir-badge'),eggSub=gi('egg-ir-sub');
    if(eggVal){
        // eggTempSource: 0=kaynak yok, 1=yerel MLX90614 (MUX CH4), 2=uzak WiFi servisi
        var eggSrc=d.eggTempSource|0;
        // Kart yalnizca hicbir kaynak yokken soluklastirilir; yerel sensor
        // calisirken uzak servis kapali olsa bile kart aktif kalmali.
        if(eggCard){if(eggSrc===0&&d.eggSensorEnabled===false)eggCard.classList.add('disabled');else eggCard.classList.remove('disabled');}
        if(eggSrc===1){
            eggVal.textContent=d.eggTemp.toFixed(1);
            if(eggBadge){eggBadge.textContent='IR Yerel';eggBadge.className='egg-ir-badge ok';}
            if(eggSub)eggSub.textContent='MLX90614 - MUX CH4';
        }else if(eggSrc===2){
            eggVal.textContent=d.eggTemp.toFixed(1);
            if(eggBadge){eggBadge.textContent='IR Yedek';eggBadge.className='egg-ir-badge ok';}
            if(eggSub)eggSub.textContent=d.eggSensorIP||'';
        }else if(d.eggSensorEnabled===false){
            eggVal.textContent='--.-';
            if(eggBadge){eggBadge.textContent='Pasif';eggBadge.className='egg-ir-badge nc';}
            if(eggSub)eggSub.textContent='Yerel IR yok, uzak servis kapali';
        }else if(!d.eggSensorIP||d.eggSensorIP===''){
            eggVal.textContent='--.-';
            if(eggBadge){eggBadge.textContent='Ayarlanmadi';eggBadge.className='egg-ir-badge nc';}
            if(eggSub)eggSub.textContent='Ayarlar → Yumurta IP';
        }else{
            eggVal.textContent='--.-';
            if(eggBadge){eggBadge.textContent='Baglaniyor...';eggBadge.className='egg-ir-badge nc';}
            if(eggSub)eggSub.textContent=d.eggSensorIP||'';
        }
    }
}

// ============================================================
//  SENSOR GRAFIKLERI (Sicaklik / Nem / CO2)
//  Backend: GET /api/history -> {interval, count, unixTime, temp[], hum[], co2[]}
//  Veriler her 5 sn'de bir guncellenir.
//  Veritabani entegrasyonu icin: history.unixTime + (i*interval) = her okumanin timestamp'i
// ============================================================
var _chartType='temp';        // 'temp' | 'hum' | 'co2'
var _chartData=null;          // son /api/history cevabi
var _chartCfg={
    temp:{label:'Sicaklik',unit:'°C',color:'#ef4444',fill:'rgba(239,68,68,.15)',decimals:1,target:'targetTemp'},
    hum: {label:'Nem',unit:'%',color:'#3b82f6',fill:'rgba(59,130,246,.15)',decimals:1,target:'targetHumLow'},
    co2: {label:'CO2',unit:'ppm',color:'#10b981',fill:'rgba(16,185,129,.15)',decimals:0,target:null}
};

function fetchHistory(){
    fetch('/api/history').then(function(r){return r.json()}).then(function(d){
        _chartData=d;
        drawChart();
        updateChartTabValues();
    }).catch(function(){/* sessiz */});
}

// ===== FAZ GECIS LOG (Phase History) =====
function fetchPhaseLog(){
    fetch('/api/phase-log').then(function(r){return r.json()}).then(function(d){
        renderPhaseLog(d);
    }).catch(function(){/* sessiz */});
}

function formatDuration(sec){
    if(sec<60) return sec+' sn';
    if(sec<3600) return Math.floor(sec/60)+' dk';
    if(sec<86400) return (sec/3600).toFixed(1)+' sa';
    return (sec/86400).toFixed(1)+' gun';
}

function formatUnixDate(unix){
    if(!unix) return '--';
    var d=new Date(unix*1000);
    var dd=String(d.getDate()).padStart(2,'0');
    var mm=String(d.getMonth()+1).padStart(2,'0');
    var yy=d.getFullYear();
    var hh=String(d.getHours()).padStart(2,'0');
    var mn=String(d.getMinutes()).padStart(2,'0');
    return dd+'.'+mm+'.'+yy+' '+hh+':'+mn;
}

function renderPhaseLog(d){
    var el=gi('phase-log-list');
    if(!el) return;
    if(!d || !d.count){
        el.innerHTML='<div style="color:#64748b;text-align:center;padding:8px">Henuz faz gecisi yok</div>';
        return;
    }
    var html='<div style="display:grid;grid-template-columns:1fr;gap:6px">';
    for(var i=0;i<d.entries.length;i++){
        var e=d.entries[i];
        var color=e.active?'#16a34a':'#64748b';
        var bg=e.active?'rgba(22,163,74,0.1)':'rgba(100,116,139,0.05)';
        var statusBadge=e.active
            ? '<span style="background:#16a34a;color:#fff;padding:2px 6px;border-radius:4px;font-size:.7rem;font-weight:700">AKTIF</span>'
            : '<span style="background:#64748b;color:#fff;padding:2px 6px;border-radius:4px;font-size:.7rem">TAMAMLANDI</span>';

        html+='<div style="border:1px solid #334155;border-left:3px solid '+color+';border-radius:6px;padding:8px;background:'+bg+'">';
        html+='<div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:4px">';
        html+='<strong style="color:'+color+';font-size:.95rem">'+(i+1)+'. '+e.name+'</strong>';
        html+=statusBadge;
        html+='</div>';
        html+='<div style="font-size:.78rem;color:#94a3b8;display:grid;grid-template-columns:auto 1fr;gap:2px 8px">';
        html+='<span>Baslangi:</span><span style="font-family:monospace">'+formatUnixDate(e.startUnix)+' (Gun '+e.startDay+')</span>';
        if(e.active){
            html+='<span>Sure:</span><span style="color:#16a34a;font-weight:600">'+formatDuration(e.durationSec)+' (devam ediyor)</span>';
        } else {
            html+='<span>Bitis:</span><span style="font-family:monospace">'+formatUnixDate(e.endUnix)+' (Gun '+e.endDay+')</span>';
            html+='<span>Sure:</span><span>'+formatDuration(e.durationSec)+'</span>';
        }
        html+='</div>';
        html+='</div>';
    }
    html+='</div>';
    el.innerHTML=html;
}

function updateChartTabValues(){
    if(!_chartData) return;
    var arr=_chartData.temp||[]; var lt=arr.length?arr[arr.length-1]:null;
    var lh=(_chartData.hum||[]).slice(-1)[0];
    var lc=(_chartData.co2||[]).slice(-1)[0];
    var et=gi('chart-val-temp'); if(et) et.textContent=(lt!=null)?(lt.toFixed(1)+'°C'):'--';
    var eh=gi('chart-val-hum'); if(eh) eh.textContent=(lh!=null)?('%'+lh.toFixed(0)):'--';
    var ec=gi('chart-val-co2'); if(ec) ec.textContent=(lc!=null)?(lc+' ppm'):'--';
}

function switchChartTab(type){
    _chartType=type;
    var tabs=document.querySelectorAll('.chart-tab');
    for(var i=0;i<tabs.length;i++){
        if(tabs[i].getAttribute('data-chart')===type) tabs[i].classList.add('active');
        else tabs[i].classList.remove('active');
    }
    drawChart();
}

function drawChart(){
    var canvas=gi('sensor-chart');
    if(!canvas) return;
    var ctx=canvas.getContext('2d');
    var empty=gi('chart-empty');

    // Canvas boyutunu CSS'e gore ayarla (HiDPI destegi)
    var dpr=window.devicePixelRatio||1;
    var rect=canvas.getBoundingClientRect();
    var w=rect.width||600;
    var h=rect.height||220;
    canvas.width=Math.round(w*dpr);
    canvas.height=Math.round(h*dpr);
    ctx.setTransform(dpr,0,0,dpr,0,0);

    // Arka plan
    ctx.fillStyle='#0f172a';
    ctx.fillRect(0,0,w,h);

    if(!_chartData || !_chartData.count){
        if(empty) empty.classList.remove('hide');
        return;
    }
    if(empty) empty.classList.add('hide');

    var cfg=_chartCfg[_chartType];
    var data=_chartData[_chartType]||[];
    if(data.length<2){
        if(empty){empty.classList.remove('hide');empty.textContent='En az 2 veri noktasi gerekli...';}
        return;
    }

    // Cizim alani
    var padL=42, padR=10, padT=10, padB=22;
    var chartW=w-padL-padR;
    var chartH=h-padT-padB;

    // Min/Max degerleri (otomatik scale, biraz padding ile)
    var minV=Infinity, maxV=-Infinity;
    for(var i=0;i<data.length;i++){
        if(data[i]<minV) minV=data[i];
        if(data[i]>maxV) maxV=data[i];
    }
    if(minV===maxV){minV-=1;maxV+=1;}
    var range=maxV-minV;
    minV-=range*0.1;
    maxV+=range*0.1;
    range=maxV-minV;

    // Grid cizgileri (yatay - 4 esit aralik)
    ctx.strokeStyle='#1e293b';
    ctx.lineWidth=1;
    ctx.fillStyle='#64748b';
    ctx.font='10px ui-monospace,monospace';
    ctx.textAlign='right';
    ctx.textBaseline='middle';
    for(var g=0;g<=4;g++){
        var y=padT+(chartH*g/4);
        ctx.beginPath();
        ctx.moveTo(padL,y);
        ctx.lineTo(padL+chartW,y);
        ctx.stroke();
        var val=maxV-(range*g/4);
        ctx.fillText(val.toFixed(cfg.decimals),padL-4,y);
    }

    // X ekseni etiketleri (zaman)
    ctx.textAlign='center';
    ctx.textBaseline='top';
    var interval=_chartData.interval||5;
    var totalSec=data.length*interval;
    ctx.fillText('-'+Math.round(totalSec/60)+'dk',padL,padT+chartH+4);
    ctx.fillText('-'+Math.round(totalSec/120)+'dk',padL+chartW/2,padT+chartH+4);
    ctx.fillText('simdi',padL+chartW,padT+chartH+4);

    // Cizgi grafik
    var stepX=chartW/(data.length-1);
    function yPos(v){return padT+chartH-((v-minV)/range)*chartH;}

    // Dolgu (alan)
    ctx.fillStyle=cfg.fill;
    ctx.beginPath();
    ctx.moveTo(padL,padT+chartH);
    for(var i=0;i<data.length;i++){
        ctx.lineTo(padL+i*stepX,yPos(data[i]));
    }
    ctx.lineTo(padL+chartW,padT+chartH);
    ctx.closePath();
    ctx.fill();

    // Cizgi
    ctx.strokeStyle=cfg.color;
    ctx.lineWidth=2;
    ctx.lineJoin='round';
    ctx.beginPath();
    for(var i=0;i<data.length;i++){
        var x=padL+i*stepX;
        var y=yPos(data[i]);
        if(i===0) ctx.moveTo(x,y); else ctx.lineTo(x,y);
    }
    ctx.stroke();

    // Son nokta (vurgulu)
    var lx=padL+(data.length-1)*stepX;
    var ly=yPos(data[data.length-1]);
    ctx.fillStyle=cfg.color;
    ctx.beginPath();
    ctx.arc(lx,ly,4,0,Math.PI*2);
    ctx.fill();

    // Stats panel guncelle
    var sum=0,minS=Infinity,maxS=-Infinity;
    for(var i=0;i<data.length;i++){
        sum+=data[i];
        if(data[i]<minS) minS=data[i];
        if(data[i]>maxS) maxS=data[i];
    }
    var avg=sum/data.length;
    var last=data[data.length-1];
    var unit=cfg.unit;
    var dec=cfg.decimals;
    var emin=gi('chart-min'); if(emin) emin.textContent=minS.toFixed(dec)+' '+unit;
    var emax=gi('chart-max'); if(emax) emax.textContent=maxS.toFixed(dec)+' '+unit;
    var eavg=gi('chart-avg'); if(eavg) eavg.textContent=avg.toFixed(dec)+' '+unit;
    var elast=gi('chart-last'); if(elast) elast.textContent=last.toFixed(dec)+' '+unit;
}
)rawliteral";

// ============================================================
//  CAPTIVE PORTAL — WiFi Kurulum Sayfasi
//  STA henuz baglanmamissa AP'ye baglanan tum cihazlar buraya yonlendirilir
// ============================================================
const char PAGE_PORTAL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="tr">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Kulucka Makinesi - WiFi Kurulumu</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:linear-gradient(135deg,#1a1a2e,#16213e);min-height:100vh;display:flex;align-items:center;justify-content:center;padding:20px;color:#fff}
.box{background:rgba(255,255,255,.06);backdrop-filter:blur(10px);border-radius:18px;padding:30px;max-width:460px;width:100%;border:1px solid rgba(255,255,255,.1);box-shadow:0 8px 32px rgba(0,0,0,.3)}
h1{font-size:1.5em;margin-bottom:6px;text-align:center}
.sub{color:#9aa;text-align:center;margin-bottom:8px;font-size:.9em}
.dev{color:#7cf;text-align:center;margin-bottom:22px;font-size:.85em;padding:8px;background:rgba(124,200,255,.08);border-radius:8px}
.row{margin-bottom:14px}
label{display:block;color:#aab;font-size:.85em;margin-bottom:6px}
input,select{width:100%;padding:11px 12px;background:rgba(0,0,0,.25);border:1px solid rgba(255,255,255,.12);border-radius:8px;color:#fff;font-size:1em}
input:focus,select:focus{outline:none;border-color:#4CAF50}
.btn{width:100%;padding:13px;border:none;border-radius:9px;font-size:1em;cursor:pointer;font-weight:600;margin-top:6px;transition:all .2s}
.btn-p{background:linear-gradient(135deg,#4CAF50,#45a049);color:#fff}
.btn-p:hover:not(:disabled){transform:translateY(-1px);box-shadow:0 4px 14px rgba(76,175,80,.4)}
.btn-s{background:rgba(255,255,255,.1);color:#fff}
.btn-s:hover{background:rgba(255,255,255,.18)}
.btn:disabled{opacity:.5;cursor:not-allowed}
.nets{max-height:230px;overflow-y:auto;background:rgba(0,0,0,.2);border-radius:8px;margin-bottom:10px}
.net{display:flex;justify-content:space-between;align-items:center;padding:11px 14px;cursor:pointer;border-bottom:1px solid rgba(255,255,255,.05)}
.net:hover{background:rgba(76,175,80,.15)}
.net:last-child{border-bottom:none}
.net.sel{background:rgba(76,175,80,.25)}
.ssid{font-weight:500}
.meta{color:#8aa;font-size:.8em}
.lock{margin-right:6px;opacity:.7}
.msg{padding:11px;border-radius:8px;margin-top:14px;text-align:center;font-size:.9em;display:none}
.msg.ok{background:rgba(76,175,80,.2);color:#8E6}
.msg.err{background:rgba(244,67,54,.2);color:#f88}
.msg.info{background:rgba(33,150,243,.18);color:#9CF}
.scan-hint{color:#778;text-align:center;padding:14px;font-size:.85em}
.footer{margin-top:18px;text-align:center;font-size:.8em;color:#667}
.footer a{color:#9af;text-decoration:none}
</style>
</head>
<body>
<div class="box">
  <h1>WiFi Kurulumu</h1>
  <p class="sub">Cihazi ev WiFi'nize baglayin</p>
  <div class="dev" id="devLabel">Cihaz: -</div>

  <div class="row">
    <label>Bulunan Aglar</label>
    <div class="nets" id="nets">
      <div class="scan-hint">"Tara" butonuna basarak baslayin</div>
    </div>
    <button class="btn btn-s" id="btnScan" type="button">Aglari Tara</button>
  </div>

  <div class="row">
    <label>Ag Adi (SSID)</label>
    <input type="text" id="ssid" placeholder="Manuel girebilir veya yukaridan secebilirsiniz" autocomplete="off">
  </div>

  <div class="row">
    <label>Sifre</label>
    <input type="password" id="pass" placeholder="WiFi sifreniz" autocomplete="off">
  </div>

  <button class="btn btn-p" id="btnConnect" type="button">Baglan</button>
  <div class="msg" id="msg"></div>

  <div class="footer">
    Hicbir agi kurmadan da <a href="/">ana paneli</a> kullanabilirsiniz.
  </div>
</div>

<script>
var SCAN_POLL_MS=2000;
var STATUS_POLL_MS=1500;
var $=function(id){return document.getElementById(id)};
var scanTimer=null,statusTimer=null;

fetch('/api/identity').then(function(r){return r.json()}).then(function(d){
  if(d.deviceName) $('devLabel').textContent='Cihaz: '+d.deviceName+' ('+d.deviceId+')';
});

function showMsg(t,k){var m=$('msg');m.textContent=t;m.className='msg '+k;m.style.display='block'}
function hideMsg(){$('msg').style.display='none'}

function rssiBars(r){if(r>=-55)return '4/4';if(r>=-65)return '3/4';if(r>=-75)return '2/4';return '1/4'}

function renderNets(list){
  var c=$('nets');
  if(!list||!list.length){c.innerHTML='<div class="scan-hint">Ag bulunamadi</div>';return}
  // Aynı SSID'yi tekrarlama, en güçlü RSSI'yi al
  var seen={};
  list.sort(function(a,b){return b.rssi-a.rssi}).forEach(function(n){if(!seen[n.ssid])seen[n.ssid]=n});
  c.innerHTML='';
  Object.keys(seen).forEach(function(k){
    var n=seen[k];
    var d=document.createElement('div');
    d.className='net';
    d.innerHTML='<span class="ssid">'+(n.sec?'<span class="lock">🔒</span>':'')+escapeHtml(n.ssid)+'</span>'+
                '<span class="meta">'+n.rssi+' dBm · '+rssiBars(n.rssi)+'</span>';
    d.onclick=function(){
      Array.prototype.forEach.call(c.querySelectorAll('.net'),function(e){e.classList.remove('sel')});
      d.classList.add('sel');
      $('ssid').value=n.ssid;
      if(n.sec)$('pass').focus();else $('pass').value='';
    };
    c.appendChild(d);
  });
}

function escapeHtml(s){return s.replace(/[&<>"']/g,function(c){return {'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]})}

function startScan(){
  hideMsg();
  $('btnScan').disabled=true;$('btnScan').textContent='Taraniyor...';
  $('nets').innerHTML='<div class="scan-hint">WiFi aglari taraniyor (5-8 sn)...</div>';
  fetch('/api/wifi/scan?refresh=1').then(function(r){return r.json()}).then(pollScan).catch(function(){
    $('btnScan').disabled=false;$('btnScan').textContent='Aglari Tara';
    showMsg('Tarama baslatilamadi','err');
  });
}

function pollScan(){
  if(scanTimer)clearTimeout(scanTimer);
  scanTimer=setTimeout(function(){
    fetch('/api/wifi/scan').then(function(r){return r.json()}).then(function(d){
      if(d.status==='done'){
        renderNets(d.networks);
        $('btnScan').disabled=false;$('btnScan').textContent='Tekrar Tara';
      } else if(d.status==='scanning'){
        pollScan();
      } else {
        $('btnScan').disabled=false;$('btnScan').textContent='Aglari Tara';
      }
    });
  },SCAN_POLL_MS);
}

$('btnScan').onclick=startScan;

$('btnConnect').onclick=function(){
  var s=$('ssid').value.trim(),p=$('pass').value;
  if(!s){showMsg('Ag adi (SSID) gerekli','err');return}
  $('btnConnect').disabled=true;$('btnConnect').textContent='Baglaniyor...';
  hideMsg();
  var fd=new FormData();fd.append('ssid',s);fd.append('pass',p);
  fetch('/api/wifi/connect',{method:'POST',body:fd}).then(function(r){return r.json()}).then(function(d){
    if(d.ok){
      showMsg('Baglanti deneniyor — durum izleniyor...','info');
      pollStatus(0);
    } else {
      showMsg(d.msg||'Hata','err');
      $('btnConnect').disabled=false;$('btnConnect').textContent='Baglan';
    }
  }).catch(function(){
    showMsg('Baglanti hatasi','err');
    $('btnConnect').disabled=false;$('btnConnect').textContent='Baglan';
  });
};

function pollStatus(tries){
  if(tries>20){
    showMsg('Zaman asimi — sifre veya SSID hatali olabilir','err');
    $('btnConnect').disabled=false;$('btnConnect').textContent='Tekrar Dene';
    return;
  }
  if(statusTimer)clearTimeout(statusTimer);
  statusTimer=setTimeout(function(){
    fetch('/api/wifi/status').then(function(r){return r.json()}).then(function(d){
      if(d.connected){
        // AP'ye hala bagli oldugumuz icin yeni IP'ye otomatik yonlendirme calismaz
        // Kullaniciya net talimat ver
        var html="Basariyla baglanildi!<br><br>"+
                 "<b>Yeni adres:</b><br>"+
                 "http://"+d.ip+"/<br>"+
                 "http://"+(d.ssid?"ev WiFi&#39;nize baglandiktan sonra ":"")+"<a href=\"/api/identity\" target=\"_blank\">cihaz adi</a>.local<br><br>"+
                 "<b>Sonraki adim:</b> Bu cihazin \"<i>Kulucka-...</i>\" AP&#39;sinden cikip <b>\""+escapeHtml(d.ssid||"")+"\"</b> agina baglandiktan sonra yukaridaki adresleri kullanin.";
        var m=$('msg');m.innerHTML=html;m.className='msg ok';m.style.display='block';
        $('btnConnect').disabled=false;$('btnConnect').textContent='Tamam';
      } else {
        pollStatus(tries+1);
      }
    }).catch(function(){pollStatus(tries+1)});
  },STATUS_POLL_MS);
}

// Açılışta otomatik tarama başlat
setTimeout(startScan,400);
</script>
</body>
</html>)rawliteral";


// ============================================================
//  PAGE_OTA_ONLINE_HTML — Internet uzerinden firmware guncelleme arayuzu
//  /ota adresinde sunulur. /api/ota/* endpoint'lerini kullanir.
// ============================================================
const char PAGE_OTA_ONLINE_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="tr">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Firmware Guncelleme</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,sans-serif;background:#f3f4f6;color:#111827;padding:16px;max-width:720px;margin:0 auto}
h1{font-size:1.4em;margin-bottom:8px;color:#111827}
.sub{color:#6b7280;font-size:.9em;margin-bottom:20px}
.card{background:#fff;border-radius:12px;padding:18px;margin-bottom:14px;box-shadow:0 1px 3px rgba(0,0,0,.08)}
.row{display:flex;justify-content:space-between;padding:6px 0;border-bottom:1px dashed #e5e7eb;font-size:.95em}
.row:last-child{border-bottom:none}
.k{color:#6b7280}.v{color:#111827;font-weight:600}
label{display:block;margin:8px 0 6px;color:#374151;font-size:.9em;font-weight:600}
input[type=text],input[type=url]{width:100%;padding:10px 12px;border:1px solid #d1d5db;border-radius:8px;font-size:.95em}
button{width:100%;padding:12px;border:none;border-radius:8px;font-size:1em;font-weight:600;cursor:pointer;margin-top:10px}
.btn-primary{background:#2563eb;color:#fff}
.btn-primary:hover{background:#1d4ed8}
.btn-success{background:#16a34a;color:#fff}
.btn-success:hover{background:#15803d}
.btn-secondary{background:#6b7280;color:#fff}
.btn-secondary:hover{background:#4b5563}
button:disabled{background:#9ca3af;cursor:not-allowed}
.bar{width:100%;height:18px;background:#e5e7eb;border-radius:9px;overflow:hidden;margin:10px 0}
.fill{height:100%;background:linear-gradient(90deg,#3b82f6,#2563eb);transition:width .25s ease;width:0%}
.msg{padding:10px 12px;border-radius:8px;margin:10px 0;font-size:.9em;display:none}
.msg.ok{background:#d1fae5;color:#065f46;display:block}
.msg.err{background:#fee2e2;color:#991b1b;display:block}
.msg.info{background:#dbeafe;color:#1e3a8a;display:block}
.badge{display:inline-block;padding:3px 10px;border-radius:12px;font-size:.78em;font-weight:600}
.b-idle{background:#e5e7eb;color:#374151}
.b-check{background:#fef3c7;color:#92400e}
.b-avail{background:#dbeafe;color:#1e40af}
.b-no{background:#d1fae5;color:#065f46}
.b-dl{background:#fde68a;color:#92400e}
.b-ver{background:#e0e7ff;color:#3730a3}
.b-ok{background:#bbf7d0;color:#166534}
.b-err{background:#fecaca;color:#991b1b}
pre.changelog{background:#f9fafb;border:1px solid #e5e7eb;border-radius:8px;padding:10px;font-size:.85em;white-space:pre-wrap;max-height:160px;overflow-y:auto;color:#374151}
a.back{display:inline-block;margin-top:10px;color:#2563eb;text-decoration:none;font-size:.9em}
</style>
</head>
<body>

<h1>Internet Uzerinden Firmware Guncelleme</h1>
<p class="sub">Cihaz, belirlediginiz adresteki <code>version.json</code> dosyasini kontrol eder ve yeni surum varsa firmware'i indirir.</p>

<!-- Cihaz bilgisi -->
<div class="card">
  <div class="row"><span class="k">Cihaz</span><span class="v" id="dName">-</span></div>
  <div class="row"><span class="k">Cihaz ID</span><span class="v" id="dId">-</span></div>
  <div class="row"><span class="k">Mevcut Firmware</span><span class="v" id="dFw">-</span></div>
  <div class="row"><span class="k">Build Tarihi</span><span class="v" id="dBuild">-</span></div>
</div>

<!-- Guncelleme URL -->
<div class="card">
  <label for="urlInput">version.json URL</label>
  <input type="url" id="urlInput" placeholder="https://raw.githubusercontent.com/USER/REPO/main/version.json" autocomplete="off">
  <button class="btn-secondary" id="btnSaveUrl">URL'i Kaydet</button>
  <div class="msg" id="urlMsg"></div>
</div>

<!-- Durum + aksiyon -->
<div class="card">
  <div class="row">
    <span class="k">Durum</span>
    <span><span class="badge b-idle" id="stBadge">Bos</span></span>
  </div>
  <div class="row" id="rRemote" style="display:none">
    <span class="k">Sunucudaki Surum</span><span class="v" id="rVer">-</span>
  </div>
  <div class="row" id="rDate" style="display:none">
    <span class="k">Yayin Tarihi</span><span class="v" id="rBuild">-</span>
  </div>
  <div class="row" id="rSize" style="display:none">
    <span class="k">Dosya Boyutu</span><span class="v" id="rSz">-</span>
  </div>

  <div id="changelogWrap" style="display:none;margin-top:10px">
    <label>Degisiklikler</label>
    <pre class="changelog" id="cLog"></pre>
  </div>

  <div id="progWrap" style="display:none">
    <div class="bar"><div class="fill" id="pFill"></div></div>
    <div style="text-align:center;font-size:.85em;color:#6b7280" id="pTxt">0%</div>
  </div>

  <button class="btn-primary" id="btnCheck">Guncelleme Kontrol Et</button>
  <button class="btn-success" id="btnPull" style="display:none">Indir ve Kur</button>

  <div class="msg" id="aMsg"></div>
</div>

<a href="/" class="back">&larr; Ana sayfaya don</a>

<script>
var POLL_MS=2000;
var pollHandle=null;

function $(id){return document.getElementById(id)}
function show(el,on){el.style.display=on?'':'none'}
function fmtBytes(b){
  if(!b||b<=0)return '-';
  if(b<1024)return b+' B';
  if(b<1048576)return (b/1024).toFixed(1)+' KB';
  return (b/1048576).toFixed(2)+' MB';
}
function setMsg(el,txt,cls){el.textContent=txt;el.className='msg '+cls}
function clrMsg(el){el.style.display='none';el.className='msg';el.textContent=''}

// Cihaz kimligi
fetch('/api/identity').then(function(r){return r.json()}).then(function(d){
  $('dName').textContent=d.deviceName||'-';
  $('dId').textContent=d.deviceId||'-';
  $('dFw').textContent=d.fwVersion||'-';
  $('dBuild').textContent=(d.buildDate||'')+(d.buildTime?' '+d.buildTime:'');
  // Firmware Guncelleme kartindaki versiyon gostergesi
  var fwEl=gi('fw-current-version');
  if(fwEl) fwEl.textContent='v'+(d.fwVersion||'?.?.?');
});

// URL'i yukle (mevcut deger)
function loadStatus(){
  return fetch('/api/ota/status').then(function(r){return r.json()}).then(function(d){
    if(d.updateUrl && !document.activeElement.matches('input')){
      $('urlInput').value=d.updateUrl;
    }
    return d;
  });
}

function applyState(d){
  var st=d.state||0;
  var b=$('stBadge');
  var classes=['b-idle','b-check','b-avail','b-no','b-dl','b-ver','b-ok','b-err'];
  var labels=['Bos','Kontrol ediliyor...','Yeni surum mevcut','Guncel','Indiriliyor...','Dogrulaniyor...','Basarili — yeniden baslatiliyor','HATA'];
  b.className='badge '+(classes[st]||'b-idle');
  b.textContent=labels[st]||'Bilinmiyor';

  // Remote bilgi alanlari
  var hasRemote=d.remoteVersion && d.remoteVersion.length>0;
  show($('rRemote'),hasRemote);
  show($('rDate'),hasRemote && d.remoteBuildDate);
  show($('rSize'),hasRemote && d.remoteSize>0);
  if(hasRemote){
    $('rVer').textContent=d.remoteVersion;
    $('rBuild').textContent=d.remoteBuildDate||'-';
    $('rSz').textContent=fmtBytes(d.remoteSize);
  }

  // Changelog
  if(d.changelog && d.changelog.length>0){
    $('cLog').textContent=d.changelog;
    show($('changelogWrap'),true);
  } else {
    show($('changelogWrap'),false);
  }

  // Progress (yalniz indirme/dogrulama sirasinda)
  if(st==4 || st==5){
    show($('progWrap'),true);
    var p=d.progress||0;
    $('pFill').style.width=p+'%';
    $('pTxt').textContent=p+'%';
  } else {
    show($('progWrap'),false);
  }

  // Buton durumu
  if(st==1 || st==4 || st==5){
    $('btnCheck').disabled=true;
    show($('btnPull'),false);
  } else if(st==2){
    $('btnCheck').disabled=false;
    $('btnCheck').textContent='Tekrar Kontrol Et';
    show($('btnPull'),true);
    $('btnPull').disabled=false;
  } else {
    $('btnCheck').disabled=false;
    $('btnCheck').textContent='Guncelleme Kontrol Et';
    show($('btnPull'),false);
  }

  // Aksiyon mesaji
  if(st==3){setMsg($('aMsg'),'Cihaz zaten guncel.','ok');}
  else if(st==6){setMsg($('aMsg'),'Yukleme basarili — cihaz yeniden baslatiliyor...','ok');}
  else if(st==7){setMsg($('aMsg'),'Hata: '+(d.error||'bilinmiyor'),'err');}
  else if(st==4){setMsg($('aMsg'),'Firmware indiriliyor — bu sirada cihaz baska istek isleyemez.','info');}
  else if(st==5){setMsg($('aMsg'),'SHA256 dogrulamasi yapiliyor...','info');}
  else if(st!=2){clrMsg($('aMsg'));}

  // Polling: aktif islem yoksa yavaslat
  if(st==1 || st==4 || st==5){
    if(!pollHandle){pollHandle=setInterval(refresh,POLL_MS)}
  } else {
    if(pollHandle){clearInterval(pollHandle);pollHandle=null}
  }
}

function refresh(){loadStatus().then(applyState).catch(function(){})}

// URL kaydet
$('btnSaveUrl').addEventListener('click',function(){
  var url=$('urlInput').value.trim();
  if(url.length<10 || url.indexOf('http')!==0){
    setMsg($('urlMsg'),'Gecerli bir http(s) URL girin.','err');return;
  }
  clrMsg($('urlMsg'));
  $('btnSaveUrl').disabled=true;
  var fd=new FormData();fd.append('url',url);
  fetch('/api/ota/url',{method:'POST',body:fd})
    .then(function(r){return r.json()})
    .then(function(d){
      if(d.ok){setMsg($('urlMsg'),'URL kaydedildi.','ok');}
      else{setMsg($('urlMsg'),'Hata: '+(d.msg||'bilinmiyor'),'err');}
    })
    .catch(function(){setMsg($('urlMsg'),'Aglatma hatasi.','err');})
    .finally(function(){$('btnSaveUrl').disabled=false});
});

// Kontrol Et
$('btnCheck').addEventListener('click',function(){
  clrMsg($('aMsg'));
  $('btnCheck').disabled=true;
  fetch('/api/ota/check',{method:'POST'})
    .then(function(r){return r.json()})
    .then(function(d){
      if(!d.ok){setMsg($('aMsg'),'Hata: '+(d.msg||'kontrol baslatilamadi'),'err');$('btnCheck').disabled=false;return;}
      // Polling baslasin
      if(!pollHandle){pollHandle=setInterval(refresh,POLL_MS)}
      refresh();
    })
    .catch(function(){setMsg($('aMsg'),'Aglatma hatasi.','err');$('btnCheck').disabled=false;});
});

// Indir
$('btnPull').addEventListener('click',function(){
  if(!confirm('Firmware indirilecek ve cihaz yeniden baslatilacak. Devam edilsin mi?'))return;
  clrMsg($('aMsg'));
  $('btnPull').disabled=true;
  fetch('/api/ota/pull',{method:'POST'})
    .then(function(r){return r.json()})
    .then(function(d){
      if(!d.ok){setMsg($('aMsg'),'Hata: '+(d.msg||'indirme baslatilamadi'),'err');$('btnPull').disabled=false;return;}
      if(!pollHandle){pollHandle=setInterval(refresh,POLL_MS)}
      refresh();
    })
    .catch(function(){setMsg($('aMsg'),'Aglatma hatasi.','err');$('btnPull').disabled=false;});
});

// Ilk yukleme
loadStatus().then(applyState);
</script>
</body>
</html>)rawliteral";