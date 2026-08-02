#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include "RTClib.h"
#include "../config/Config.h"

class RTCManager {
public:
    RTCManager();

    bool begin();
    DateTime now() const;
    int  getElapsedDays() const;
    void setStartDate(const DateTime &date);
    DateTime getStartDate() const;
    String getFormattedTime() const;
    String getFormattedDate() const;
    float getModuleTemperature();

private:
    mutable RTC_DS3231 _rtc;
    DateTime   _startDate;
    bool       _initialized;

    void selectMuxChannel() const;
};

#endif // RTC_MANAGER_H
