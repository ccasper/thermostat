#ifndef SETTINGS_H_
#define SETTINGS_H_

#include <EEPROM.h>

#include "millis_since.h"

// 65536 is the largest representable value.
constexpr uint16_t VERSION = 34806;

enum class Mode { HEAT, COOL };

constexpr char daysOfTheWeek[7][3] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};

struct Setpoint {
  uint8_t hour = 0;
  uint8_t minute = 0;
  int temperature_x10 = 0;
};

struct Date {
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t day_of_week = 0;
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

  int fan_extend_mins = 5;
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

static void SetChangedAndPersist(Settings* settings) {
  settings->changed = true;
  EEPROM.put(0, settings->persisted);
}

static void SaveAllSettingsForDebug(Settings* settings) {
  EEPROM.put(sizeof(PersistedSettings), settings);
}

static void OutputAllSettingsForDebug() {
  Serial.println("Loading all settings for debug");
  // Read the settings from EEPROM.
  Settings settings;
  // Skip past the persisted settings to the stored debug settings.
  EEPROM.get(sizeof(PersistedSettings), settings);

  // If the versions mismatch, don't attempt to print the data.
  if (settings.persisted.version != VERSION) {
    Serial.println("Settings version mismatch");
  }
  // TODO Print out the fields.
}

static void SetChanged(Settings* settings) {
  settings->changed = true;
}

static Settings GetEepromOrDefaultSettings() {
  // Read the settings from EEPROM.
  Settings settings;
  EEPROM.get(0, settings.persisted);

  // If it don't look right, use the defaults.
  if (settings.persisted.version != VERSION) {
    Settings defaults;
    defaults.persisted.version = VERSION;
    // 7am-9pm -> 70.0° ; 9pm-7am -> 69°
    defaults.persisted.heat_setpoints[0].hour = 7;
    defaults.persisted.heat_setpoints[0].temperature_x10 = 700;
    defaults.persisted.heat_setpoints[1].hour = 21;
    defaults.persisted.heat_setpoints[1].temperature_x10 = 690;

    // 7am-9pm -> 77.0° ; 9pm-7am -> 72°
    defaults.persisted.cool_setpoints[0].hour = 7;
    defaults.persisted.cool_setpoints[0].temperature_x10 = 770;
    defaults.persisted.cool_setpoints[1].hour = 21;
    defaults.persisted.cool_setpoints[1].temperature_x10 = 720;

    // With a 1.1° tolerance.
    //
    // If the setpoint is 70°, heat stops at 70° and heating restarts at 68.9°, or cooling
    // restarts at 71.1°.
    defaults.persisted.tolerance_x10 = 11;

    // Write them to the eeprom.
    SetChangedAndPersist(&defaults);

    return defaults;
  }
  return settings;
}

static int GetSetpointTemp(const Settings& settings, const Date& date, Mode mode);

static bool IsOverrideTempActive(const Settings& settings) {
  return settings.override_temperature_x10 != 0;
}

static int GetOverrideTemp(const Settings& settings) {
  if (IsOverrideTempActive(settings)) {
    return max(400, min(999, settings.override_temperature_x10));
  }
  return settings.current_mean_temperature_x10;
}

// Overrides and temperature based on the current setpoint temperature the changed flag
// for faster updating.
static void SetOverrideTemp(int amount, Settings* settings, const Date& date) {
  // Bound the value to 40.0-99.9.
  settings->override_temperature_x10 =
    max(400, min(999, GetOverrideTemp(*settings) + amount));
  settings->override_temperature_started_ms = millis();
}

// This checks if we should be in a 5 minute lockout when switching from cooling to
// heating or heating to cooling.
static bool IsInLockoutMode(const Mode mode, const Event* events,
                            const uint8_t events_size) {
  constexpr uint32_t kLockoutMs = 5UL * 60UL * 1000UL;
  uint8_t index = 0;
  uint32_t millis_since = 0xFFFFFFFF;  // some big number

  // Find the newest event.
  for (int i = 0; i < events_size; ++i) {
    if (events[i].empty) {
      continue;
    }
    if (millisSince(events[i].start_time) < millis_since) {
      millis_since = millisSince(events[i].start_time);
      index = i;
    }
  }

  // Are there any events found?
  if (millis_since == 0xFFFFFFFF) {
    return false;
  }
  // If the current event is greater than we're out of the window.
  if (millis_since > kLockoutMs) {
    return false;
  }

  // Are we actively heating or cooling?
  if (mode == Mode::COOL && millis_since < kLockoutMs && events[index].heat) {
    return true;
  }
  if (mode == Mode::HEAT && millis_since < kLockoutMs && events[index].cool) {
    return true;
  }

  // Keep looking back until we're beyond the window..
  while (true) {
    index = (index - 1) % events_size;
    // The previous event is empty.
    if (events[index].empty) {
      return false;
    }

    // We use the started time of the newer event to know when it stopped.
    if (mode == Mode::COOL && events[index].heat) {
      return true;
    }

    if (mode == Mode::HEAT && events[index].cool) {
      return true;
    }

    // use the start time for the next previous event as it's stop time.
    millis_since = millisSince(events[index].start_time);

    if (millis_since > kLockoutMs) {
      return false;
    }
  }

  return false;
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
  return max(400, min(999, temp));
}

#endif  // SETTINGS_H_
