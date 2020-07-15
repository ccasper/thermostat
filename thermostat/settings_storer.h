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
};

} // namespace thermostat
#endif // SETTINGS_STORER_H_
