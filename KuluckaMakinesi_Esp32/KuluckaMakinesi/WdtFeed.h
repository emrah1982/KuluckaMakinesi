#ifndef WDT_FEED_H
#define WDT_FEED_H

#include <esp_task_wdt.h>

// =====================================================================
//  wdtFeed() - Guvenli watchdog besleme
//
//  Watchdog bilerek setup() sonunda baslatilir: begin() zinciri (splash
//  animasyonu, SD denemesi, sensor taramasi) uzun surebilir ve bu sirada
//  panic reset istemeyiz.
//
//  Ancak begin() icindeki besleme cagrilari o noktada henuz WDT'ye abone
//  olmayan bir task'tan geliyordu. Her cagri sunu basiyordu:
//      E (899) task_wdt: esp_task_wdt_reset(707): task not found
//  Onlarca satir halinde seri log'u doldurup gercek tani mesajlarini
//  (sensor/MUX hatalari) goze batmaz hale getiriyordu.
//
//  Cozum: abone degilsek sessizce atla. Watchdog semantigi degismez;
//  sadece anlamsiz hata ciktisi susturulur.
// =====================================================================
static inline void wdtFeed() {
    if (esp_task_wdt_status(NULL) == ESP_OK) esp_task_wdt_reset();
}

#endif // WDT_FEED_H
