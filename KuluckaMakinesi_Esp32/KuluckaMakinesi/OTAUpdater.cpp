#include "OTAUpdater.h"
#include "Config.h"
#include "DeviceIdentity.h"

#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <Update.h>
#include <esp_task_wdt.h>
#include <mbedtls/md.h>

namespace {
    constexpr const char *NVS_NAMESPACE = "otapull";
    constexpr const char *NVS_KEY_URL   = "url";

    // ArduinoJson bagimliligini eklemeden cok kucuk bir JSON alici.
    // Sadece bekledigimiz alanlari (string veya sayi) cikarir.
    String extractJsonString(const String &json, const char *key) {
        String pattern = String("\"") + key + "\"";
        int keyIdx = json.indexOf(pattern);
        if (keyIdx < 0) return "";
        int colon = json.indexOf(':', keyIdx + pattern.length());
        if (colon < 0) return "";
        int start = json.indexOf('"', colon);
        if (start < 0) return "";
        start++;
        String out;
        out.reserve(64);
        while (start < (int)json.length()) {
            char c = json[start];
            if (c == '\\' && start + 1 < (int)json.length()) {
                char n = json[start + 1];
                if (n == '"')       out += '"';
                else if (n == '\\') out += '\\';
                else if (n == '/')  out += '/';
                else if (n == 'n')  out += '\n';
                else if (n == 't')  out += '\t';
                else if (n == 'r')  out += '\r';
                else                out += n;
                start += 2;
                continue;
            }
            if (c == '"') break;
            out += c;
            start++;
        }
        return out;
    }

    long extractJsonNumber(const String &json, const char *key) {
        String pattern = String("\"") + key + "\"";
        int keyIdx = json.indexOf(pattern);
        if (keyIdx < 0) return -1;
        int colon = json.indexOf(':', keyIdx + pattern.length());
        if (colon < 0) return -1;
        int start = colon + 1;
        while (start < (int)json.length() && (json[start] == ' ' || json[start] == '\t')) start++;
        String num;
        while (start < (int)json.length()) {
            char c = json[start];
            if ((c >= '0' && c <= '9') || c == '-') num += c;
            else break;
            start++;
        }
        if (num.length() == 0) return -1;
        return num.toInt();
    }

    // JSON string escape (UI tarafina dondurulecek changelog/error icin)
    void jsonEscape(String &s) {
        s.replace("\\", "\\\\");
        s.replace("\"", "\\\"");
        s.replace("\n", "\\n");
        s.replace("\r", "\\r");
        s.replace("\t", "\\t");
    }
}

OTAUpdater::OTAUpdater()
    : _state(STATE_IDLE)
    , _remoteSize(0)
    , _progress(0)
    , _checkRequested(false)
    , _pullRequested(false)
{
}

void OTAUpdater::begin() {
    loadUrlFromNvs();
    if (_updateUrl.length() == 0) {
        _updateUrl = OTA_UPDATE_URL_DEFAULT;
    }
    Serial.printf("[OTAPull] Hazir. version.json URL: %s\n",
                  _updateUrl.length() ? _updateUrl.c_str() : "(ayarlanmamis)");
}

void OTAUpdater::loadUrlFromNvs() {
    Preferences p;
    if (!p.begin(NVS_NAMESPACE, true)) return;
    _updateUrl = p.getString(NVS_KEY_URL, "");
    p.end();
}

void OTAUpdater::saveUrlToNvs(const String &url) {
    Preferences p;
    if (!p.begin(NVS_NAMESPACE, false)) return;
    if (url.length() == 0) p.remove(NVS_KEY_URL);
    else                   p.putString(NVS_KEY_URL, url);
    p.end();
}

bool OTAUpdater::setUpdateUrl(const String &url) {
    String u = url;
    u.trim();
    if (u.length() > 256) return false;
    if (u.length() > 0 && !u.startsWith("https://") && !u.startsWith("http://")) {
        return false;
    }
    _updateUrl = u;
    saveUrlToNvs(u);
    // Yeni URL girildiginde onceki remote bilgisini sifirla
    resetRemoteInfo();
    _state = STATE_IDLE;
    _error = "";
    return true;
}

String OTAUpdater::getUpdateUrl() const { return _updateUrl; }

void OTAUpdater::requestCheck() {
    if (_state == STATE_DOWNLOADING || _state == STATE_VERIFYING) return;
    _checkRequested = true;
}

void OTAUpdater::requestPull() {
    if (_state == STATE_DOWNLOADING || _state == STATE_VERIFYING) return;
    // Pull icin onceden check yapilmis ve remote bilgisi gecerli olmali
    if (_remoteUrl.length() == 0 || _remoteSha256.length() != 64) {
        setError("Once 'Guncelleme Kontrol Et' butonuna basin");
        return;
    }
    _pullRequested = true;
}

void OTAUpdater::update() {
    if (_checkRequested) {
        _checkRequested = false;
        doCheck();
    }
    if (_pullRequested) {
        _pullRequested = false;
        doPull();
    }
}

OTAUpdater::State OTAUpdater::getState() const { return _state; }
uint8_t           OTAUpdater::getProgress() const { return _progress; }
String            OTAUpdater::getError() const { return _error; }
String            OTAUpdater::getRemoteVersion() const { return _remoteVersion; }
String            OTAUpdater::getRemoteChangelog() const { return _remoteChangelog; }
String            OTAUpdater::getRemoteBuildDate() const { return _remoteBuildDate; }
size_t            OTAUpdater::getRemoteSize() const { return _remoteSize; }

const char *OTAUpdater::getStateText() const {
    switch (_state) {
        case STATE_IDLE:             return "idle";
        case STATE_CHECKING:         return "checking";
        case STATE_UPDATE_AVAILABLE: return "update_available";
        case STATE_NO_UPDATE:        return "no_update";
        case STATE_DOWNLOADING:      return "downloading";
        case STATE_VERIFYING:        return "verifying";
        case STATE_SUCCESS:          return "success";
        case STATE_ERROR:            return "error";
    }
    return "unknown";
}

void OTAUpdater::resetRemoteInfo() {
    _remoteVersion   = "";
    _remoteUrl       = "";
    _remoteSha256    = "";
    _remoteChangelog = "";
    _remoteBuildDate = "";
    _remoteSize      = 0;
    _progress        = 0;
}

void OTAUpdater::setError(const String &msg) {
    _state = STATE_ERROR;
    _error = msg;
    Serial.printf("[OTAPull] HATA: %s\n", msg.c_str());
}

// ============================================================
//  CHECK — version.json indir + parse et + karsilastir
// ============================================================
void OTAUpdater::doCheck() {
    if (WiFi.status() != WL_CONNECTED) {
        setError("WiFi bagli degil — internete erisim yok");
        return;
    }
    if (_updateUrl.length() == 0) {
        setError("version.json URL ayarlanmamis — POST /api/ota/url ile ayarlayin");
        return;
    }

    _state = STATE_CHECKING;
    _error = "";
    Serial.printf("[OTAPull] Kontrol: %s\n", _updateUrl.c_str());

    String body, err;
    if (!fetchVersionJson(body, err))  { setError(err); return; }
    if (!parseVersionJson(body, err))  { setError(err); return; }

    if (isVersionNewer(_remoteVersion)) {
        _state = STATE_UPDATE_AVAILABLE;
        Serial.printf("[OTAPull] Yeni surum: %s (mevcut: %s, boyut: %u B)\n",
                      _remoteVersion.c_str(), FW_VERSION_STRING, (unsigned)_remoteSize);
    } else {
        _state = STATE_NO_UPDATE;
        Serial.printf("[OTAPull] Guncel (sunucu: %s, mevcut: %s)\n",
                      _remoteVersion.c_str(), FW_VERSION_STRING);
    }
}

bool OTAUpdater::fetchVersionJson(String &body, String &errOut) {
    WiFiClientSecure secure;
    WiFiClient       plain;
    WiFiClient      *client = nullptr;

    if (_updateUrl.startsWith("https://")) {
        secure.setInsecure();   // TLS dogrulamasi yok — SHA256 ile koruma
        secure.setTimeout(OTA_DOWNLOAD_TIMEOUT / 1000);
        client = &secure;
    } else {
        client = &plain;
    }

    HTTPClient http;
    String ua = String(OTA_USER_AGENT) + "/" + FW_VERSION_STRING
                + " (" + DeviceIdentity::getDeviceId() + ")";
    http.setUserAgent(ua);
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    http.setTimeout(OTA_DOWNLOAD_TIMEOUT);
    http.setConnectTimeout(OTA_DOWNLOAD_TIMEOUT);

    if (!http.begin(*client, _updateUrl)) {
        errOut = "HTTP baslatma hatasi";
        return false;
    }

    int code = http.GET();
    if (code != 200) {
        errOut = "Sunucu cevabi: HTTP " + String(code);
        if (code < 0) errOut += " (" + http.errorToString(code) + ")";
        http.end();
        return false;
    }

    int len = http.getSize();
    if (len > OTA_VERSION_JSON_MAX) {
        errOut = "version.json cok buyuk (>4KB)";
        http.end();
        return false;
    }

    body = http.getString();
    http.end();

    if (body.length() == 0) {
        errOut = "Bos cevap";
        return false;
    }
    return true;
}

bool OTAUpdater::parseVersionJson(const String &body, String &errOut) {
    _remoteVersion   = extractJsonString(body, "version");
    _remoteUrl       = extractJsonString(body, "url");
    _remoteSha256    = extractJsonString(body, "sha256");
    _remoteChangelog = extractJsonString(body, "changelog");
    _remoteBuildDate = extractJsonString(body, "build_date");
    long sz          = extractJsonNumber(body, "size");
    _remoteSize      = (sz > 0) ? (size_t)sz : 0;

    // sha256 lower case
    _remoteSha256.toLowerCase();

    if (_remoteVersion.length() == 0) {
        errOut = "version.json: 'version' alani eksik";
        return false;
    }
    if (_remoteUrl.length() == 0) {
        errOut = "version.json: 'url' alani eksik";
        return false;
    }
    if (_remoteSha256.length() != 64) {
        errOut = "version.json: 'sha256' 64 hex karakter olmali";
        return false;
    }
    if (!_remoteUrl.startsWith("https://") && !_remoteUrl.startsWith("http://")) {
        errOut = "version.json: 'url' http/https ile baslamali";
        return false;
    }
    // sha256 karakter kontrolu
    for (int i = 0; i < 64; i++) {
        char c = _remoteSha256[i];
        bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        if (!ok) { errOut = "version.json: 'sha256' gecersiz hex"; return false; }
    }
    return true;
}

bool OTAUpdater::parseSemver(const String &s, int &maj, int &min, int &patch) const {
    maj = min = patch = 0;
    int d1 = s.indexOf('.');
    if (d1 < 0) return false;
    int d2 = s.indexOf('.', d1 + 1);
    if (d2 < 0) return false;
    maj   = s.substring(0, d1).toInt();
    min   = s.substring(d1 + 1, d2).toInt();
    patch = s.substring(d2 + 1).toInt();
    return true;
}

bool OTAUpdater::isVersionNewer(const String &remote) const {
    int rMaj, rMin, rPat;
    if (!parseSemver(remote, rMaj, rMin, rPat)) return false;
    if (rMaj != FW_VERSION_MAJOR) return rMaj > FW_VERSION_MAJOR;
    if (rMin != FW_VERSION_MINOR) return rMin > FW_VERSION_MINOR;
    return rPat > FW_VERSION_PATCH;
}

// ============================================================
//  PULL — .bin indir + SHA256 dogrula + flash et + reboot
// ============================================================
void OTAUpdater::doPull() {
    if (WiFi.status() != WL_CONNECTED) {
        setError("WiFi bagli degil");
        return;
    }
    if (_remoteUrl.length() == 0 || _remoteSha256.length() != 64) {
        setError("Remote bilgi eksik — once check yapin");
        return;
    }

    Serial.printf("[OTAPull] Indirme: %s (%u B)\n",
                  _remoteUrl.c_str(), (unsigned)_remoteSize);

    _state    = STATE_DOWNLOADING;
    _progress = 0;
    _error    = "";

    String err;
    if (!streamDownloadAndFlash(err)) {
        setError(err);
        return;
    }

    _state    = STATE_SUCCESS;
    _progress = 100;
    Serial.println("[OTAPull] BASARILI! 2 saniye sonra reboot...");
    delay(2000);
    ESP.restart();
}

bool OTAUpdater::streamDownloadAndFlash(String &errOut) {
    WiFiClientSecure secure;
    WiFiClient       plain;
    WiFiClient      *client = nullptr;

    if (_remoteUrl.startsWith("https://")) {
        secure.setInsecure();
        secure.setTimeout(OTA_DOWNLOAD_TIMEOUT / 1000);
        client = &secure;
    } else {
        client = &plain;
    }

    HTTPClient http;
    String ua = String(OTA_USER_AGENT) + "/" + FW_VERSION_STRING
                + " (" + DeviceIdentity::getDeviceId() + ")";
    http.setUserAgent(ua);
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    http.setTimeout(OTA_DOWNLOAD_TIMEOUT);
    http.setConnectTimeout(OTA_DOWNLOAD_TIMEOUT);

    if (!http.begin(*client, _remoteUrl)) {
        errOut = "HTTP baslatma hatasi";
        return false;
    }

    int code = http.GET();
    if (code != 200) {
        errOut = "Sunucu cevabi: HTTP " + String(code);
        if (code < 0) errOut += " (" + http.errorToString(code) + ")";
        http.end();
        return false;
    }

    int contentLength = http.getSize();
    if (contentLength <= 0 && _remoteSize > 0) {
        contentLength = (int)_remoteSize;
    }
    if (contentLength <= 0) {
        errOut = "Content-Length yok ve version.json'daki 'size' gecersiz";
        http.end();
        return false;
    }

    if (!Update.begin((size_t)contentLength, U_FLASH)) {
        errOut = "Update.begin: " + String(Update.errorString());
        http.end();
        return false;
    }

    // SHA256 streaming baslat
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!info || mbedtls_md_setup(&ctx, info, 0) != 0 || mbedtls_md_starts(&ctx) != 0) {
        errOut = "SHA256 baslatma hatasi";
        Update.abort();
        mbedtls_md_free(&ctx);
        http.end();
        return false;
    }

    WiFiClient *stream = http.getStreamPtr();
    uint8_t buf[OTA_DOWNLOAD_BUFFER];
    size_t  totalRead = 0;
    unsigned long lastLog = 0;
    unsigned long lastData = millis();

    while (http.connected() && totalRead < (size_t)contentLength) {
        size_t avail = stream->available();
        if (avail > 0) {
            size_t toRead = (avail > sizeof(buf)) ? sizeof(buf) : avail;
            int r = stream->read(buf, toRead);
            if (r <= 0) break;

            if (Update.write(buf, r) != (size_t)r) {
                errOut = "Flash yazma hatasi: " + String(Update.errorString());
                Update.abort();
                mbedtls_md_free(&ctx);
                http.end();
                return false;
            }
            if (mbedtls_md_update(&ctx, buf, r) != 0) {
                errOut = "SHA256 guncelleme hatasi";
                Update.abort();
                mbedtls_md_free(&ctx);
                http.end();
                return false;
            }

            totalRead += r;
            _progress = (uint8_t)((totalRead * 100) / (size_t)contentLength);
            lastData  = millis();

            esp_task_wdt_reset();   // watchdog besle

            if (millis() - lastLog > 1000) {
                lastLog = millis();
                Serial.printf("[OTAPull] %u/%d (%u%%)\n",
                              (unsigned)totalRead, contentLength, _progress);
            }
        } else {
            // Veri yoksa kisa bekleme; 10sn boyunca veri gelmezse iptal
            if (millis() - lastData > 10000) {
                errOut = "Veri akisi durdu (10sn timeout)";
                Update.abort();
                mbedtls_md_free(&ctx);
                http.end();
                return false;
            }
            delay(5);
            esp_task_wdt_reset();
        }
    }

    http.end();

    if (totalRead != (size_t)contentLength) {
        errOut = "Indirme yarim kaldi (" + String((unsigned)totalRead) + "/"
                 + String(contentLength) + ")";
        Update.abort();
        mbedtls_md_free(&ctx);
        return false;
    }

    // SHA256 sonuclandir + karsilastir
    _state = STATE_VERIFYING;
    uint8_t hash[32];
    mbedtls_md_finish(&ctx, hash);
    mbedtls_md_free(&ctx);

    String calc = bytesToHex(hash, 32);
    if (calc != _remoteSha256) {
        errOut = "SHA256 uyusmazligi! Beklenen: " + _remoteSha256.substring(0, 16)
                 + "..., hesaplanan: " + calc.substring(0, 16) + "...";
        Update.abort();
        return false;
    }
    Serial.printf("[OTAPull] SHA256 OK: %s\n", calc.c_str());

    if (!Update.end(true)) {
        errOut = "Update.end: " + String(Update.errorString());
        return false;
    }
    return true;
}

String OTAUpdater::bytesToHex(const uint8_t *bytes, size_t len) {
    String s;
    s.reserve(len * 2);
    for (size_t i = 0; i < len; i++) {
        char b[3];
        snprintf(b, sizeof(b), "%02x", bytes[i]);
        s += b;
    }
    return s;
}

String OTAUpdater::getStatusJSON() const {
    String json = "{";
    json += "\"state\":\""; json += getStateText(); json += "\"";
    json += ",\"progress\":" + String(_progress);
    json += ",\"currentVersion\":\"" + String(FW_VERSION_STRING) + "\"";

    if (_updateUrl.length()) {
        String u = _updateUrl;
        jsonEscape(u);
        json += ",\"updateUrl\":\"" + u + "\"";
    } else {
        json += ",\"updateUrl\":\"\"";
    }

    if (_remoteVersion.length()) {
        json += ",\"remoteVersion\":\"" + _remoteVersion + "\"";
    }
    if (_remoteBuildDate.length()) {
        json += ",\"remoteBuildDate\":\"" + _remoteBuildDate + "\"";
    }
    if (_remoteSize > 0) {
        json += ",\"remoteSize\":" + String((unsigned)_remoteSize);
    }
    if (_remoteChangelog.length()) {
        String c = _remoteChangelog;
        jsonEscape(c);
        json += ",\"changelog\":\"" + c + "\"";
    }
    if (_error.length()) {
        String e = _error;
        jsonEscape(e);
        json += ",\"error\":\"" + e + "\"";
    }
    json += "}";
    return json;
}
