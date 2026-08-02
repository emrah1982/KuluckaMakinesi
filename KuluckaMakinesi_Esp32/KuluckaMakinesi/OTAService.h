#ifndef OTA_SERVICE_H
#define OTA_SERVICE_H

#include <Arduino.h>
#include <WiFi.h>
#include <Update.h>
#include <ESPAsyncWebServer.h>
#include "Config.h"

enum OTAState {
    OTA_IDLE,
    OTA_RECEIVING,
    OTA_SUCCESS,
    OTA_ERROR
};

class OTAService {
public:
    OTAService();

    void begin(AsyncWebServer &server);
    void update();

    OTAState getState() const;
    uint8_t  getProgress() const;
    String   getErrorMessage() const;
    String   getStatusJSON() const;

    void setOnStartCallback(void (*callback)());
    void setOnEndCallback(void (*callback)(bool success));
    void setOnProgressCallback(void (*callback)(uint8_t progress));

private:
    OTAState _state;
    uint8_t  _progress;
    String   _errorMsg;
    size_t   _totalSize;
    size_t   _currentSize;

    void (*_onStart)();
    void (*_onEnd)(bool success);
    void (*_onProgress)(uint8_t progress);

    void setupRoutes(AsyncWebServer &server);
    void handleOTAUpload(AsyncWebServerRequest *request, 
                         const String &filename, 
                         size_t index, 
                         uint8_t *data, 
                         size_t len, 
                         bool final);
};

#endif // OTA_SERVICE_H
