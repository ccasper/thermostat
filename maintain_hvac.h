#ifndef MAINTAIN_HVAC_H_
#define MAINTAIN_HVAC_H_

namespace thermostat {

constexpr uint32_t kManualTemperatureOverrideDuration = Clock::HoursToMillis(2);

class HvacController {
  // TODO: Move the Hvac logic below here.
};

// This is shown as the final character of the first row. This is the only character in
// the first row used by the menus.
void ShowStatus(Display *display, const Error error) {
  static Error s_error = Error::STATUS_OK;
  static uint8_t s_counter = 0;

  // An error gets latched on the screen until the thermostat is reset.
  if (error != Error::STATUS_OK) {
    s_error = error;
  }

  // We use the last character in the first row.
  display->SetCursor(15, 0);
  if (s_error == Error::STATUS_OK) {
    // Make an animation to allow a user to know the HVAC is still fully updating.
    s_counter = (s_counter + 1) % 4;
    if (s_counter == 0) {
     display->Write('/');
    }
    if (s_counter == 1) {
     display->Write('-');
    }
    if (s_counter == 2) {
     display->Write(byte(1));  // Prints the '\', the LCD's '\' is the Yen symbol.
    }
    if (s_counter == 3) {
     display->Write('|');
    }
  } else {
    // Show the error instead.
    switch (s_error) {
      case Error::BME_SENSOR_FAIL:
       display->Print('B');
        break;
      case Error::HEAT_AND_COOL:
       display->Print('b');
        break;
      default:
       display->Print('X');
        break;
    }
  }
}


// Window for calculating the mean temperature.
//
// This conditions the temperature signal ensuring fast fluctations don't
// affect the HVAC.
constexpr uint8_t kTemperatureWindowSize = 8;
int temperature_window[kTemperatureWindowSize] = {0};

// Maintain a sum of the window values to make calculating the mean fast.
int temperature_sum = 0;

// Index to fill in next. Starts at zero so the mean can be calculated accurately during
// initial filling.
uint8_t temperature_index = 0;

// Flag gets updated to true once temperature_index reaches temperature_SIZE. This ensures
// the mean calculation is accurate.
bool temperature_filled = false;

// Called to check and update the HVAC, humidifier and fan actions. This needs to be
// performed once every 1-5 seconds.
uint8_t hvac_run_counter = 0;
bool hvac_gas_measurement_on = false;

// Tracks when MaintainHvac() was last allowed to run to completion allowing it to only
// run at a set interval.
uint32_t last_hvac_update = 0;

// This routine is always run periodically, even when blocking in menus. This ensures the
// HVAC controls are continuously maintained.
//
// For example, when we call into a menu, the menu blocks, and since the menu always
// blocks using WaitForButtonPress, we regularily update this function.
Error MaintainHvac(Settings *settings, Clock *clock, Display *display, Relays *relays, FanController *fan, Sensor *bme_sensor, Sensor *dallas_sensor) {
  const uint32_t now = clock->Millis();
  // Run only once every 2.5 seconds.
  if (!settings->changed && Clock::millisDiff(last_hvac_update, now) <= 2500) {
    return Error::STATUS_NONE;
  }
  last_hvac_update = millis();

  // Reset the settings changed flag since we're going to update the HVAC with the values
  // in the settings object.
  settings->changed = 0;

  ++hvac_run_counter;

  // Allow the manual temperature override to only apply for 2 hours. We clear the
  // override temperature to indicate no override is being applied.
  if (settings->override_temperature_x10 != 0 &&
      Clock::millisDiff(settings->override_temperature_started_ms, now) >
      kManualTemperatureOverrideDuration) {
    settings->override_temperature_x10 = 0;
  }

  // Scale by 10 and clip the temperature to 99.9°.
  const int temperature =
    cmin(dallas_sensor->GetTemperature() * 10, 999);

  // Store the new value in the settings.
  settings->current_temperature_x10 = temperature;

  if (!bme_sensor->EndReading()) {
    return Error::BME_SENSOR_FAIL;
  }

  Serial.print("MaintainHvac Temperature = ");
  const int bme_temperature = cmin(bme_sensor->GetTemperature() * 10.0, 999);
  settings->current_bme_temperature_x10 = bme_temperature;
  Serial.print(bme_temperature);
  Serial.println(" °F");
  Serial.print(temperature);
  Serial.println(" °F");
  Serial.print("Pressure = ");
  Serial.print(bme_sensor->GetPressure() / 100.0);
  Serial.println(" hPa");
  Serial.print("Humidity = ");
  Serial.print(bme_sensor->GetHumidity());
  Serial.println(" %");
  if (hvac_gas_measurement_on) {
    Serial.print("Gas = ");
    Serial.print(bme_sensor->GetGasResistance() / 1000.0);
    Serial.println(" KOhms");
    settings->air_quality_score = CalculateIaqScore(bme_sensor->GetHumidity(), bme_sensor->GetGasResistance());
    Serial.print("IAQ: ");
    Serial.print(settings->air_quality_score);
    Serial.print("% ");
  }

  // Turn off the gas heater/sensor if it was previously enabled.
  if (hvac_gas_measurement_on) {
    bme_sensor->EnableGasHeater(false);
    hvac_gas_measurement_on = false;
  }

  // Run the gas sensor periodically (every 20s). We do this periodically based on the
  // modulo of the hvac_run_counter.
  if (hvac_run_counter % 8 == 7) {
    bme_sensor->EnableGasHeater(true);
    hvac_gas_measurement_on = true;
  }

  // Kick off the next asynchronous readings.
  bme_sensor->StartRequestAsync();
  dallas_sensor->StartRequestAsync();

  // Only subtract past values that were previously added to the window.
  if (temperature_filled) {
    temperature_sum -= temperature_window[temperature_index];
  }
  temperature_window[temperature_index] = settings->current_temperature_x10;
  temperature_sum += temperature_window[temperature_index];
  if (temperature_index == kTemperatureWindowSize - 1) {
    temperature_filled = true;
  }
  temperature_index = (temperature_index + 1) % kTemperatureWindowSize;

  const uint8_t temperature_counts =
    temperature_filled ? kTemperatureWindowSize : temperature_index;
  const int temperature_mean = temperature_sum / temperature_counts;
  settings->current_mean_temperature_x10 = temperature_mean;

  // set the cursor to column 0, line 1
  // (note: line 1 is the second row, since counting begins with 0):
  display->SetCursor(0, 0);

  // Display the mean temperature field.
 display->Print(static_cast<int>(temperature_mean) / 10);
 display->Print(".");
 display->Print(static_cast<int>(temperature_mean) % 10);
 display->Write(byte(0));  // Print the custom '°' symbol.
 display->Print(" ");

  // Display the relative humidity field.
  settings->current_humidity = bme_sensor->GetHumidity();
 display->Print(static_cast<int>(bme_sensor->GetHumidity()));  // Clips to 99.9° indoor.
 display->Print(".");
 display->Print(static_cast<int>(bme_sensor->GetHumidity() * 10) % 10);  // Clips to 99.9° indoor.
 display->Print("% ");

  // Display 'o' if the manual temperature override is in effect.
  if (IsOverrideTempActive(*settings)) {
   display->Write('o');
  } else {
   display->Write(' ');
  }

  // Update the fan control based on the current settings.
  fan->Maintain(settings, now);
  if (settings->fan_running) {
   display->Write('F');
    relays->Set(RelayType::kFan, RelayState::kOn);
  } else {
   display->Write('_');
    relays->Set(RelayType::kFan, RelayState::kOff);
  }

  const int setpoint_temperature_x10 =
    GetSetpointTemp(*settings, clock->Now(), Mode::HEAT);
  Serial.println("Temp: " + String(settings->current_temperature_x10) +
                 " Mean: " + String(temperature_mean) +
                 " heat enabled:" + String(settings->persisted.heat_enabled) +
                 " Tol: " + String(settings->persisted.tolerance_x10));

  bool lockout_heat = false;
  bool lockout_cool = false;

  // If heating is on, keep the temperature above the setpoint.
  // If cooling is on, keep the temperature below the setpont.
  // cool to the setpoint. If off, drift to the setpoint plus the tolerance.
  // Should we enable/disable heating mode?
  if (settings->heat_running) {
    Serial.println("Heat running");

    if (temperature_mean >= setpoint_temperature_x10 + settings->persisted.tolerance_x10) {
      Serial.println("Reached setpoint");
      // Stop heating, we've reached the tolerance above the heating setpoint.
      settings->heat_running = false;
    }
  } else {
    Serial.println("Heat not running");
    if (settings->persisted.heat_enabled) {
      Serial.println("Settings heat true");
    }
    if (temperature_mean > setpoint_temperature_x10) {
      Serial.println("Temp not met " + String(temperature_mean) +
                     "<=" + String(setpoint_temperature_x10) + "-" +
                     String(settings->persisted.tolerance_x10));
    }
    // Assuming Set point is 70 and current room temp is 68, we should enable heating if
    // tolerance is 2.
    if (settings->persisted.heat_enabled && temperature_mean < setpoint_temperature_x10) {
      // Start heating, we've reached our heating setpoint.
      if (!IsInLockoutMode(Mode::HEAT, settings->events, 10, now)) {
        settings->heat_running = true;
      } else {
        lockout_heat = true;
      }
    }
  }

  // Prevent/Stop heating, we're disabled.
  if (!settings->persisted.heat_enabled) {
    settings->heat_running = false;
    lockout_heat = false;
  }

  // If the setpoint is 75°, we attempt to keep cooler than this temperature at all times.
  //
  // Therefore when the thermostat reaches 75° when set at 75°, the user can expect
  // cooling to turn on.
  const int cool_temperature_x10 = GetSetpointTemp(*settings, clock->Now(), Mode::COOL);
  // Should we enable/disable cooling mode?
  if (settings->cool_running) {
    if (temperature_mean <= cool_temperature_x10 - settings->persisted.tolerance_x10) {
      // Stop cooling, we've reached the our lower tolerance limit.
      settings->cool_running = false;
    }
  } else {
    // Assuming a set point of 70, we should start cooling at 72 with a tolerance set
    // of 2.
    if (temperature_mean > cool_temperature_x10) {
      if (!IsInLockoutMode(Mode::COOL, settings->events, 10, now)) {
        // Start cooling, we've reached the cooling setpoint.
        settings->cool_running = true;
      } else {
        lockout_cool = true;
      }
    }
  }

  // Stop/prevent cooling, we're disabled.
  if (!settings->persisted.cool_enabled) {
    settings->cool_running = false;
    lockout_cool = false;
  }
  Error status = Error::STATUS_OK;

  // This should never happen, but disable both if both are enabled.
  if (settings->heat_running && settings->cool_running) {
    status = Error::HEAT_AND_COOL;
    settings->heat_running = false;
    settings->cool_running = false;
  }

  if (settings->heat_running && settings->current_humidity < settings->persisted.humidity) {
    // Allow humidifier only when heating. During hot A/C days, we want as much humidity
    // removed as possible.
    relays->Set(RelayType::kHumidifier, RelayState::kOn);
  } else {
    relays->Set(RelayType::kHumidifier, RelayState::kOff);
  }

  // Configure the relays correctly, and also display either:
  // 'c'= Cooling wants to run but heating ran too recently so it's in lockout mode yet.
  // 'C'= Cooling is turned on.
  // 'h'= Heating wants to run but cooling ran too recently so it's in lockout mode yet.
  // 'H'= Heating is turned on.
  // '_'= The thermostat is not actively requesting heating or cooling.
  if (settings->heat_running) {
    relays->Set(RelayType::kHeat, RelayState::kOn);
    relays->Set(RelayType::kCool, RelayState::kOff);
   display->Write('H');
  } else if (settings->cool_running) {
    relays->Set(RelayType::kHeat, RelayState::kOff);
    relays->Set(RelayType::kCool, RelayState::kOn);
   display->Write('C');
  } else {
    relays->Set(RelayType::kHeat, RelayState::kOff);
    relays->Set(RelayType::kCool, RelayState::kOff);
    if (lockout_heat) {
     display->Write('h');
    } else if (lockout_cool) {
     display->Write('c');
    } else {
     display->Write('_');
    }
  }

  // Maintain the historical events list.
  AddOrUpdateEvent(millis(), settings);

  // The very last character in the first row is for an error flag for debugging.
  return status;
}

}
#endif // MAINTAIN_HVAC_H_
