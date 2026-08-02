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
