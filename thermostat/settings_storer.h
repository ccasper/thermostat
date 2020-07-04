#ifndef SETTINGS_STORER_H_
#define SETTINGS_STORER_H_

#include "interfaces.h"

#include <EEPROM.h>

namespace thermostat {
  
class EepromSettingsStorer : public SettingsStorer {
  public:
  void write(const Settings& settings) override {
    EEPROM.put(0, settings.persisted);
  };
  void Read(Settings *settings) override {
    EEPROM.get(0, settings->persisted);
  };
};

static void SetChangedAndPersist(Settings *settings, SettingsStorer *writer) {
  settings->changed = true;
  writer->write(*settings);
};

static void SetChanged(Settings* settings) {
  settings->changed = true;
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
    SetChangedAndPersist(&defaults, storer);

    return defaults;
  }
  return settings;
};

} // namespace thermostat
#endif // SETTINGS_STORER_H_
