#include "web_server.h"

// ============================================================
//  PROGMEM Web Arayuzu - Tek sayfa (inline CSS+JS)
//  SmartFarmFeeder yaklasimi: HTML+CSS+JS tek PROGMEM string
//  Firmware ile birlikte yuklenir, SPIFFS/LittleFS gerektirmez
// ============================================================

const char PAGE_INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="tr">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0,maximum-scale=1.0,user-scalable=no">
<title>Kulucka</title>
<style>
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

/* NET INFO (kucuk) */
#net-info{font-size:.6rem;color:#64748b;text-align:center;line-height:1.3}
#net-info .ab{color:#d97706;font-weight:600}
#net-info .sb{color:#2563eb;font-weight:600}

/* RESPONSIVE */
@media(max-width:360px){
.g-val{font-size:2rem}
.ph-g{grid-template-columns:1fr 1fr}
.btn-grid{grid-template-columns:1fr}
}
</style>
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
<button class="tb active" onclick="switchTab('dash')">&#9670;<br><small>Durum</small></button>
<button class="tb" onclick="switchTab('ctrl')">&#9881;<br><small>Kontrol</small></button>
<button class="tb" onclick="switchTab('prof')">&#128051;<br><small>Profil</small></button>
<button class="tb" onclick="switchTab('set')">&#9878;<br><small>Ayar</small></button>
</nav>

<!-- ANA ICERIK -->
<main>

<!-- ===== DASHBOARD ===== -->
<div class="tab active" id="tab-dash">

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
    <div class="sns"><span class="s-lbl">SHT20</span><span class="s-v" id="sensor1">--</span></div>
    <div class="sns"><span class="s-lbl">RS485</span><span class="s-v" id="sensor2">--</span></div>
    <div class="sns"><span class="s-lbl">Baslangic Tarihi</span><span class="s-v" id="start-date">--/--/----</span></div>
    <div class="sns"><span class="s-lbl">Cikim Tarihi</span><span class="s-v" id="hatch-date">--/--/----</span></div>
    <div class="sns"><span class="s-lbl">Çalışma Süresi</span><span class="s-v" id="uptime">--</span></div>
    <div class="sns"><span class="s-lbl">Saat</span><span class="s-v" id="rtc-time">--:--</span></div>
    <div class="sns" id="net-info"></div>
</div>

<div class="card" id="alarm-card">
    <div id="alarm-status" class="alarm-txt">Alarm yok</div>
    <button class="btn red" onclick="resetSafety()">Guvenlik Sifirla</button>
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
        </div></div>
        <div class="ph" id="phase-1"><b>Evre 2</b><div class="ph-g">
            <input type="number" id="cp-p1-start" placeholder="Bas" min="1">
            <input type="number" id="cp-p1-end" placeholder="Bit" min="1">
            <input type="number" id="cp-p1-temp" placeholder="C" step="0.1">
            <input type="number" id="cp-p1-humLow" placeholder="N%" step="1">
            <input type="number" id="cp-p1-humHigh" placeholder="N%" step="1">
            <select id="cp-p1-turning"><option value="1">Cev+</option><option value="0">Cev-</option></select>
            <input type="text" id="cp-p1-name" placeholder="Ad" value="Gelisim-2">
        </div></div>
        <div class="ph" id="phase-2"><b>Evre 3</b><div class="ph-g">
            <input type="number" id="cp-p2-start" placeholder="Bas" min="1">
            <input type="number" id="cp-p2-end" placeholder="Bit" min="1">
            <input type="number" id="cp-p2-temp" placeholder="C" step="0.1">
            <input type="number" id="cp-p2-humLow" placeholder="N%" step="1">
            <input type="number" id="cp-p2-humHigh" placeholder="N%" step="1">
            <select id="cp-p2-turning"><option value="1">Cev+</option><option value="0" selected>Cev-</option></select>
            <input type="text" id="cp-p2-name" placeholder="Ad" value="Cikim">
        </div></div>
        <div class="ph hide" id="phase-3"><b>Evre 4</b><div class="ph-g">
            <input type="number" id="cp-p3-start" placeholder="Bas" min="1">
            <input type="number" id="cp-p3-end" placeholder="Bit" min="1">
            <input type="number" id="cp-p3-temp" placeholder="C" step="0.1">
            <input type="number" id="cp-p3-humLow" placeholder="N%" step="1">
            <input type="number" id="cp-p3-humHigh" placeholder="N%" step="1">
            <select id="cp-p3-turning"><option value="1">Cev+</option><option value="0">Cev-</option></select>
            <input type="text" id="cp-p3-name" placeholder="Ad" value="Evre-4">
        </div></div>
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

<div class="card">
    <h2>Ayarlar</h2>
    <div class="btn-grid">
        <button class="btn green big" onclick="saveSettings()">Kaydet</button>
        <button class="btn blue big" onclick="loadSettings()">Yukle</button>
    </div>
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

</div>

</main>

<div id="toast" class="toast hide"></div>

<script>
// ESP32 Akilli Kulucka - Dokunmatik Ekran Arayuz
var SN=['Baslatiliyor','Auto-Tuning','Calisiyor','Duraklatildi','Tamamlandi','ACIL DURUM'];
var pLoaded=false,timer=null;
var IC={Tavuk:'\uD83D\uDC14',Bildircin:'\uD83D\uDC26',Kaz:'\uD83E\uDDA2',Ordek:'\uD83E\uDD86',Hindi:'\uD83E\uDD83',Sulun:'\uD83E\uDD9A',Guvercin:'\uD83D\uDD4A',Papagan:'\uD83E\uDD9C',DeveKusu:'\uD83E\uDD9B','Ipek Bocegi':'\uD83D\uDC1B'};

function gi(id){return document.getElementById(id)}
function pIcon(n){if(!n)return'\uD83D\uDC23';for(var k in IC)if(n.indexOf(k)!==-1)return IC[k];return'\u2B50'}
function sCls(s){var m={0:'st-stopped',1:'st-tuning',2:'st-running',3:'st-paused',4:'st-done',5:'st-alarm'};return m[s]||'st-stopped'}

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

    // Alarm
    var ac=gi('alarm-card'),as=gi('alarm-status');
    if(d.alarm){ac.className='card al';as.textContent='ALARM: '+d.alarmMsg;as.style.color='#dc2626'}
    else{ac.className='card';as.textContent='Alarm yok';as.style.color='#16a34a'}

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
}

// CONNECTION
function connOK(){var e=gi('conn-dot');if(e)e.className='dot'}
function connErr(){var e=gi('conn-dot');if(e)e.className='dot err'}

// PROFILES
function loadProfiles(){
    fetch('/api/profiles').then(function(r){return r.json()}).then(function(data){
        var sel=gi('profile-select');sel.innerHTML='';
        data.sort(function(a,b){
            return (a.name||'').localeCompare((b.name||''),'tr',{sensitivity:'base'});
        });
        data.forEach(function(p){
            var o=document.createElement('option');
            o.value=p.index;
            o.textContent=p.name+' ('+p.nameEN+') - '+p.days+'g';
            sel.appendChild(o);
        });
        pLoaded=true;
    }).catch(function(){showToast('Profiller yuklenemedi','err')});
}

function setProfile(){
    var v=gi('profile-select').value;
    if(v==='')return;
    postAPI('/api/profile','index='+v);
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

// SAFETY
function resetSafety(){postAPI('/api/safety','')}

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
        b+='&p'+p+'_name='+encodeURIComponent(gi(x+'name').value||('Evre-'+(p+1)));
    }
    fetch('/api/custom-profile',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b})
    .then(function(r){return r.json()})
    .then(function(d){if(d.ok){showToast(d.msg,'ok');loadCustomProfiles()}else showToast(d.msg,'err')})
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
function saveSettings(){postAPI('/api/save-settings','')}
function loadSettings(){
    fetch('/api/load-settings').then(function(r){return r.json()}).then(function(d){
        gi('pid-kp').value=d.kp;gi('pid-ki').value=d.ki;gi('pid-kd').value=d.kd;
        gi('hum-low').value=d.humLow;gi('hum-high').value=d.humHigh;
        if(d.ssid)gi('wifi-ssid').value=d.ssid;
        showToast('Yuklendi','ok');
    }).catch(function(){showToast('Yuklenemedi','err')});
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

</script>
</body>
</html>
)rawliteral";

// Artik ayri CSS/JS yok - hepsi HTML icinde inline
const char PAGE_STYLE_CSS[] PROGMEM = "";
const char PAGE_APP_JS[] PROGMEM = "";