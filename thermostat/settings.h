#ifndef SETTINGS_H_
#define SETTINGS_H_
// This header implements the object and helper functions to store user settings and HVAC control and status information.

#include "interfaces.h"
#include "comparison.h"


namespace thermostat {

// 65536 is the largest representable value.
constexpr uint16_t VERSION = 34807;

enum class Mode { HEAT, COOL };

constexpr char daysOfTheWeek[7][3] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};

struct Setpoint {
  uint8_t hour = 0;
  uint8_t minute = 0;
  int temperature_x10 = 0;
};


struct PersistedSettings {
  PersistedSettings()
    : heat_enabled(true), cool_enabled(true), fan_always_on(false), humidity(30) {};
  uint16_t version;

  // Is heating enabled.
  uint8_t heat_enabled : 1;  // one bit.

  // Is cooling enabled.
  uint8_t cool_enabled : 1;  // one bit.

  // User configured fan setting.
  uint8_t fan_always_on : 1;

  // RH to set the humidifier to. 0 = Off, 100%=On.
  uint8_t humidity : 7;

  Setpoint heat_setpoints[2];
  Setpoint cool_setpoints[2];
  int tolerance_x10 = 15;  // 1.5 degrees above and below.

  uint16_t fan_extend_mins = 5;
  uint16_t fan_on_min_period = 60;
  uint8_t fan_on_duty = 25; // 0 (OFF) - 99%
};

// List of events for temperature change calculations.
class Event {
  public:
    Event() : empty(true) {}

    uint8_t empty : 1;
    uint8_t heat : 1;
    uint8_t cool : 1;
    // The temperature when the event occurred.
    int16_t temperature_x10 : 14;
    uint32_t start_time;
    uint32_t temperature_x10_overshoot : 3;
};

constexpr uint8_t EVENT_SIZE = 10;

struct Settings {
  // The settings have had a recently changed field.
  //
  // Use SetChanged() to update EEPROM data and this bit.
  uint8_t changed : 1;  // one bit.

  // Is cooling actively running.
  bool cool_running = 0;

  // Is heating actively running.
  bool heat_running = 0;

  // Is fan actively running.
  bool fan_running = 0;

  // Snapshot of current humidity.
  uint8_t current_humidity = 0;

  // Snapshot of the current temperature.
  int current_temperature_x10 = 0;
  int current_bme_temperature_x10 = 0;
  int current_mean_temperature_x10 = 0;

  int override_temperature_x10 = 0;
  uint32_t override_temperature_started_ms;

  uint16_t average_run_seconds;
  uint16_t average_off_seconds;

  float air_quality_score;

  uint8_t event_size = EVENT_SIZE;
  uint8_t event_index = 0;

  // Events initialize to field empty = true.
  Event events[EVENT_SIZE] = {};

  int CurrentEventIndex() const {
    if (events[event_index].empty) {
      return -1;
    }
    return event_index;
  }
  int PrevEventIndex(int index) {
    if (index == -1) {
      return -1;
    }
    uint8_t unsigned_index = index;
    if (events[unsigned_index].empty) {
      return -1;
    }
    if (events[(unsigned_index - 1) % EVENT_SIZE].empty) {
      return -1;
    }
    return (unsigned_index - 1) % EVENT_SIZE;
  }

  PersistedSettings persisted;
};


static int GetSetpointTemp(const Settings& settings, const Date& date, Mode mode);

static bool IsOverrideTempActive(const Settings& settings) {
  return settings.override_temperature_x10 != 0;
}

static int GetOverrideTemp(const Settings& settings) {
  if (IsOverrideTempActive(settings)) {
    return cmax(400, cmin(999, settings.override_temperature_x10));
  }
  return settings.current_mean_temperature_x10;
}

// Overrides and temperature based on the current setpoint temperature the changed flag
// for faster updating.
static void SetOverrideTemp(int amount, Settings* settings, const uint32_t now) {
  // Bound the value to 40.0-99.9.
  settings->override_temperature_x10 =
    cmax(400, cmin(999, GetOverrideTemp(*settings) + amount));
  settings->override_temperature_started_ms = now;
}


// Gets the current setpoint temperature based on current date and any set override.
static int GetSetpointTemp(const Settings& settings, const Date& date, const Mode mode) {
  if (IsOverrideTempActive(settings)) {
    return GetOverrideTemp(settings);
  }

  // TODO: Loop through the setpoints looking for a match, otherwise use the first one
  // set. Find the setpoint time closest, but smaller than the current date.
  uint16_t clock_minutes = date.hour * 60 + date.minute;

  uint16_t smallest_diff = 24 * 60;
  int temp = 0;

  for (const Setpoint& setpoint : (mode == Mode::HEAT)
       ? settings.persisted.heat_setpoints
       : settings.persisted.cool_setpoints) {
    uint16_t minutes = setpoint.hour * 60 + setpoint.minute;
    // clock=18:00, set1=19:00, set2=23:00
    //   diff1= 19-18 = 1
    //   diff2=
    const uint16_t diff = (clock_minutes > minutes)
                          ? clock_minutes - minutes
                          : ((24 * 60) - minutes) + clock_minutes;
    if (diff < smallest_diff) {
      smallest_diff = diff;
      temp = setpoint.temperature_x10;
    }
  }
  return cmax(400, cmin(999, temp));
}

}
#endif  // SETTINGS_H_