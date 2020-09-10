#ifndef INTERFACES_H_
#define INTERFACES_H_

#define UNUSED(x) (void)(x)

#include "print.h"

// This class defines interfaces that are hardware agnostic.
//
// This allows unit testing to use mock like interfaces to test much of the logic.
namespace thermostat {

enum class Error {
  STATUS_NONE,
  STATUS_OK,
  BME_SENSOR_FAIL,
  HEAT_AND_COOL,
  MENU_DISPLAY_ARG,
};

struct Date {
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t day_of_week = 0;
};

class Clock {
  public:
    virtual Date Now() = 0;
    virtual void Set(const Date& date) = 0;

    virtual uint32_t Millis() const = 0;

    // Calculate the millis since the previous time accounting for wrap around.
    uint32_t millisSince(const uint32_t previous) {
      return millisDiff(previous, Millis());
    }

    // Calculate the millis since the previous time accounting for wrap around.
    uint32_t secondsSince(const uint32_t previous) {
      return millisDiff(previous, Millis()) / 1000 /*ms/sec*/;
    }

    // Calculate the millis since the previous time accounting for wrap around.
    uint32_t minutesSince(const uint32_t previous) {
      return millisDiff(previous, Millis()) / 1000 /*ms/sec*/ / 60 /*sec/min*/;
    }
    // Calculate the millis since the previous time accounting for wrap around.
    uint32_t hoursSince(const uint32_t previous) {
      return minutesSince(previous) / 60 /*mins/hour*/;
    }
    // Calculate the millis since the previous time accounting for wrap around.
    uint32_t daysSince(const uint32_t previous) {
      return minutesSince(previous) / 60 /*mins/hour*/ / 24 /*hours/day*/;
    }

    // Calculate the millis since the previous time accounting for wrap around.
    static constexpr uint32_t daysDiff(const uint32_t previous, const uint32_t next) {
      // Millis rolls over after about 50 days, the unsigned subtraction accounts for this.
      return Clock::MillisToDays(Clock::millisDiff(previous, next));
    }

    // Calculate the millis since the previous time accounting for wrap around.
    static constexpr uint32_t MinutesDiff(const uint32_t previous, const uint32_t next) {
      // Millis rolls over after about 50 days, the unsigned subtraction accounts for this.
      return Clock::MillisToMinutes(Clock::millisDiff(previous, next));
    }
    // Calculate the millis since the previous time accounting for wrap around.
    static constexpr uint32_t secondsDiff(const uint32_t previous, const uint32_t next) {
      // Millis rolls over after about 50 days, the unsigned subtraction accounts for this.
      return Clock::millisDiff(previous,next) / 1000;
    }

    // Calculate the millis since the previous time accounting for wrap around.
    static constexpr uint32_t millisDiff(const uint32_t previous, const uint32_t next) {
      // Millis rolls over after about 50 days, the unsigned subtraction accounts for this.
      return next - previous;
    }

    // Converts Millis to Seconds for easier comparison.
    static constexpr uint32_t SecondsToMillis(const uint32_t& seconds) {
      return seconds * 1000;
    }
    // Converts Millis to Minutes for easier comparison.
    static constexpr uint32_t MinutesToMillis(const uint32_t& minutes) {
      return SecondsToMillis(minutes * 60);
    }
    // Convert Hours to Millis for easier comparison.
    static constexpr uint32_t HoursToMillis(const uint32_t& hours) {
      return MinutesToMillis(hours * 60);
    }
    static constexpr uint32_t DaysToMillis(const uint32_t& days) {
      return HoursToMillis(days * 24);
    }
    static constexpr uint32_t MillisToDays(const uint32_t ms) {
      return MillisToHours(ms) / 24;
    }
    static constexpr uint32_t MillisToHours(const uint32_t ms) {
      return MillisToMinutes(ms) / 60;
    }
    static constexpr uint32_t MillisToMinutes(const uint32_t ms) {
      return MillisToSeconds(ms) / 60;
    }
    static constexpr uint32_t MillisToSeconds(const uint32_t ms) {
      return ms / 1000;
    }
};

class Display : public Print {
  public:
    virtual void SetCursor(const int column, const int row) {
      UNUSED(column);
      UNUSED(row);
    };
};

class Sensor {
  public:    
    // Most sensors implement this.
    virtual void StartRequestAsync() = 0;

    // Returns temperature in fehrenheit.
    virtual float GetTemperature() = 0;

    // Returns relative humidity in %.
    virtual float GetHumidity() {
      return 0;
    };

    virtual float GetPressure() {
      return 0;
    };

    // Ensures the reading completed.
    virtual bool EndReading() {
      return true;
    };

    // Turns on or off the heater for a gas measurement.
    virtual void EnableGasHeater(const bool enable) {
      UNUSED(enable);
    };

    virtual uint32_t GetGasResistance() {
      return 0;
    };
};

class Settings;

class SettingsStorer {
  public:
    virtual void Write(const Settings& settings) = 0;
    virtual void Read(Settings *settings) = 0;
};

enum class RelayType {
  kHeat, kCool, kFan, kHumidifier, kMax
};

enum class RelayState {
  kOn, kOff
};

class Relays {
  public:
    virtual void Set(const RelayType relay, const RelayState state) = 0;
};

}
#endif
