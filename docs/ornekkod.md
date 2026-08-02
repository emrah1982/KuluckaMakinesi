#include <Wire.h>
#include <WiFi.h>
#include "Adafruit_SHT31.h"
#include "RTClib.h"

// WIFI
const char* ssid = "WIFI_ADI";
const char* password = "SIFRE";

Adafruit_SHT31 sht1 = Adafruit_SHT31();
Adafruit_SHT31 sht2 = Adafruit_SHT31();
RTC_DS3231 rtc;

// PINLER
#define HEATER_PIN 25
#define FAN_PIN 27
#define HUMIDIFIER_PIN 26

// PWM
#define PWM_HEATER 0
#define PWM_FAN 1

// PID
double setTemp = 37.5;
double inputTemp, output;
double Kp = 20, Ki = 0.8, Kd = 5;
double integral = 0, lastError = 0;

// AutoTune
bool autoTune = true;
double Ku = 0, Tu = 0;
double maxTemp = 0, minTemp = 100;
unsigned long lastSwitchTime = 0;
bool heating = true;
int cycleCount = 0;

// Nem
float humLow = 50;
float humHigh = 65;
bool humidifierState = false;
unsigned long humidifierDelay = 0;

// Filtre
float filteredTemp = 0;
float alpha = 0.2;

// Alarm
float alarmHigh = 38.5;
float alarmLow = 36.0;

// Gün
int startDay = 0;

// Alarm fonksiyon
void sendAlert(String msg) {
Serial.println("ALARM: " + msg);
}

// AUTO PID
void autoTunePID(double temp) {

if (!autoTune) return;

if (heating) ledcWrite(PWM_HEATER, 200);
else ledcWrite(PWM_HEATER, 0);

if (temp > maxTemp) maxTemp = temp;
if (temp < minTemp) minTemp = temp;

if (temp > setTemp + 0.2 && heating) {
heating = false;
lastSwitchTime = millis();
cycleCount++;
}

if (temp < setTemp - 0.2 && !heating) {
heating = true;
lastSwitchTime = millis();
}

if (cycleCount >= 6) {

```
double amplitude = (maxTemp - minTemp) / 2.0;
Tu = (millis() - lastSwitchTime) / 1000.0;

Ku = (4.0 * 200) / (3.14 * amplitude);

Kp = 0.6 * Ku;
Ki = 1.2 * Ku / Tu;
Kd = 0.075 * Ku * Tu;

Serial.println("AUTO TUNE TAMAMLANDI");
autoTune = false;
```

}
}

void setup() {
Serial.begin(115200);

WiFi.begin(ssid, password);

pinMode(HUMIDIFIER_PIN, OUTPUT);

ledcSetup(PWM_HEATER, 1, 8);
ledcAttachPin(HEATER_PIN, PWM_HEATER);

ledcSetup(PWM_FAN, 25000, 8);
ledcAttachPin(FAN_PIN, PWM_FAN);

Wire.begin();

sht1.begin(0x44);
sht2.begin(0x45);

rtc.begin();
startDay = rtc.now().day();
}

void loop() {

float t1 = sht1.readTemperature();
float t2 = sht2.readTemperature();
float h1 = sht1.readHumidity();
float h2 = sht2.readHumidity();

if (isnan(t1)) t1 = t2;
if (isnan(t2)) t2 = t1;

float avgTemp = (t1 + t2) / 2.0;
float avgHum = (h1 + h2) / 2.0;

// Spike filtre
if (abs(avgTemp - filteredTemp) > 2) {
avgTemp = filteredTemp;
}

// EMA filtre
filteredTemp = alpha * avgTemp + (1 - alpha) * filteredTemp;
inputTemp = filteredTemp;

// Gün hesabı
int dayDiff = rtc.now().day() - startDay;
if(dayDiff < 0) dayDiff += 30;

// EVRE
if (dayDiff <= 18) {
setTemp = 37.5;
humLow = 50;
humHigh = 55;
} else {
setTemp = 37.2;
humLow = 65;
humHigh = 75;
}

// AUTO TUNE
autoTunePID(inputTemp);

// PID
if (!autoTune) {
double error = setTemp - inputTemp;
integral += error;

```
if(integral > 100) integral = 100;
if(integral < -100) integral = -100;

double derivative = error - lastError;
output = Kp * error + Ki * integral + Kd * derivative;
lastError = error;

int heaterPWM = constrain(output, 0, 255);
ledcWrite(PWM_HEATER, heaterPWM);
```

}

// FAN
int fanPWM = map(inputTemp, 36, 38, 80, 255);
fanPWM = constrain(fanPWM, 80, 255);
ledcWrite(PWM_FAN, fanPWM);

// Nem gecikmeli
if (avgHum < humLow && !humidifierState) {
digitalWrite(HUMIDIFIER_PIN, HIGH);
humidifierState = true;
}

if (avgHum > humHigh && humidifierState) {
humidifierDelay = millis();
humidifierState = false;
}

if (!humidifierState && millis() - humidifierDelay > 10000) {
digitalWrite(HUMIDIFIER_PIN, LOW);
}

// Alarm
if (inputTemp > alarmHigh) sendAlert("YUKSEK SICAKLIK!");
if (inputTemp < alarmLow) sendAlert("DUSUK SICAKLIK!");

// Debug
Serial.print("Temp: "); Serial.print(inputTemp);
Serial.print(" Hum: "); Serial.print(avgHum);
Serial.print(" Kp: "); Serial.print(Kp);
Serial.print(" PWM: "); Serial.println(output);

delay(2000);
}
