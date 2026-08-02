#include "OTAService.h"

OTAService::OTAService()
    : _state(OTA_IDLE)
    , _progress(0)
    , _errorMsg("")
    , _totalSize(0)
    , _currentSize(0)
    , _onStart(nullptr)
    , _onEnd(nullptr)
    , _onProgress(nullptr)
{
}

void OTAService::begin(AsyncWebServer &server) {
    setupRoutes(server);
    Serial.println("[OTA] Firmware guncelleme servisi baslatildi");
    Serial.println("[OTA] Guncelleme sayfasi: /update");
}

void OTAService::update() {
    // OTA durumu kontrolu (gerekirse)
}

OTAState OTAService::getState() const {
    return _state;
}

uint8_t OTAService::getProgress() const {
    return _progress;
}

String OTAService::getErrorMessage() const {
    return _errorMsg;
}

String OTAService::getStatusJSON() const {
    String json = "{";
    json += "\"state\":";
    switch (_state) {
        case OTA_IDLE:      json += "\"idle\""; break;
        case OTA_RECEIVING: json += "\"receiving\""; break;
        case OTA_SUCCESS:   json += "\"success\""; break;
        case OTA_ERROR:     json += "\"error\""; break;
    }
    json += ",\"progress\":" + String(_progress);
    if (_errorMsg.length() > 0) {
        json += ",\"error\":\"" + _errorMsg + "\"";
    }
    json += "}";
    return json;
}

void OTAService::setOnStartCallback(void (*callback)()) {
    _onStart = callback;
}

void OTAService::setOnEndCallback(void (*callback)(bool success)) {
    _onEnd = callback;
}

void OTAService::setOnProgressCallback(void (*callback)(uint8_t progress)) {
    _onProgress = callback;
}

void OTAService::setupRoutes(AsyncWebServer &server) {
    // OTA guncelleme sayfasi
    server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
        String html = R"rawliteral(
<!DOCTYPE html>
<html lang="tr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Firmware Guncelleme - Kulucka Makinesi</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            padding: 20px;
        }
        .container {
            background: rgba(255,255,255,0.05);
            backdrop-filter: blur(10px);
            border-radius: 20px;
            padding: 40px;
            max-width: 500px;
            width: 100%;
            box-shadow: 0 8px 32px rgba(0,0,0,0.3);
            border: 1px solid rgba(255,255,255,0.1);
        }
        h1 {
            color: #fff;
            text-align: center;
            margin-bottom: 10px;
            font-size: 1.8em;
        }
        .subtitle {
            color: #888;
            text-align: center;
            margin-bottom: 30px;
            font-size: 0.9em;
        }
        .upload-area {
            border: 2px dashed rgba(255,255,255,0.3);
            border-radius: 15px;
            padding: 40px 20px;
            text-align: center;
            transition: all 0.3s ease;
            cursor: pointer;
            margin-bottom: 20px;
        }
        .upload-area:hover, .upload-area.dragover {
            border-color: #4CAF50;
            background: rgba(76,175,80,0.1);
        }
        .upload-area svg {
            width: 60px;
            height: 60px;
            fill: #888;
            margin-bottom: 15px;
        }
        .upload-area p {
            color: #aaa;
            margin-bottom: 10px;
        }
        .upload-area .hint {
            color: #666;
            font-size: 0.85em;
        }
        input[type="file"] { display: none; }
        .file-info {
            background: rgba(76,175,80,0.2);
            border-radius: 10px;
            padding: 15px;
            margin-bottom: 20px;
            display: none;
        }
        .file-info.show { display: block; }
        .file-info p {
            color: #4CAF50;
            font-size: 0.9em;
        }
        .progress-container {
            background: rgba(255,255,255,0.1);
            border-radius: 10px;
            height: 20px;
            overflow: hidden;
            margin-bottom: 15px;
            display: none;
        }
        .progress-container.show { display: block; }
        .progress-bar {
            height: 100%;
            background: linear-gradient(90deg, #4CAF50, #8BC34A);
            width: 0%;
            transition: width 0.3s ease;
            border-radius: 10px;
        }
        .progress-text {
            color: #aaa;
            text-align: center;
            font-size: 0.9em;
            margin-bottom: 20px;
            display: none;
        }
        .progress-text.show { display: block; }
        .btn {
            width: 100%;
            padding: 15px;
            border: none;
            border-radius: 10px;
            font-size: 1.1em;
            cursor: pointer;
            transition: all 0.3s ease;
            font-weight: 600;
        }
        .btn-primary {
            background: linear-gradient(135deg, #4CAF50, #45a049);
            color: white;
        }
        .btn-primary:hover:not(:disabled) {
            transform: translateY(-2px);
            box-shadow: 0 5px 20px rgba(76,175,80,0.4);
        }
        .btn-primary:disabled {
            background: #555;
            cursor: not-allowed;
        }
        .status {
            margin-top: 20px;
            padding: 15px;
            border-radius: 10px;
            text-align: center;
            display: none;
        }
        .status.show { display: block; }
        .status.success {
            background: rgba(76,175,80,0.2);
            color: #4CAF50;
        }
        .status.error {
            background: rgba(244,67,54,0.2);
            color: #f44336;
        }
        .warning {
            background: rgba(255,152,0,0.2);
            border-radius: 10px;
            padding: 15px;
            margin-bottom: 20px;
        }
        .warning p {
            color: #ff9800;
            font-size: 0.85em;
            line-height: 1.5;
        }
        .warning svg {
            width: 20px;
            height: 20px;
            fill: #ff9800;
            vertical-align: middle;
            margin-right: 8px;
        }
        .back-link {
            display: block;
            text-align: center;
            margin-top: 20px;
            color: #888;
            text-decoration: none;
            font-size: 0.9em;
        }
        .back-link:hover { color: #fff; }
        .version-info {
            text-align: center;
            margin-top: 20px;
            padding-top: 20px;
            border-top: 1px solid rgba(255,255,255,0.1);
        }
        .version-info p {
            color: #666;
            font-size: 0.8em;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🔄 Firmware Guncelleme</h1>
        <p class="subtitle" id="deviceLabel">Kulucka Makinesi OTA</p>
        
        <div class="warning">
            <p>
                <svg viewBox="0 0 24 24"><path d="M1 21h22L12 2 1 21zm12-3h-2v-2h2v2zm0-4h-2v-4h2v4z"/></svg>
                <strong>Dikkat:</strong> Guncelleme sirasinda cihazin gucunu kesmeyin. 
                Islem tamamlaninca cihaz otomatik yeniden baslatilacaktir.
            </p>
        </div>
        
        <div class="upload-area" id="uploadArea" onclick="document.getElementById('fileInput').click()">
            <svg viewBox="0 0 24 24"><path d="M9 16h6v-6h4l-7-7-7 7h4zm-4 2h14v2H5z"/></svg>
            <p>Firmware dosyasini secin veya surukleyin</p>
            <p class="hint">.bin uzantili dosya (maks 4MB)</p>
        </div>
        <input type="file" id="fileInput" accept=".bin">
        
        <div class="file-info" id="fileInfo">
            <p id="fileName"></p>
        </div>
        
        <div class="progress-container" id="progressContainer">
            <div class="progress-bar" id="progressBar"></div>
        </div>
        <p class="progress-text" id="progressText">Yukleniyor: 0%</p>
        
        <button class="btn btn-primary" id="uploadBtn" disabled>Guncellemeyi Baslat</button>
        
        <div class="status" id="status"></div>
        
        <a href="/" class="back-link">← Ana Sayfaya Don</a>
        
        <div class="version-info">
            <p>Cihaz: <span id="deviceName">-</span> (<span id="deviceId">-</span>)</p>
            <p>Mevcut Surum: <span id="fwVersion">-</span></p>
            <p>Build: <span id="buildDate">-</span></p>
            <p>Heap: <span id="heapInfo">-</span> KB</p>
        </div>
    </div>

    <script>
        const uploadArea = document.getElementById('uploadArea');
        const fileInput = document.getElementById('fileInput');
        const fileInfo = document.getElementById('fileInfo');
        const fileName = document.getElementById('fileName');
        const uploadBtn = document.getElementById('uploadBtn');
        const progressContainer = document.getElementById('progressContainer');
        const progressBar = document.getElementById('progressBar');
        const progressText = document.getElementById('progressText');
        const status = document.getElementById('status');
        
        let selectedFile = null;
        
        // Drag & Drop
        uploadArea.addEventListener('dragover', (e) => {
            e.preventDefault();
            uploadArea.classList.add('dragover');
        });
        
        uploadArea.addEventListener('dragleave', () => {
            uploadArea.classList.remove('dragover');
        });
        
        uploadArea.addEventListener('drop', (e) => {
            e.preventDefault();
            uploadArea.classList.remove('dragover');
            const files = e.dataTransfer.files;
            if (files.length > 0) handleFile(files[0]);
        });
        
        fileInput.addEventListener('change', (e) => {
            if (e.target.files.length > 0) handleFile(e.target.files[0]);
        });
        
        function handleFile(file) {
            if (!file.name.endsWith('.bin')) {
                showStatus('Sadece .bin dosyalari kabul edilir!', 'error');
                return;
            }
            if (file.size > 4 * 1024 * 1024) {
                showStatus('Dosya boyutu 4MB\'dan buyuk olamaz!', 'error');
                return;
            }
            selectedFile = file;
            fileName.textContent = '📁 ' + file.name + ' (' + formatSize(file.size) + ')';
            fileInfo.classList.add('show');
            uploadBtn.disabled = false;
            status.classList.remove('show');
        }
        
        function formatSize(bytes) {
            if (bytes < 1024) return bytes + ' B';
            if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
            return (bytes / (1024 * 1024)).toFixed(2) + ' MB';
        }
        
        uploadBtn.addEventListener('click', startUpload);
        
        function startUpload() {
            if (!selectedFile) return;
            
            uploadBtn.disabled = true;
            progressContainer.classList.add('show');
            progressText.classList.add('show');
            
            const xhr = new XMLHttpRequest();
            const formData = new FormData();
            formData.append('firmware', selectedFile);
            
            xhr.upload.addEventListener('progress', (e) => {
                if (e.lengthComputable) {
                    const percent = Math.round((e.loaded / e.total) * 100);
                    progressBar.style.width = percent + '%';
                    progressText.textContent = 'Yukleniyor: ' + percent + '%';
                }
            });
            
            xhr.addEventListener('load', () => {
                if (xhr.status === 200) {
                    showStatus('✅ Guncelleme basarili! Cihaz yeniden baslatiliyor...', 'success');
                    setTimeout(() => { location.href = '/'; }, 5000);
                } else {
                    let errMsg = 'Guncelleme basarisiz!';
                    try {
                        const resp = JSON.parse(xhr.responseText);
                        if (resp.error) errMsg = resp.error;
                    } catch(e) {}
                    showStatus('❌ ' + errMsg, 'error');
                    uploadBtn.disabled = false;
                }
            });
            
            xhr.addEventListener('error', () => {
                showStatus('❌ Baglanti hatasi!', 'error');
                uploadBtn.disabled = false;
            });
            
            xhr.open('POST', '/api/ota');
            xhr.send(formData);
        }
        
        function showStatus(msg, type) {
            status.textContent = msg;
            status.className = 'status show ' + type;
        }
        
        // Heap bilgisi
        fetch('/api/status')
            .then(r => r.json())
            .then(d => {
                if (d.freeHeap) {
                    document.getElementById('heapInfo').textContent = Math.round(d.freeHeap / 1024);
                }
            })
            .catch(() => {});

        // Cihaz kimligi (yan yana cihazlarda hangisini guncelledigini bilmek icin)
        fetch('/api/identity')
            .then(r => r.json())
            .then(d => {
                if (d.deviceName) {
                    document.getElementById('deviceName').textContent = d.deviceName;
                    document.getElementById('deviceLabel').textContent = d.deviceName + ' - OTA Guncelleme';
                    document.title = d.deviceName + ' - Firmware Guncelleme';
                }
                if (d.deviceId)   document.getElementById('deviceId').textContent   = d.deviceId;
                if (d.fwVersion)  document.getElementById('fwVersion').textContent  = 'v' + d.fwVersion;
                if (d.buildDate)  document.getElementById('buildDate').textContent  = d.buildDate;
            })
            .catch(() => {});
    </script>
</body>
</html>
)rawliteral";
        request->send(200, "text/html", html);
    });

    // OTA API durumu
    server.on("/api/ota/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", getStatusJSON());
    });

    // OTA firmware yukleme
    server.on("/api/ota", HTTP_POST,
        // Yukleme tamamlandiginda
        [this](AsyncWebServerRequest *request) {
            if (_state == OTA_SUCCESS) {
                request->send(200, "application/json", "{\"ok\":true,\"msg\":\"Guncelleme basarili, yeniden baslatiliyor...\"}");
                delay(500);
                ESP.restart();
            } else {
                String errJson = "{\"ok\":false,\"error\":\"" + _errorMsg + "\"}";
                request->send(400, "application/json", errJson);
            }
        },
        // Dosya yukleme islemi
        [this](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
            handleOTAUpload(request, filename, index, data, len, final);
        }
    );
}

void OTAService::handleOTAUpload(AsyncWebServerRequest *request, 
                                  const String &filename, 
                                  size_t index, 
                                  uint8_t *data, 
                                  size_t len, 
                                  bool final) {
    // Ilk paket
    if (index == 0) {
        Serial.println("[OTA] Guncelleme basliyor: " + filename);
        _state = OTA_RECEIVING;
        _progress = 0;
        _errorMsg = "";
        _currentSize = 0;
        
        // Content-Length header'dan toplam boyutu al
        if (request->hasHeader("Content-Length")) {
            _totalSize = request->header("Content-Length").toInt();
        } else {
            _totalSize = 0;
        }
        
        if (_onStart) _onStart();
        
        // Flash'i guncelleme icin hazirla
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
            _state = OTA_ERROR;
            _errorMsg = "Flash baslatma hatasi: " + String(Update.errorString());
            Serial.println("[OTA] HATA: " + _errorMsg);
            return;
        }
    }
    
    // Hata durumunda devam etme
    if (_state == OTA_ERROR) return;
    
    // Veri yaz
    if (len > 0) {
        if (Update.write(data, len) != len) {
            _state = OTA_ERROR;
            _errorMsg = "Flash yazma hatasi: " + String(Update.errorString());
            Serial.println("[OTA] HATA: " + _errorMsg);
            Update.abort();
            return;
        }
        
        _currentSize += len;
        
        // Ilerleme hesapla
        if (_totalSize > 0) {
            _progress = (_currentSize * 100) / _totalSize;
        } else {
            _progress = 50; // Bilinmeyen boyut
        }
        
        if (_onProgress) _onProgress(_progress);
        
        // Her 100KB'da log
        if (_currentSize % 102400 < len) {
            Serial.printf("[OTA] Yuklendi: %u bytes (%u%%)\n", _currentSize, _progress);
        }
    }
    
    // Son paket
    if (final) {
        if (Update.end(true)) {
            _state = OTA_SUCCESS;
            _progress = 100;
            Serial.printf("[OTA] Guncelleme tamamlandi! Toplam: %u bytes\n", _currentSize);
            if (_onEnd) _onEnd(true);
        } else {
            _state = OTA_ERROR;
            _errorMsg = "Guncelleme sonlandirma hatasi: " + String(Update.errorString());
            Serial.println("[OTA] HATA: " + _errorMsg);
            if (_onEnd) _onEnd(false);
        }
    }
}
