#ifndef SETTINGS_STORER_H_
#define SETTINGS_STORER_H_

#include "interfaces.h"

#include <EEPROM.h>

namespace thermostat {

class EepromSettingsStorer : public SettingsStorer {
  public:
    void Write(const Settings& settings) override {
      EEPROM.put(0, settings.persisted);
    };
    void Read(Settings *settings) override {
      EEPROM.get(0, settings->persisted);
    };
    //    void WriteDebug(const Settings& settings, const uint32_t millis) {
    //      EEPROM.put(sizeof(PersistedSettings), settings);
    //      EEPROM.put(sizeof(PersistedSettings) + sizeof(Settings), millis);
    //    };
    //    uint32_t ReadDebug(Settings *settings) {
    //      EEPROM.get(sizeof(PersistedSettings), settings);
    //      uint32_t millis = 0;
    //      EEPROM.get(sizeof(PersistedSettings)+ sizeof(Settings), &millis);
    //      return millis;
    //    };
};

static Settings GetEepromOrDefaultSettings(SettingsStorer* storer) {
  // Read the settings from EEPROM.
  Settings settings;
  storer->Read(&settings);

  // If it don't look right, use the defaults.
  if (settings.persisted.version != VERSION) {
    Settings defaults;
    defaults.persisted.version = VERSION;
    // 7am-9pm -> 70.0° ; 9pm-7am -> 69°
    defaults.persisted.heat_setpoints[0].hour = 7;
    defaults.persisted.heat_setpoints[0].temperature_x10 = 695;
    defaults.persisted.heat_setpoints[1].hour = 21;
    defaults.persisted.heat_setpoints[1].temperature_x10 = 685;

    // 7am-9pm -> 77.0° ; 9pm-7am -> 72°
    defaults.persisted.cool_setpoints[0].hour = 7;
    defaults.persisted.cool_setpoints[0].temperature_x10 = 770;
    defaults.persisted.cool_setpoints[1].hour = 21;
    defaults.persisted.cool_setpoints[1].temperature_x10 = 750;

    // heating/cooling enabled defaults.
    defaults.persisted.cool_enabled = false;
    defaults.persisted.heat_enabled = true;

    // With a 1.2° tolerance.
    //
    // If the setpoint is 70.0° (1° tolerance), heat starts at 69.0 and stops at 70.0°. When cooling
    // starts at 71.0°.
    defaults.persisted.tolerance_x10 = 11;

    defaults.persisted.fan_extend_mins = 0;

    // Recommend: minimum 15% duty cycle (30 mins) every 3 hours.
    defaults.persisted.fan_on_min_period = 180;
    defaults.persisted.fan_on_duty = 0; // 0 (OFF) - 99%

    // Write them to the eeprom.
    SetChangedAndPersist(&defaults, storer);

    return defaults;
  }
  return settings;
};

} // namespace thermostat
#endif // SETTINGS_STORER_H_
