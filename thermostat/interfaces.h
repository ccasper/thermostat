#ifndef INTERFACES_H_
#define INTERFACES_H_

#define UNUSED(x) (void)(x)

#include "print.h"

// This class defines interfaces that are hardware agnostic.
//
// This allows unit testing to use mock like interfaces to test much of the logic.
namespace thermostat {

// Forward declare settings.
class Settings;

enum class Status {kOk, kSkipped,
                   kBmeSensorFail,
                   kHeatAndCool,
                   kMenuDisplayArg, kError
                  };

class ThermostatTask {
  public:
    // This method is always run periodically, even when blocking in menus. This ensures the
    // HVAC controls are continuously maintained.
    //
    // This updates the HVAC, relays, top LCD row and fan actions. This needs to be
    // performed once every ~1-5 seconds.
    //
    // For example, when we call into a menu, the menu blocks, and since the menu always
    virtual Status RunOnce(Settings* settings) = 0;
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
      return MillisDiff(previous, Millis());
    }

    // Calculate the millis since the previous time accounting for wrap around.
    uint32_t secondsSince(const uint32_t previous) {
      return MillisDiff(previous, Millis()) / 1000 /*ms/sec*/;
    }

    // Calculate the millis since the previous time accounting for wrap around.
    uint32_t minutesSince(const uint32_t previous) {
      return MillisDiff(previous, Millis()) / 1000 /*ms/sec*/ / 60 /*sec/min*/;
    }
    // Calculate the millis since the previous time accounting for wrap around.
    uint32_t hoursSince(const uint32_t previous) {
      return minutesSince(previous) / 60 /*mins/hour*/;
    }
    // Calculate the millis since the previous time accounting for wrap around.
    uint32_t daysSince(const uint32_t previous) {
      return minutesSince(previous) / 60 /*mins/hour*/ / 24 /*hours/day*/;
    }

    static constexpr uint32_t HoursToSeconds(const uint32_t hours) {
      return hours * 60 * 60;
    }

    static constexpr uint32_t MinutesToSeconds(const uint32_t minutes) {
      return minutes * 60;
    }

    // Calculate the millis since the previous time accounting for wrap around.
    static constexpr uint32_t daysDiff(const uint32_t previous, const uint32_t next) {
      // Millis rolls over after about 50 days, the unsigned subtraction accounts for this.
      return Clock::MillisToDays(Clock::MillisDiff(previous, next));
    }

    // Calculate the millis since the previous time accounting for wrap around.
    static constexpr uint32_t MinutesDiff(const uint32_t previous, const uint32_t next) {
      // Millis rolls over after about 50 days, the unsigned subtraction accounts for this.
      return Clock::MillisToMinutes(Clock::MillisDiff(previous, next));
    }
    // Calculate the millis since the previous time accounting for wrap around.
    static constexpr uint32_t SecondsDiff(const uint32_t previous, const uint32_t next) {
      // Millis rolls over after about 50 days, the unsigned subtraction accounts for this.
      return Clock::MillisDiff(previous, next) / 1000;
    }

    // Calculate the millis since the previous time accounting for wrap around.
    static constexpr uint32_t MillisDiff(const uint32_t previous, const uint32_t next) {
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

// Subtract time a from time b. If a is more recent than b, the
// result is positive.
//
// This assumes the difference between 'a' and 'b' is not more than
// half of the max millis value that causes wrap around.
static int32_t MillisSubtract(uint32_t a, uint32_t b) {
  // if a - b are more than half the max wrap value, then we
  // normalize them from the wrap around.
  const uint32_t half_wrap = static_cast<uint32_t>(-1) / 2;
  if (a - b > half_wrap) {
    // If a wrapped around and is a smaller value.
    //
    // This is an optimization instead of normalizing both a and b
    // by adding UINT32_MAX/2 and then doing the subtraction.
    return (b - a) * -1;
  }
  return a - b;
}


class Display : public Print {
  public:
    virtual void SetCursor(const int column, const int row) {
      UNUSED(column);
      UNUSED(row);
    };
};

class Sensor {
  public:
    virtual void SetUp() {};

    // Most sensors implement this.
    virtual void StartRequestAsync() {};

    // Returns temperature in fehrenheit.
    virtual float GetTemperature() {
      return 0;
    };

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


class SettingsStorer {
  public:
    virtual void Write(const Settings& settings) = 0;
    virtual void Read(Settings *settings) = 0;
};

enum class RelayType {
  kHeat, kCool, kFan, kHeatHigh, kMax
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
