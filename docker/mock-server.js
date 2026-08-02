const express = require('express');
const cors = require('cors');

const app = express();
app.use(cors());
app.use(express.json());
app.use(express.urlencoded({ extended: true }));

// Simulasyon verileri
let state = {
    temperature: 37.5,
    humidity: 55.0,
    targetTemp: 37.8,
    targetHumLow: 55,
    targetHumHigh: 60,
    heaterPWM: 128,
    fanPWM: 200,
    humidifier: false,
    day: 5,
    totalDays: 21,
    phase: "Gelisim",
    profile: "Tavuk",
    state: 2, // RUNNING (index 2 in SN array)
    sensor1: true,
    sensor2: true,
    kp: 20.0,
    ki: 0.8,
    kd: 5.0,
    alarm: false,
    alarmMsg: "",
    uptime: 3600,
    apActive: true,
    apIP: "192.168.4.1",
    apClients: 1,
    staConnected: false,
    staIP: ""
};

// Profiller (ESP32 formatinda) - Kaynak: Hayvan.pdf
const profiles = [
    { index: 0, name: "Tavuk", nameEN: "Chicken", days: 21, 
      phases: [{ start: 1, end: 18, temp: 37.5, humLow: 55, humHigh: 60, turning: true, name: "Gelisim" },
               { start: 19, end: 21, temp: 37.2, humLow: 65, humHigh: 75, turning: false, name: "Cikim" }] },
    { index: 1, name: "Bildircin", nameEN: "Quail", days: 18,
      phases: [{ start: 1, end: 14, temp: 37.5, humLow: 55, humHigh: 60, turning: true, name: "Gelisim" },
               { start: 15, end: 18, temp: 37.2, humLow: 65, humHigh: 75, turning: false, name: "Cikim" }] },
    { index: 2, name: "Kaz", nameEN: "Goose", days: 30,
      phases: [{ start: 1, end: 25, temp: 37.5, humLow: 58, humHigh: 65, turning: true, name: "Gelisim" },
               { start: 26, end: 30, temp: 37.2, humLow: 78, humHigh: 85, turning: false, name: "Cikim" }] },
    { index: 3, name: "Ordek", nameEN: "Duck", days: 28,
      phases: [{ start: 1, end: 25, temp: 37.5, humLow: 58, humHigh: 65, turning: true, name: "Gelisim" },
               { start: 26, end: 28, temp: 37.2, humLow: 75, humHigh: 85, turning: false, name: "Cikim" }] },
    { index: 4, name: "Hindi", nameEN: "Turkey", days: 28,
      phases: [{ start: 1, end: 25, temp: 37.5, humLow: 55, humHigh: 62, turning: true, name: "Gelisim" },
               { start: 26, end: 28, temp: 37.2, humLow: 70, humHigh: 80, turning: false, name: "Cikim" }] },
    { index: 5, name: "Sulun", nameEN: "Pheasant", days: 25,
      phases: [{ start: 1, end: 20, temp: 37.5, humLow: 55, humHigh: 60, turning: true, name: "Gelisim" },
               { start: 21, end: 25, temp: 37.2, humLow: 65, humHigh: 75, turning: false, name: "Cikim" }] },
    { index: 6, name: "Guvercin", nameEN: "Pigeon", days: 19,
      phases: [{ start: 1, end: 15, temp: 37.5, humLow: 55, humHigh: 60, turning: true, name: "Gelisim" },
               { start: 16, end: 19, temp: 37.2, humLow: 65, humHigh: 75, turning: false, name: "Cikim" }] },
    { index: 7, name: "Papagan", nameEN: "Parrot", days: 26,
      phases: [{ start: 1, end: 23, temp: 37.0, humLow: 50, humHigh: 60, turning: true, name: "Gelisim" },
               { start: 24, end: 26, temp: 36.7, humLow: 65, humHigh: 75, turning: false, name: "Cikim" }] },
    { index: 8, name: "Devekusu", nameEN: "Ostrich", days: 42,
      phases: [{ start: 1, end: 38, temp: 36.0, humLow: 25, humHigh: 35, turning: true, name: "Gelisim" },
               { start: 39, end: 42, temp: 36.0, humLow: 40, humHigh: 50, turning: false, name: "Cikim" }] },
    { index: 9, name: "Ipek Bocegi", nameEN: "Silkworm", days: 42,
      phases: [{ start: 1, end: 10, temp: 25.0, humLow: 75, humHigh: 80, turning: false, name: "Yumurta" },
               { start: 11, end: 22, temp: 27.0, humLow: 80, humHigh: 90, turning: false, name: "Genc Larva" },
               { start: 23, end: 35, temp: 24.0, humLow: 65, humHigh: 75, turning: false, name: "Olgun Larva" },
               { start: 36, end: 42, temp: 24.5, humLow: 55, humHigh: 65, turning: false, name: "Koza Orme" }] }
];

// Ozel profiller
let customProfiles = [];

// GET /status - Sistem durumu
app.get('/status', (req, res) => {
    // Simulasyon: sicaklik ve nem hafif degissin
    state.temperature = 37.5 + (Math.random() - 0.5) * 0.3;
    state.humidity = 55 + (Math.random() - 0.5) * 2;
    state.uptime += 2;
    
    // Heater PWM simulasyonu
    if (state.temperature < state.targetTemp) {
        state.heaterPWM = Math.min(255, state.heaterPWM + 5);
    } else {
        state.heaterPWM = Math.max(0, state.heaterPWM - 5);
    }
    
    // Mevcut profili bul ve kalan gun hesapla
    const currentProfile = profiles.find(p => p.name === state.profile) || profiles[0];
    const currentPhase = getCurrentPhase(currentProfile, state.day);
    const remainingDays = Math.max(0, state.totalDays - state.day);
    const phaseRemaining = currentPhase ? Math.max(0, currentPhase.end - state.day) : 0;
    const phaseEndDay = currentPhase ? currentPhase.end : 0;

    res.json({
        temp: parseFloat(state.temperature.toFixed(1)),
        hum: parseFloat(state.humidity.toFixed(1)),
        targetTemp: state.targetTemp,
        targetHumLow: state.targetHumLow,
        targetHumHigh: state.targetHumHigh,
        heaterPWM: state.heaterPWM,
        fanPWM: state.fanPWM,
        humidifier: state.humidifier,
        day: state.day,
        totalDays: state.totalDays,
        remainingDays: remainingDays,
        phase: state.phase,
        phaseRemaining: phaseRemaining,
        phaseEndDay: phaseEndDay,
        profile: state.profile,
        state: state.state,
        sensor1: state.sensor1,
        sensor2: state.sensor2,
        kp: state.kp,
        ki: state.ki,
        kd: state.kd,
        alarm: state.alarm,
        alarmMsg: state.alarmMsg,
        uptime: state.uptime,
        apActive: state.apActive,
        apIP: state.apIP,
        apClients: state.apClients,
        staConnected: state.staConnected,
        staIP: state.staIP
    });
});

// GET /profiles - Profil listesi
app.get('/profiles', (req, res) => {
    res.json(profiles);
});

// Mevcut gun icin evre bul
function getCurrentPhase(profile, day) {
    for (const phase of profile.phases) {
        if (day >= phase.start && day <= phase.end) {
            return phase;
        }
    }
    return profile.phases[0]; // Varsayilan ilk evre
}

// POST /profile - Profil sec
app.post('/profile', (req, res) => {
    const index = parseInt(req.body.index);
    const profile = profiles.find(p => p.index === index);
    if (profile) {
        state.profile = profile.name;
        state.totalDays = profile.days;
        state.day = 1;
        
        // Evre bazli nem ve sicaklik otomatik ayarla
        const phase = getCurrentPhase(profile, state.day);
        state.targetTemp = phase.temp;
        state.targetHumLow = phase.humLow;
        state.targetHumHigh = phase.humHigh;
        state.phase = phase.name;
        
        res.json({ ok: true, msg: `Profil: ${profile.name}` });
    } else {
        res.json({ ok: false, msg: "Profil bulunamadi" });
    }
});

// POST /control - Sistem kontrol
app.post('/control', (req, res) => {
    const action = req.body.action;
    switch(action) {
        case 'start':
            state.state = 2; // RUNNING
            state.day = 1;
            res.json({ ok: true, msg: "Kulucka baslatildi" });
            break;
        case 'pause':
            state.state = 3; // PAUSED
            res.json({ ok: true, msg: "Duraklatildi" });
            break;
        case 'resume':
            state.state = 2; // RUNNING
            res.json({ ok: true, msg: "Devam ediliyor" });
            break;
        case 'stop':
            state.state = 0; // IDLE
            res.json({ ok: true, msg: "Durduruldu" });
            break;
        default:
            res.json({ ok: false, msg: "Bilinmeyen komut" });
    }
});

// POST /pid - PID parametreleri
app.post('/pid', (req, res) => {
    if (req.body.kp) state.kp = parseFloat(req.body.kp);
    if (req.body.ki) state.ki = parseFloat(req.body.ki);
    if (req.body.kd) state.kd = parseFloat(req.body.kd);
    res.json({ ok: true, msg: "PID guncellendi" });
});

// POST /humidity - Nem esikleri
app.post('/humidity', (req, res) => {
    if (req.body.low) state.targetHumLow = parseFloat(req.body.low);
    if (req.body.high) state.targetHumHigh = parseFloat(req.body.high);
    res.json({ ok: true, msg: "Nem esikleri guncellendi" });
});

// POST /safety - Guvenlik sifirla
app.post('/safety', (req, res) => {
    state.alarm = false;
    state.alarmMsg = "";
    state.state = 0;
    res.json({ ok: true, msg: "Guvenlik sifirlandi" });
});

// GET /custom-profiles - Ozel profiller
app.get('/custom-profiles', (req, res) => {
    res.json(customProfiles);
});

// POST /custom-profile - Ozel profil kaydet
app.post('/custom-profile', (req, res) => {
    const newProfile = {
        index: customProfiles.length,
        name: req.body.name,
        nameEN: req.body.nameEN,
        totalDays: parseInt(req.body.totalDays),
        phaseCount: parseInt(req.body.phaseCount)
    };
    customProfiles.push(newProfile);
    console.log('Ozel profil kaydedildi:', newProfile);
    res.json({ ok: true, msg: "Profil kaydedildi" });
});

// DELETE /custom-profile - Ozel profil sil
app.delete('/custom-profile', (req, res) => {
    const index = parseInt(req.query.index);
    if (index >= 0 && index < customProfiles.length) {
        customProfiles.splice(index, 1);
        // Indexleri yeniden duzenle
        customProfiles.forEach((p, i) => p.index = i);
        res.json({ ok: true, msg: "Profil silindi" });
    } else {
        res.json({ ok: false, msg: "Profil bulunamadi" });
    }
});

// POST /save-settings - Ayarlari kaydet
app.post('/save-settings', (req, res) => {
    res.json({ ok: true, msg: "Ayarlar kaydedildi" });
});

// GET /load-settings - Ayarlari yukle
app.get('/load-settings', (req, res) => {
    res.json({
        kp: state.kp,
        ki: state.ki,
        kd: state.kd,
        humLow: state.targetHumLow,
        humHigh: state.targetHumHigh,
        ssid: "KuluckaMakinesi"
    });
});

// POST /wifi - WiFi kaydet
app.post('/wifi', (req, res) => {
    console.log('WiFi:', req.body.ssid);
    res.json({ ok: true, msg: "WiFi kaydedildi, yeniden baslatiliyor..." });
});

// GET /log - Log indir
app.get('/log', (req, res) => {
    const csv = "timestamp,temp,hum,heater,fan,day,phase\n" +
                "2024-01-01 10:00:00,37.5,55.0,128,200,1,Gelisim\n" +
                "2024-01-01 10:05:00,37.6,54.8,125,200,1,Gelisim\n";
    res.setHeader('Content-Type', 'text/csv');
    res.setHeader('Content-Disposition', 'attachment; filename=kulucka_log.csv');
    res.send(csv);
});

// DELETE /log - Log temizle
app.delete('/log', (req, res) => {
    res.json({ ok: true, msg: "Log temizlendi" });
});

// Sunucuyu baslat
const PORT = 3000;
app.listen(PORT, '0.0.0.0', () => {
    console.log(`Mock API sunucusu calisiyor: http://localhost:${PORT}`);
    console.log('Endpointler:');
    console.log('  GET  /status');
    console.log('  GET  /profiles');
    console.log('  POST /profile');
    console.log('  POST /start');
    console.log('  POST /pause');
    console.log('  POST /resume');
    console.log('  POST /stop');
    console.log('  POST /pid');
    console.log('  POST /targets');
    console.log('  POST /alarm/clear');
    console.log('  POST /customProfile');
});
