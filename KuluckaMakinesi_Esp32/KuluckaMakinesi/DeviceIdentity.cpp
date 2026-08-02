#include "DeviceIdentity.h"
#include "Config.h"
#include <Preferences.h>
#include <WiFi.h>
#include <esp_mac.h>

namespace {
    // Ayri NVS namespace — PersistentStorage/StorageService ile cakismaz
    constexpr const char* NVS_NAMESPACE   = "device";
    constexpr const char* NVS_KEY_NAME    = "name";

    String g_deviceId;       // "KM-A4C138F2"
    String g_deviceName;     // Kullanici ismi veya deviceId fallback
    String g_apSsid;         // "Kulucka-A4C138F2"
    String g_mdnsHost;       // "kulucka-a4c138f2"
    String g_macAddress;     // "AA:BB:CC:DD:EE:FF"
    bool   g_initialized = false;

    bool isValidNameChar(char c) {
        return isalnum((unsigned char)c) || c == ' ' || c == '-' || c == '_';
    }

    bool isValidName(const String &name) {
        if (name.length() == 0) return true;  // Bos = sil (gecerli)
        if (name.length() > DEVICE_NAME_MAX_LEN) return false;
        for (size_t i = 0; i < name.length(); i++) {
            if (!isValidNameChar(name[i])) return false;
        }
        return true;
    }

    void buildIdentityStrings() {
        // MAC adresini cek — WiFi baslatilmamis da olsa calisir
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);

        char macBuf[18];
        snprintf(macBuf, sizeof(macBuf), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        g_macAddress = macBuf;

        // Son 4 byte -> 8 hex karakter (cihaz kimligi icin yeterli tekillik)
        char suffix[9];
        snprintf(suffix, sizeof(suffix), "%02X%02X%02X%02X",
                 mac[2], mac[3], mac[4], mac[5]);

        g_deviceId = String(DEVICE_ID_PREFIX) + "-" + suffix;
        g_apSsid   = String("Kulucka-") + suffix;

        // mDNS hostname: RFC uyumlu, kucuk harf
        String host = String("kulucka-") + suffix;
        host.toLowerCase();
        g_mdnsHost = host;
    }

    void loadStoredName() {
        Preferences prefs;
        if (!prefs.begin(NVS_NAMESPACE, true)) {
            g_deviceName = g_deviceId;
            return;
        }
        String stored = prefs.getString(NVS_KEY_NAME, "");
        prefs.end();
        g_deviceName = (stored.length() > 0) ? stored : g_deviceId;
    }
}

namespace DeviceIdentity {

void begin() {
    if (g_initialized) return;
    buildIdentityStrings();
    loadStoredName();
    g_initialized = true;

    Serial.println("--------------------------------------------");
    Serial.printf("[ID]  Cihaz: %s\n", g_deviceName.c_str());
    Serial.printf("[ID]  ID:    %s\n", g_deviceId.c_str());
    Serial.printf("[ID]  MAC:   %s\n", g_macAddress.c_str());
    Serial.printf("[ID]  FW:    %s (build %s %s)\n",
                  FW_VERSION_STRING, FW_BUILD_DATE, FW_BUILD_TIME);
    Serial.printf("[ID]  HW:    %s / %s\n", HW_BOARD, HW_MODEL);
    Serial.printf("[ID]  mDNS:  %s.local\n", g_mdnsHost.c_str());
    Serial.println("--------------------------------------------");
}

const String& getDeviceId()      { return g_deviceId; }
const String& getDeviceName()    { return g_deviceName; }
const String& getApSsid()        { return g_apSsid; }
const String& getMdnsHostname()  { return g_mdnsHost; }
const String& getMacAddress()    { return g_macAddress; }

bool setDeviceName(const String &name) {
    String trimmed = name;
    trimmed.trim();

    if (!isValidName(trimmed)) return false;

    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) return false;

    bool ok;
    if (trimmed.length() == 0) {
        prefs.remove(NVS_KEY_NAME);
        g_deviceName = g_deviceId;
        ok = true;
    } else {
        ok = (prefs.putString(NVS_KEY_NAME, trimmed) == trimmed.length());
        if (ok) g_deviceName = trimmed;
    }
    prefs.end();
    return ok;
}

const char* getFirmwareVersion() { return FW_VERSION_STRING; }
const char* getBuildDate()       { return FW_BUILD_DATE; }
const char* getBuildTime()       { return FW_BUILD_TIME; }
const char* getHardwareModel()   { return HW_MODEL; }

String getIdentityJSON() {
    String json = "{";
    json += "\"deviceId\":\""   + g_deviceId   + "\"";
    json += ",\"deviceName\":\"" + g_deviceName + "\"";
    json += ",\"mac\":\""        + g_macAddress + "\"";
    json += ",\"apSSID\":\""     + g_apSsid     + "\"";
    json += ",\"mdns\":\""       + g_mdnsHost   + ".local\"";
    json += ",\"fwVersion\":\""  + String(FW_VERSION_STRING) + "\"";
    json += ",\"fwMajor\":"      + String(FW_VERSION_MAJOR);
    json += ",\"fwMinor\":"      + String(FW_VERSION_MINOR);
    json += ",\"fwPatch\":"      + String(FW_VERSION_PATCH);
    json += ",\"buildDate\":\""  + String(FW_BUILD_DATE) + "\"";
    json += ",\"buildTime\":\""  + String(FW_BUILD_TIME) + "\"";
    json += ",\"hwBoard\":\""    + String(HW_BOARD) + "\"";
    json += ",\"hwModel\":\""    + String(HW_MODEL) + "\"";
    json += "}";
    return json;
}

} // namespace DeviceIdentity
