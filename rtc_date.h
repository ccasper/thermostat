#ifndef RTC_DATE_H_
#define RTC_DATE_H_

#include "settings.h"
#include "uRTCLib.h"

// Virtual class to allow testing on a non-arduino platform.
class Rtc {
  public:
    virtual Date Now() = 0;
    virtual void Set(const Date& date) = 0;
};

class RtcDate : public Rtc {
  public:
    RtcDate() : rtc_(0x68, URTCLIB_MODEL_DS1307) {};

    Date Now() {
      rtc_.refresh();
      Date date;
      date.hour = rtc_.hour();
      date.minute = rtc_.minute();
      date.day_of_week = rtc_.dayOfWeek();
      return date;
    }

    void Set(const Date& date) {
      rtc_.set(/*seconds=*/0, date.minute, date.hour, date.day_of_week, /*dayOfMonth=*/1,
                           /*month=*/1, /*year*/ 20);
    }

  private:
    uRTCLib rtc_;
};

#endif
