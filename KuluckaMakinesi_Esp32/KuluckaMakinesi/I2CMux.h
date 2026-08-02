#ifndef I2C_MUX_H
#define I2C_MUX_H

#include <Arduino.h>
#include <Wire.h>
#include "Config.h"

namespace I2CMux {

void begin();
bool isReady();
uint8_t addr();
bool ping();
void closeAll();
bool selectChannel(uint8_t ch);

// ---------- Bus saglik / kurtarma ----------
// Bu MUX'in arkasinda RTC, iki sicaklik sensoru, CO2, IR ve ROLELER var.
// Bus kilitlenirse sistem hem kor kalir hem de roleleri anahtarlayamaz
// (isitici son durumunda takili kalir). Bu yuzden kurtarma kritiktir.

// Bus'i zorla kurtar: takilan slave'i SCL darbeleriyle birak, Wire'i
// yeniden baslat, MUX'i tekrar bul. Basarili olursa true.
bool recover();

// Art arda basarisiz kanal secimi sayisi (basarili secimde sifirlanir)
uint8_t getFailCount();

// Acilistan beri kac kez bus kurtarma yapildi (tani icin)
uint32_t getRecoverCount();

// MUX bulundu ve son islemler basarili mi
bool isHealthy();

}

#endif // I2C_MUX_H
