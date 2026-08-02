#ifndef DEVICE_IDENTITY_H
#define DEVICE_IDENTITY_H

#include <Arduino.h>

// ============================================================
// CIHAZ KIMLIGI
// - deviceId: MAC adresinden turetilen sabit, benzersiz ID (ornek: "KM-A4C138F2")
// - deviceName: Kullanici tarafindan atanmis isim (NVS'de saklanir, yoksa deviceId)
// - AP SSID: "Kulucka-<MAC6>" formatinda (yan yana cihazlarda cakisma olmasin diye)
// - Firmware versiyonu ve donanim modeli: Config.h'den okunur
// ============================================================

namespace DeviceIdentity {
    // NVS'den kullanici tanimli ismi yukler. Setup'ta bir kez cagrilir.
    void begin();

    // "KM-A4C138F2" gibi sabit benzersiz ID. MAC adresinin son 4 byte'indan turetilir.
    const String& getDeviceId();

    // Kullanici ismi varsa onu, yoksa deviceId'yi dondurur.
    const String& getDeviceName();

    // Kullanici tarafindan atanan ismi NVS'e kaydeder.
    // Gecerli karakterler: harf, rakam, bosluk, tire, alt cizgi. Maks DEVICE_NAME_MAX_LEN.
    // Bos string gonderilirse isim silinir (fallback olarak deviceId kullanilir).
    // Dondurdugu: gecerli & kaydedildi = true, dogrulama hatasi = false.
    bool setDeviceName(const String &name);

    // AP SSID — "Kulucka-A4C138F2". Yan yana cihazlarda cakisma olmamasi icin.
    const String& getApSsid();

    // mDNS hostname — "kulucka-a4c138f2" (sadece kucuk harf, nokta yok).
    // Tarayicida "kulucka-a4c138f2.local" olarak erisilir.
    const String& getMdnsHostname();

    // MAC'in okunabilir hali (AA:BB:CC:DD:EE:FF)
    const String& getMacAddress();

    // Firmware versiyon bilgileri (Config.h makrolarini sarar)
    const char* getFirmwareVersion();
    const char* getBuildDate();
    const char* getBuildTime();
    const char* getHardwareModel();

    // Cihaz kimligini JSON olarak dondurur. /api/identity ve banner icin.
    String getIdentityJSON();
}

#endif // DEVICE_IDENTITY_H
