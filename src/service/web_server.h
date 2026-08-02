#ifndef WEB_SERVER_PAGES_H
#define WEB_SERVER_PAGES_H

#include <Arduino.h>

// ============================================================
//  PROGMEM Web Sayfalari
//  SPIFFS yerine firmware icine gomulu HTML/CSS/JS
//  data/ klasorunun PROGMEM karsiligi
// ============================================================

extern const char PAGE_INDEX_HTML[] PROGMEM;
extern const char PAGE_STYLE_CSS[] PROGMEM;
extern const char PAGE_APP_JS[] PROGMEM;

#endif // WEB_SERVER_PAGES_H
