#ifndef INTERFACES_H_
#define INTERFACES_H_

#include <LiquidCrystal.h>
#include <Adafruit_BME680.h>

// This class defines interfaces that are not hardware dependent.
//
// This allows unit testing of non-hardware related logic.
namespace thermostat {


enum class Error {
  STATUS_NONE,
  STATUS_OK,
  BME_SENSOR_FAIL,
  HEAT_AND_COOL,
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

    virtual uint32_t Millis() = 0;

    // Calculate the millis since the previous time accounting for wrap around.
    uint32_t millisSince(const uint32_t previous) {
      return millisDiff(previous, Millis());
    }

    // Calculate the millis since the previous time accounting for wrap around.
    int minutesSince(const uint32_t previous) {
      return millisDiff(previous, Millis()) / 1000 /*sec*/ / 60 /*minutes*/;
    }

    // Calculate the millis since the previous time accounting for wrap around.
    int daysSince(const uint32_t previous) {
      return minutesSince(previous) / 60 /*hours*/ / 24 /*hours*/;
    }

    // Calculate the millis since the previous time accounting for wrap around.
    static uint32_t daysDiff(const uint32_t previous, const uint32_t next) {
      // Millis rolls over after about 50 days, the unsigned subtraction accounts for this.
      return Clock::MillisToDays(Clock::millisDiff(previous, next));
    }

    // Calculate the millis since the previous time accounting for wrap around.
    static uint32_t MinutesDiff(const uint32_t previous, const uint32_t next) {
      // Millis rolls over after about 50 days, the unsigned subtraction accounts for this.
      return Clock::MillisToMinutes(Clock::millisDiff(previous, next));
    }

    // Calculate the millis since the previous time accounting for wrap around.
    static uint32_t millisDiff(const uint32_t previous, const uint32_t next) {
      // Millis rolls over after about 50 days, the unsigned subtraction accounts for this.
      return next - previous;
    }

    // Converts Millis to Seconds for easier comparison.
    static constexpr uint32_t SecondsToMillis(const uint32_t& seconds) {
      return seconds * 1000;
    }

    // Converts Millis to Minutes for easier comparison.
    static constexpr uint32_t MinutesToMillis(const uint32_t& minutes) {
      return minutes * 60 * 1000;
    }

    // Convert Hours to Millis for easier comparison.
    static constexpr uint32_t HoursToMillis(const uint32_t& hours) {
      return hours * 60 * 60 * 1000;
    }
    static constexpr uint32_t DaysToMillis(const uint32_t& days) {
      return days * 1000 * 60 * 60 * 24;
    }
    static constexpr uint32_t MillisToMinutes(const uint32_t ms) {
      return ms / 1000 / 60;
    }
    static constexpr uint32_t MillisToDays(const uint32_t ms) {
      return ms / 1000 / 60 / 60 / 24;
    }
};

class Display {
  public:
    virtual void Print(const float value) {};
    virtual void Print(uint16_t value) {};
    virtual void Print(int value) {};
    virtual void Print(const char* const str) {};
    virtual void Write(uint8_t ch) {};
    virtual void SetCursor(const int column, const int row) {
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
    virtual void EnableGasHeater(const bool enable) {};

    virtual uint32_t GetGasResistance() {
      return 0;
    };
};

enum class RelayType {
  kHeat, kCool, kFan, kHumidifier
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
