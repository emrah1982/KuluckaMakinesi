#ifndef OTA_UPDATER_H
#define OTA_UPDATER_H

#include <Arduino.h>

// ============================================================
//  PULL-BASED OTA — Internet uzerinden otomatik firmware guncelleme
//
//  Akis:
//    1. requestCheck()  -> sunucudan version.json indirilir, parse edilir
//    2. Eger remote version > FW_VERSION_STRING -> STATE_UPDATE_AVAILABLE
//    3. requestPull()   -> .bin indirilir + SHA256 hesaplanir
//    4. Hesaplanan SHA256 version.json'daki ile eslesirse Update.end(true)
//    5. Otomatik reboot
//
//  Guvenlik:
//    - HTTPS (WiFiClientSecure) ile transport sifrelenir
//    - SHA256 dogrulamasi binary butunlugunu garanti eder
//    - !!! Bu versiyon TLS sertifika dogrulamasi yapmaz (setInsecure).
//        MITM'e karsi tam koruma icin version.json'un Ed25519 ile
//        imzalanip public key'in firmware'e gomulmesi onerilir.
// ============================================================

class OTAUpdater {
public:
    enum State {
        STATE_IDLE,
        STATE_CHECKING,           // version.json indiriliyor
        STATE_UPDATE_AVAILABLE,   // Yeni surum bulundu, kullanici onayi bekleniyor
        STATE_NO_UPDATE,          // Guncel
        STATE_DOWNLOADING,        // .bin indiriliyor
        STATE_VERIFYING,          // SHA256 karsilastiriliyor
        STATE_SUCCESS,            // Basarili, reboot ediliyor
        STATE_ERROR
    };

    OTAUpdater();

    // NVS'den update URL'ini yukler. Setup'ta cagrilir.
    void begin();

    // Loop'tan periyodik cagrilir — istek varsa ilgili islemi yapar.
    // Indirme blocking (internet hizina gore 5-60 sn), ana dongu duraklar.
    void update();

    // version.json URL'i (NVS'de kalici saklanir)
    String getUpdateUrl() const;
    bool   setUpdateUrl(const String &url);

    // Asenkron tetikleyiciler (endpoint'ten cagrilir, update() icinde islenir)
    void requestCheck();
    void requestPull();

    // Durum sorgulama
    State       getState() const;
    const char* getStateText() const;
    uint8_t     getProgress() const;    // 0-100
    String      getError() const;

    // version.json'dan alinan son bilgiler
    String getRemoteVersion() const;
    String getRemoteChangelog() const;
    String getRemoteBuildDate() const;
    size_t getRemoteSize() const;

    // Tum durumu JSON olarak dondur (UI polling icin)
    String getStatusJSON() const;

private:
    State   _state;
    String  _updateUrl;

    // Sunucudan alinan
    String  _remoteVersion;
    String  _remoteUrl;
    String  _remoteSha256;
    String  _remoteChangelog;
    String  _remoteBuildDate;
    size_t  _remoteSize;

    uint8_t _progress;
    String  _error;

    bool    _checkRequested;
    bool    _pullRequested;

    void loadUrlFromNvs();
    void saveUrlToNvs(const String &url);

    void doCheck();
    void doPull();

    bool fetchVersionJson(String &body, String &errOut);
    bool parseVersionJson(const String &body, String &errOut);
    bool isVersionNewer(const String &remote) const;
    bool parseSemver(const String &s, int &maj, int &min, int &patch) const;

    bool streamDownloadAndFlash(String &errOut);
    static String bytesToHex(const uint8_t *bytes, size_t len);

    void resetRemoteInfo();
    void setError(const String &msg);
};

#endif // OTA_UPDATER_H
