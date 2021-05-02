// This is the core logic that makes the thermostat's HVAC system function.
//
// Ideally these classes would all be in individual .cc/.h files, however this
// makes it hard to use in the arduino IDE.
//
// These classes manage the top row of the LCD Display, relays, sensor readings, fan, history data.
#ifndef MAINTAIN_HVAC_H_
#define MAINTAIN_HVAC_H_

#include "comparison.h"
#include "interfaces.h"
#include "calculate_iaq.h"
#include "events.h"

namespace thermostat {
constexpr uint32_t kManualTemperatureOverrideDuration = Clock::HoursToMillis(2);

constexpr uint8_t kTemperatureWindowSize = 8;

constexpr int kRunEveryMillis = 1500;

// Error status latching value.
static Status g_status = Status::kOk;

class WrapperThermostatTask : public ThermostatTask {
  public:
    Status RunOnce(Settings* settings) override {
      return Status::kOk;
    }
};

// ThermostatTask decorator layer that performs HV/AC control management.
class HvacControllerThermostatTask final : public ThermostatTask {
  public:
    explicit HvacControllerThermostatTask(Clock* const clock, Print* const print, ThermostatTask* const wrapped) :
      clock_(clock),
      print_(print),
      wrapped_(wrapped) {};

    HvacMode DetermineCoolMode(const Settings& settings, bool* within_tolerance) {
      HvacMode mode =
          (settings.hvac == HvacMode::COOL_LOCKOUT) ?
              HvacMode::IDLE : settings.hvac;
 
      const bool in_cool_mode = (mode == HvacMode::COOL);

      if (!settings.persisted.cool_enabled) {
        if (in_cool_mode) {
          return HvacMode::IDLE;
        }
        return mode;
      }

      const int setpoint_x10 = GetSetpointTemp(settings, clock_->Now(), HvacMode::COOL);

      *within_tolerance = settings.current_mean_temperature_x10 <= setpoint_x10;

      // Should we disable cooling mode?
      if (in_cool_mode) {
        if (settings.current_mean_temperature_x10 <= setpoint_x10 - settings.persisted.tolerance_x10) {
          // Stop cooling, we've reached the our lower tolerance limit.
          return HvacMode::IDLE;
        }
        return mode;
      } else {

        // Should we start cooling?
        //
        // Assuming a set point of 70, we should start cooling at 72 with a tolerance set
        // of 2.
        if (!*within_tolerance) {
          if (IsInLockoutMode(HvacMode::COOL, settings.events, clock_->Millis())) {
            return HvacMode::COOL_LOCKOUT;
          }

          // Start cooling, we've reached the cooling setpoint.
          return HvacMode::COOL;
        }
      }
      return mode;
    }

    HvacMode DetermineHeatMode(const Settings& settings, bool* within_tolerance) {
      HvacMode mode =
          (settings.hvac == HvacMode::HEAT_LOCKOUT) ?
              HvacMode::IDLE : settings.hvac;
            
      const bool in_heat_mode = (mode == HvacMode::HEAT);

      if (!settings.persisted.heat_enabled) {
        if (in_heat_mode) {
          return HvacMode::IDLE;
        }
        return mode;
      }

      const int setpoint_x10 =
        GetSetpointTemp(settings, clock_->Now(), HvacMode::HEAT);
//   for (const Setpoint& setpoint : settings.persisted.heat_setpoints) {
//      print_->print("HX: ");
//      print_->print(setpoint.temperature_x10);
//      print_->print('\n');
//  }
//       
//      print_->print("Heat override: ");
//      print_->print(settings.override_temperature_x10);
//      print_->print('\n');
//      print_->print("Heat Setpoint: ");
//      print_->print(setpoint_x10);
//      print_->print('\n');

      // If heating is on, keep the temperature above the setpoint.
      // If cooling is on, keep the temperature below the setpoint.
      // cool to the setpoint. If off, drift to the setpoint plus the tolerance.
      //
      // Temperature behavior chart
      //
      // Temp| Action
      // ------------
      // 74.0|-- Cool on (> setpoint)
      //     |
      // 72.9|-- Cool stop based on tolerance (1.1)
      //     |
      //     | [HVAC idle]
      //     |
      // 71.1|-- Heat stop based on tolerance (1.1)
      //     |
      // 70.0|-- Heat on (< setpoint)

      *within_tolerance = settings.current_mean_temperature_x10 >= setpoint_x10;

      // Should we turn off heating mode.
      if (in_heat_mode) {
        // Stop heating, we've reached the tolerance above the heating setpoint.
        if (settings.current_mean_temperature_x10 >= setpoint_x10 + settings.persisted.tolerance_x10) {
          return HvacMode::IDLE;
        }
        return mode;
      }
	  
      // Turn on the heat if we've reached our heating setpoint.
      //
      // Assuming Setpoint is 70 and current room temp is 69.9, we should enable heating.
      if (!*within_tolerance) {
        // Lockout mode prevents quick switches between heat and cool
        if (IsInLockoutMode(HvacMode::HEAT, settings.events, clock_->Millis())) {
          return HvacMode::HEAT_LOCKOUT;
        } else {
          return HvacMode::HEAT;
        }
      }
      return mode;
    }

    Status RunOnce(Settings* settings) override {
      Status status = wrapped_->RunOnce(settings);
      if (status != Status::kOk) {
        return status;
      }

      // Reset the settings changed flag since we're going to update the HVAC with the values
      // in the settings object.
      settings->changed = 0;

      ++hvac_run_counter_;

      // Allow the manual temperature override to only apply for 2 hours. We clear the
      // override temperature to indicate no override is being applied.
      if (settings->override_temperature_x10 != 0 &&
          Clock::MillisDiff(settings->override_temperature_started_ms, settings->now) >
          kManualTemperatureOverrideDuration) {
        settings->override_temperature_x10 = 0;
      }

      bool within_tolerance = true;
      settings->hvac = DetermineHeatMode(*settings, &within_tolerance);

      settings->within_tolerance = within_tolerance;

      print_->print("Curr: ");
      print_->print(settings->current_temperature_x10);
      print_->print(" Mean: " );
      print_->print(settings->current_mean_temperature_x10);
      print_->print(" heat enabled:");
      print_->print(settings->persisted.heat_enabled ? 'T' : 'F');
      print_->print(" Tol: ");
      print_->print(settings->persisted.tolerance_x10);
      print_->print('\n');

      // Heating takes precedence over cooling.
      if (settings->hvac == HvacMode::HEAT || settings->hvac == HvacMode::HEAT_LOCKOUT) {
        return status;
      }

      settings->hvac = DetermineCoolMode(*settings, &within_tolerance);
      if (!within_tolerance) { settings->within_tolerance = false; }

      return status;
    }

  private:
    uint8_t hvac_run_counter_ = 0;

    Clock* const clock_;
    Print* const print_;

    ThermostatTask* const wrapped_;
};

// ThermostatTask decorator layer that increases to High Heat after 30 minutes.
class HeatAdvancingThermostatTask final : public ThermostatTask {
  public:
    explicit HeatAdvancingThermostatTask(ThermostatTask* const wrapped) :
      wrapped_(wrapped) {};

    Status RunOnce(Settings* settings) override {
      Status status = wrapped_->RunOnce(settings);

      // If this is the first run from boot, initialize.
      if (settings->first_run == true) {
        hvac_start_time_ = settings->now;
        settings->heat_high = false;
      }

      // Clear the settings when not heating.
      if (settings->hvac != HvacMode::HEAT) {
        // Keep the start_time tracking the last non-heating mode.
        hvac_start_time_ = settings->now;
        settings->heat_high = false;
        return status;
      }

      // If we've been heating for 30 minutes and haven't gotten above the low, move to high mode.
      //
      // The high heat flag needs to be sticky until heat turns off, which is why we only clear it
      // if not in HEAT mode.
      //
      // This ensures we run at low heat most of the time, except when low really can't keep us within
      // our tolerance.
      if (Clock::MinutesDiff(hvac_start_time_, settings->now) > 10 && !settings->within_tolerance) {
        settings->heat_high = true;
      }

      return status;
    }

  private:
    uint32_t hvac_start_time_ = 0;
    ThermostatTask* const wrapped_;
};

// ThermostatTask decorator layer that performs HV/AC control management.
class SensorUpdatingThermostatTask final : public ThermostatTask {
  public:
    explicit SensorUpdatingThermostatTask(Clock* const clock, Sensor* const bme_sensor, Sensor* const dallas_sensor, Print* const print, ThermostatTask* const wrapped) :
      clock_(clock),
      print_(print),
      bme_sensor_(bme_sensor),
      dallas_sensor_(dallas_sensor),
      wrapped_(wrapped) {};

    Status RunOnce(Settings* settings) override {
      Status status = wrapped_->RunOnce(settings);
      if (status != Status::kOk) {
        return status;
      }

      if (!sensor_started_) {
        // Kick off the next asynchronous readings.
        bme_sensor_->StartRequestAsync();
        dallas_sensor_->StartRequestAsync();
        sensor_started_ = true;
        ;

        // Give the sensor time to get a value.
        for (const uint32_t now = clock_->Millis(); clock_->millisSince(now) < 100;) {
          // Do nothing
        }
      }

      // Scale by 10 and clip the temperature to 99.9°.
      const int temperature =
        cmin(dallas_sensor_->GetTemperature() * 10, 999);

      // Store the new value in the settings.
      settings->current_temperature_x10 = temperature;

      if (!bme_sensor_->EndReading()) {
        return Status::kBmeSensorFail;
      }
      settings->current_humidity = bme_sensor_->GetHumidity();

      print_->print("MaintainHvac");
      print_->print(" BME = ");
      const int bme_temperature = cmin(bme_sensor_->GetTemperature() * 10.0, 999);
      settings->current_bme_temperature_x10 = bme_temperature;
      print_->print(bme_temperature);
      print_->print(" °F\r\n");
      print_->print(" Dallas = ");
      print_->print(temperature);
      print_->print(" °F\r\n");
      print_->print(" Pressure = ");
      print_->print(bme_sensor_->GetPressure() / 100.0);
      print_->print(" hPa\r\n");
      print_->print(" Humidity = ");
      print_->print(bme_sensor_->GetHumidity());
      print_->print(" %\r\n");
      if (hvac_gas_measurement_on_) {
        print_->print(" Gas = ");
        print_->print(bme_sensor_->GetGasResistance() / 1000.0);
        print_->print(" KOhms\r\n");
        settings->air_quality_score = CalculateIaqScore(bme_sensor_->GetHumidity(), bme_sensor_->GetGasResistance());
        print_->print(" IAQ: ");
        print_->print(settings->air_quality_score);
        print_->print("% ");
      }

      // Turn off the gas heater/sensor if it was previously enabled.
      if (hvac_gas_measurement_on_) {
        bme_sensor_->EnableGasHeater(false);
        hvac_gas_measurement_on_ = false;
      }

      // // Run the gas sensor periodically (every 20s). We do this periodically based on the
      // // modulo of the hvac_run_counter.
      // if (hvac_run_counter % (20000 / kRunEveryMillis) == 0) {
      //   bme_sensor->EnableGasHeater(true);
      //   hvac_gas_measurement_on = true;
      //  }

      // Kick off the next asynchronous readings.
      bme_sensor_->StartRequestAsync();
      dallas_sensor_->StartRequestAsync();

      // Only subtract past values that were previously added to the window.
      if (temperature_filled_) {
        temperature_sum_ -= temperature_window_[temperature_index_];
      }
      temperature_window_[temperature_index_] = settings->current_temperature_x10;
      temperature_sum_ += temperature_window_[temperature_index_];
      if (temperature_index_ == kTemperatureWindowSize - 1) {
        temperature_filled_ = true;
      }
      temperature_index_ = (temperature_index_ + 1) % kTemperatureWindowSize;

      const uint8_t temperature_counts =
        temperature_filled_ ? kTemperatureWindowSize : temperature_index_;
      const int temperature_mean = temperature_sum_ / temperature_counts;
      settings->current_mean_temperature_x10 = temperature_mean;

      return status;
    }

  private:
    // Window for calculating the mean temperature.
    //
    // This conditions the temperature signal ensuring fast fluctations don't
    // affect the HVAC.
    int temperature_window_[kTemperatureWindowSize] = {0};

    // Maintain a sum of the window values to make calculating the mean fast.
    int temperature_sum_ = 0;

    // Index to fill in next. Starts at zero so the mean can be calculated accurately during
    // initial filling.
    uint8_t temperature_index_ = 0;

    // Flag gets updated to true once temperature_index reaches temperature_SIZE. This ensures
    // the mean calculation is accurate.
    bool temperature_filled_ = false;

    bool sensor_started_ = false;
    bool values_initialized_ = false;
    bool hvac_gas_measurement_on_ = false;

    Clock* const clock_;
    Print* const print_;

    Sensor* const bme_sensor_;
    Sensor* const dallas_sensor_;


    ThermostatTask* const wrapped_;
};

// ThermostatTask decorator layer that performs HV/AC control management.
class PacingThermostatTask final : public ThermostatTask {
  public:
    explicit PacingThermostatTask(Clock* const clock, ThermostatTask* const wrapped) :
      clock_(clock),
      wrapped_(wrapped) {};

    Status RunOnce(Settings* settings) override {
      const uint32_t now = clock_->Millis();

      // Run only once every 1.5 seconds unless the settings (such as fan state) changed.
      if (!settings->changed && Clock::MillisDiff(settings->now, now) <= kRunEveryMillis) {
        return Status::kSkipped;
      }

      // Update the now value other thermostat tasks will use.
      settings->now = now;

      const Status status = wrapped_->RunOnce(settings);

      // Clear the settings initialization default of true.
      settings->first_run = false;

      return status;
    }

  private:
    Clock* const clock_;
    ThermostatTask* const wrapped_;
};

// Perform Fan Control using a ThermostatTask decorator layer.
//
// Determines when the fan should be running based on the events structure.
//
// This allows the fan to run for 5 minutes for better room balancing after the HVAC stops
// running.
class FanControllerThermostatTask final : public ThermostatTask {
  public:
    explicit FanControllerThermostatTask(Clock* const clock, Print* const print, ThermostatTask* const wrapped) :
      clock_(clock),
      print_(print),
      last_maintain_time_(clock->Millis()),
      wrapped_(wrapped) {};

    Status RunOnce(Settings* settings) override {

      const bool hvac_previously_on = (last_hvac_on_ == last_maintain_time_);

      // Keep the last_hvac_on time set.
      const bool hvac_running = settings->GetHvacMode() == HvacMode::HEAT || settings->GetHvacMode() == HvacMode::COOL;

      // Get the persisted fan setting.
      bool fan_enable = settings->persisted.fan_always_on;

      // Keep running the fan for extended minutes after a heat/cool cycle.
      if (last_hvac_on_set_ &&
          !hvac_previously_on &&
          clock_->minutesSince(last_hvac_on_) < settings->persisted.fan_extend_mins) {
        fan_enable = true;
      }

      // The furnace is configured to run the fan during hvac and an additional 5 minutes after each hvac cycle. Account for this in cycle_seconds.
      const bool fan_auto_runnning = (clock_->minutesSince(last_hvac_on_) < 5) || hvac_running;

      const bool fan_is_running = settings->GetFanMode() == FanMode::ON;

      if (fan_is_running || fan_auto_runnning) {
        // We decrease scaled based on the fan duty cycle.
        cycle_seconds -=  clock_->secondsSince(last_maintain_time_) / (settings->persisted.fan_on_duty / 100.0);;
      } else {
        // We increase in seconds for enablement when reaching the desired period.
        cycle_seconds += clock_->secondsSince(last_maintain_time_);
      }

      // Bound the range of cycle_seconds.
      if (cycle_seconds < 0) {
        cycle_seconds = 0;
      }

      // Require the fan to run some number of minutes.
      const uint32_t fan_period_sec = static_cast<uint32_t>(settings->persisted.fan_on_min_period) * 60;

      // Enable the fan if we hit the upper bound period.
      if (cycle_seconds >= fan_period_sec) {
        cycle_seconds = fan_period_sec;
        fan_enable = true;
      }

      // Keep the fan running until we empty the tokens to run the desired duty cycle length.
      if (fan_is_running && cycle_seconds > 0) {
        fan_enable = true;
      }

      // Store the new setting.
      settings->fan = fan_enable ? FanMode::ON : FanMode::OFF;

      // Update the previous call values.
      last_maintain_time_ = settings->now;
      if (hvac_running) {
        last_hvac_on_set_ = true;
        last_hvac_on_ = settings->now;
      }
      return wrapped_->RunOnce(settings);
    }

  private:
    Clock* const clock_;
    Print* const print_;

    float cycle_seconds = 0;

    uint32_t last_maintain_time_ = 0;
    bool last_hvac_on_set_ = false;

    // Ensures the fan meets the fan running duty cycle. Each time the fan runs, the fan will continue running until cycle_seconds returns to zero.
    //
    // We increment for each second the fan is off and decrement (1 second / duty%) for every second fan on.
    // Each time the hvac runs, we run until this counter reaches zero. To handle hvac off cases, when the cycle counter reaches the cycle period, the fan will be forced on.
    uint32_t last_hvac_on_ = 0;

    ThermostatTask* const wrapped_;
};

// ThermostatTask decorator layer that performs HV/AC control management.
class RelaySettingThermostatTask final : public ThermostatTask {
  public:
    using GetSystemStatusFn = Status (*)(void);

    explicit RelaySettingThermostatTask(Relays* const relays, Print* const print, GetSystemStatusFn system_status, ThermostatTask* const wrapped) :
      relays_(relays),
      print_(print),
      system_status_(system_status),
      wrapped_(wrapped) {};

    Status RunOnce(Settings* settings) override {
      const Status status = wrapped_->RunOnce(settings);

      // On system error, force off.
      if (system_status_() != Status::kOk) {
        relays_->Set(RelayType::kHeat, RelayState::kOff);
        relays_->Set(RelayType::kCool, RelayState::kOff);
        relays_->Set(RelayType::kFan, RelayState::kOff);
        return status;
      }

      if (settings->hvac == HvacMode::HEAT && settings->heat_high) {
        relays_->Set(RelayType::kHeatHigh, RelayState::kOn);
      } else {
        relays_->Set(RelayType::kHeatHigh, RelayState::kOff);
      }

      // Configure the relays correctly, and also display either:
      // 'c'= Cooling wants to run but heating ran too recently so it's in lockout mode yet.
      // 'C'= Cooling is turned on.
      // 'h'= Heating wants to run but cooling ran too recently so it's in lockout mode yet.
      // 'H'= Heating is turned on.
      // '_'= The thermostat is not actively requesting heating or cooling.
      if (settings->hvac == HvacMode::HEAT) {
        // Note: cycling off for 1.5s makes the furnace jets turn off, so don't do this.
        relays_->Set(RelayType::kHeat, RelayState::kOn);
        relays_->Set(RelayType::kCool, RelayState::kOff);
      } else if (settings->hvac == HvacMode::COOL) {
        relays_->Set(RelayType::kHeat, RelayState::kOff);
        relays_->Set(RelayType::kCool, RelayState::kOn);
      } else {
        relays_->Set(RelayType::kHeat, RelayState::kOff);
        relays_->Set(RelayType::kCool, RelayState::kOff);
      }

      if (settings->fan == FanMode::ON) {
        relays_->Set(RelayType::kFan, RelayState::kOn);
      } else {
        relays_->Set(RelayType::kFan, RelayState::kOff);
      }

      return status;
    }

  private:
    Relays* const relays_;
    Print* const print_;
    GetSystemStatusFn system_status_;

    ThermostatTask* const wrapped_;
};

// ThermostatTask decorator layer that performs HV/AC control management.
class UpdateDisplayThermostatTask final : public ThermostatTask {
  public:
    explicit UpdateDisplayThermostatTask(Display* const display, Print* const print, ThermostatTask* const wrapped) :
      display_(display),
      print_(print),
      wrapped_(wrapped) {};

    Status RunOnce(Settings* settings) override {
      const Status status = wrapped_->RunOnce(settings);

      // Skip empty statuses and don't spin the spinner.
      if (status == Status::kSkipped) {
        return status;
      }

      // set the cursor to column 0, line 1
      // (note: line 1 is the second row, since counting begins with 0):
      display_->SetCursor(0, 0);

      // Display the mean temperature field.
      display_->print(static_cast<int>(settings->current_mean_temperature_x10) / 10);
      display_->print(".");
      display_->print(static_cast<int>(settings->current_mean_temperature_x10) % 10);
      display_->write(uint8_t(0));  // Print the custom '°' symbol.
      display_->print(" ");

      // Display the relative humidity field.
      display_->print(static_cast<int>(settings->current_humidity));  // Clips to 99.9° indoor.
      display_->print(".");
      display_->print(static_cast<int>(settings->current_humidity * 10) % 10);  // Clips to 99.9° indoor.
      display_->print("% ");

      // Display 'o' if the manual temperature override is in effect.
      if (IsOverrideTempActive(*settings)) {
        display_->write('o');
      } else {
        display_->write(' ');
      }

      if (settings->fan == FanMode::ON) {
        display_->write('F');
      } else {
        display_->write('_');
      }

      // Configure the relays correctly, and also display either:
      // 'c'= Cooling wants to run but heating ran too recently so it's in lockout mode yet.
      // 'C'= Cooling is turned on.
      // 'h'= Heating wants to run but cooling ran too recently so it's in lockout mode yet.
      // 'H'= Heating is turned on.
      // '_'= The thermostat is not actively requesting heating or cooling.
      switch (settings->hvac) {
        case HvacMode::HEAT:
          if (settings->heat_high) {
            display_->write('#');
            break;
          }
          display_->write('H');
          break;
        case HvacMode::COOL:
          display_->write('C');
          break;
        case HvacMode::HEAT_LOCKOUT:
          display_->write('h');
          break;
        case HvacMode::COOL_LOCKOUT:
          display_->write('c');
          break;
        default:
          display_->write('_');
      }
      return status;
    }

  private:
    Display* const display_;
    Print* const print_;
    ThermostatTask* const wrapped_;
};

// ThermostatTask decorator layer that performs HV/AC control management.
class ErrorDisplayingThermostatTask final : public ThermostatTask {
  public:
    explicit ErrorDisplayingThermostatTask(Display* const display, Print* const print, ThermostatTask* const wrapped) :
      display_(display),
      print_(print),
      wrapped_(wrapped) {};

    Status RunOnce(Settings* settings) override {
      static uint8_t s_counter = 0;

      const Status status = wrapped_->RunOnce(settings);

      // Skip empty statuses and don't spin the spinner.
      if (status == Status::kSkipped) {
        return status;
      }

      // An error gets latched on the screen until the thermostat is reset.
      if (status != Status::kOk) {
        g_status = status;
      }

      // The character in the first row far right is the status.
      display_->SetCursor(15, 0);

      // We use the last character in the first row.
      if (g_status != Status::kOk) {
        // Show the error instead.
        // Errors are in the range of A-Z (0-26).
        display_->print(static_cast<char>('A' + static_cast<uint8_t>(g_status)));
        return status;
      }

      // Make the spinning animation to allow a user to know the HVAC is still fully updating.
      s_counter = (s_counter + 1) % 4;
      if (s_counter == 0) {
        display_->write('/');
      }
      if (s_counter == 1) {
        display_->write('-');
      }
      if (s_counter == 2) {
        display_->write(uint8_t(1));  // Prints a custom '\'. The LCD's default '\' is the Yen symbol.
      }
      if (s_counter == 3) {
        display_->write('|');
      }
      return status;
    }

  private:
    Display* const display_;
    Print* const print_;
    ThermostatTask* const wrapped_;
};

// ThermostatTask decorator layer that performs HV/AC control management.
class HistoryUpdatingThermostatTask final : public ThermostatTask {
  public:
    explicit HistoryUpdatingThermostatTask(ThermostatTask* const wrapped) :
      wrapped_(wrapped) {};

    Status RunOnce(Settings* settings) override {
      Status status = wrapped_->RunOnce(settings);

      // Clear any events that are over 24 days old.
      // Since the max time stored in a uint32_t is 49.7 days and we want to clearly detect rollover,
      // no event should be more than max time / 2. See MillisSubtract for more details.
      for (uint8_t i = 0; i < EVENT_SIZE; ++i) {
        if (settings->events[i].empty()) {
          continue;
        }
        if (Clock::MillisDiff(settings->events[i].start_time, settings->now) >
            Clock::DaysToMillis(24)) {
          settings->events[i].set_empty();
        }
      }

      Event* const event = &settings->events[settings->event_index];
      const HvacMode current_hvac = Sanitize(settings->GetHvacMode());
      const FanMode current_fan = Sanitize(settings->GetFanMode());

      // Update the 10 minute temperature when heating more than 10 minutes.
      if (current_hvac == HvacMode::HEAT) {
        if (Clock::MillisDiff(event->start_time, settings->now) > Clock::MinutesToMillis(10)) {
          event->temperature_10min_x10 = settings->current_mean_temperature_x10;
        }
      }

      // When the current event matches current settings, don't create a new event.
      if (current_hvac == event->hvac && current_fan == event->fan) {
        return status;
      }

      settings->event_index = (settings->event_index + 1) % EVENT_SIZE;
      Event* new_event = &settings->events[settings->event_index];
      new_event->start_time = settings->now;

      new_event->temperature_x10 = settings->current_mean_temperature_x10;
      new_event->hvac = current_hvac;
      new_event->fan = current_fan;

      // We need always maintain one empty event to ensure we don't have an
      // incorrect duration comparing against the oldest start time.
      settings->events[(settings->event_index + 1) % EVENT_SIZE]
      .set_empty();

      return status;
    };
  private:
    ThermostatTask* const wrapped_;

};

}
#endif // MAINTAIN_HVAC_H_
