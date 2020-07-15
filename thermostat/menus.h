#ifndef MENUS_H_
#define MENUS_H_
// This header implements useful menus to allow the user to change settings and view statuses.

#include "buttons.h"
#include "events.h"
#include "settings.h"
#include "interfaces.h"
#include "settings_storer.h"
//
//  Each [R] press takes the user through editable settings.
//       Pressing [U] or [D] causes edit mode to be enabled.
//       When multiple values are available to adjust, [R] allows cycling through the
//       different items in the setting. [SEL] confirms the new value and updates the
//       field/EEPROM.
//
//  Each [L] Takes the user through status information.
//
//  Main -> [R][R] Setpoint 1 -> [U][D] Edits HH:MM *F -> [Up] Increase item selected
//                                                  -> [Dn] Decrease item selected
//                                                  -> [L] ABORT and return to Home
//                                                  status.
//                                                  -> [R] Move item selected to right
//                                                  -> [Sel] Save
//

namespace thermostat {

//using WaitForButtonPressFn = std::function<Button(uint32_t timeout)>;
using WaitForButtonPressFn = Button (*)(uint32_t timeout);
using SettingFn = Button (*)();
using GetDateFn = Date (*)();

static void SetChangedAndPersist(Settings *settings, SettingsStorer *writer) {
  settings->changed = true;
  writer->Write(*settings);
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

class Flasher {
  public:
    Flasher(Clock* clock) : clock_(clock) {};
    bool State() {

      if (clock_->millisSince(flash_ms_) > 500) {
        state_ = !state_;

        flash_ms_ = clock_->Millis();
      }
      return state_;
    };

    // Clear the counter which keeps the flashing state off for the next 500ms.
    //
    // This allows the user to see the digits without flashing when actively adjusting the values.
    void Clear() {
      state_ = false;
      flash_ms_ = clock_->Millis();
    };

    void Underline() {
      state_ = true;
      flash_ms_ = clock_->Millis();
    };

  private:
    // Delete the copy constructor
    Flasher(const Flasher&) = delete;

    // Delete the copy assignment operator
    Flasher& operator=(const Flasher&) = delete;

    bool state_ = false;
    Clock *clock_;
    uint32_t flash_ms_ = clock_->Millis();
};

// Represents an editable digit in the display.
//
// For flashing to work correctly, Print needs to be called twice per second.
// If x10 is true, a decimal point is added before the last digit.
class Digit {
  public:
    Digit(uint16_t value, uint16_t min, uint16_t max, const bool x10, const char *unit, Display *display, Flasher *flasher) : value_(value), min_(min), max_(max), x10_(x10), unit_(unit), display_(display), flasher_(flasher) {
    };
    uint16_t Value() {
      return value_;
    };
    void Increment(const bool selected, const int amount) {
      if (!selected) {
        return;
      }
      flasher_->Clear();

      // Safely prevent going below zero.
      if (amount < 0 && static_cast<uint16_t>(amount * -1) > value_) {
        SetValue(0);
        return;
      }
      // Set the value with min/max bounds checking.
      SetValue(value_ + amount);
    };

    void SetValue(const uint16_t value) {
      if (value < min_) {
        value_ = min_;
        return;
      }
      if (value > max_) {
        value_ = max_;
        return;
      }
      value_ = value;

    };

    void Print(bool selected) {
      // One significant digit fixed point decimal case.
      if (x10_) {
        // Print the flash underscore state.
        if (selected && flasher_->State()) {
          // Should we show 3 digits?
          if (max_ >= 100) {
            display_->write('_');
          }
          display_->print("_._");
          display_->print(unit_);
          return;
        }
        // Otherwise print the value and prefix '0's for small values.
        if (max_ >= 100 && value_ < 100) {
          display_->write('0');
        }
        display_->print(value_ / 10);
        display_->write('.');
        display_->print(value_ % 10);
        display_->print(unit_);
        return;
      }
      // Normal 2 or 3 digit whole number.
      if (selected && flasher_->State()) {
        for (int max_div = max_; max_div > 1; max_div /= 10) {
          display_->write('_');
        }
        display_->print(unit_);
        return;
      }
      if (max_ >= 100 && value_ < 100) {
        display_->write('0');
      }
      if (max_ >= 10 && value_ < 10) {
        display_->write('0');
      }
      display_->print(value_);
      display_->print(unit_);
    }

  private:
    uint16_t value_;
    const uint16_t min_;
    const uint16_t max_;
    const bool x10_;
    const char* const unit_;
    Display* const display_;
    Flasher* const flasher_;
};

// Returns button presses and waits 10 seconds before returning a Timeout button.
// This returns every 500ms when no buttons are pressed to allow flashing to work.
class Waiter {
  public:
    Waiter(WaitForButtonPressFn *wait_for_button_press) :
      wait_for_button_press_(wait_for_button_press) {};

    Button Wait() {
      Button button = (*wait_for_button_press_)(500);
      if (button == Button::TIMEOUT) {
        timeout_counter_++;
        if (timeout_counter_ < 20) {
          // Stay in the menu for now.
          return Button::NONE;
        }
        return button;
      }
      timeout_counter_ = 0;
      return button;
    };

  private:
    uint16_t timeout_counter_ = 0;
    WaitForButtonPressFn *wait_for_button_press_;
};


// This menu operates on the second row of the LCD whereas the update function operates
// the HVAC and first row of the LCD.
class Menus {
  public:
    Menus(Settings *settings, WaitForButtonPressFn wait_for_button_press, Clock *clock,
          Display *display, SettingsStorer *storer)
      : settings_(settings),
        storer_(storer),
        wait_for_button_press_(wait_for_button_press),
        clock_(clock),
        display_(display) {}

    void ShowStatuses() {
      uint8_t menu_index = 0;
      constexpr uint8_t MENU_MAX = 6;

      while (true) {
        ResetLine();
        Button button;
        switch (menu_index) {
          case 0:
            display_->SetCursor(0, 1);
            display_->print("On ratio: ");
            display_->print(OnPercent(*settings_, clock_->Millis()));
            button = wait_for_button_press_(10000);
            break;
          case 1:
            display_->SetCursor(0, 1);
            display_->print("BME Temp: ");
            display_->print(settings_->current_bme_temperature_x10);
            button = wait_for_button_press_(10000);
            break;
          case 2:
            display_->SetCursor(0, 1);
            display_->print("Dal Temp: ");
            display_->print(settings_->current_temperature_x10);
            button = wait_for_button_press_(10000);
            break;
          case 3:
            display_->SetCursor(0, 1);
            display_->print("IAQ: ");
            display_->print(settings_->air_quality_score);
            button = wait_for_button_press_(10000);
            break;
          case 4:
            display_->SetCursor(0, 1);
            display_->print("Status 4: ");
            button = wait_for_button_press_(10000);
            break;
          case 5:
            display_->SetCursor(0, 1);
            display_->print("Status 5: ");
            button = wait_for_button_press_(10000);
            break;
        }

        ResetLine();

        switch (button) {
          case Button::LEFT:
            menu_index = (menu_index + 1) % MENU_MAX;
            break;
          case Button::RIGHT:
          case Button::NONE:
          case Button::TIMEOUT:
          default:
            return;
        }
      }
    }

    void EditSettings() {
      ResetLine();

      uint8_t menu_index = 0;
      constexpr uint8_t kMenuMax = 11;

      while (true) {
        Button button;
        switch (menu_index) {
          case 0:
            button = SetHumidifierLevel();
            break;
          case 1:
            button = SetFan();
            break;
          case 2:
            button = SetMode();
            break;
          case 3:
            button = SetSetpointNew(0, Mode::HEAT);
            break;
          case 4:
            button = SetSetpointNew(1, Mode::HEAT);
            break;
          case 5:
            button = SetSetpointNew(0, Mode::COOL);
            break;
          case 6:
            button = SetSetpointNew(1, Mode::COOL);
            break;
          case 8:
            button = SetTolerance();
            break;
          case 9:
            button = SetDate();
            break;
          case 10:
            button = SetFanCycle();
            break;
          case kMenuMax:
            break;
        }
        switch (button) {
          case Button::LEFT:
            // Left is the exit button.
            return;
          case Button::RIGHT:
            menu_index = (menu_index + 1) % kMenuMax;
            break;
          case Button::NONE:
          case Button::TIMEOUT:
          default:
            return;
        }
      }
    };

    Button SetFanCycle() {
      // Fields: Minimum period between fan cycles in minutes 0-999m, on time duty 0-90.
      // 1234567890123456
      // Fan .Δ: 999m 90%
      Flasher flasher(clock_);
      Waiter waiter(&wait_for_button_press_);
      Digit mins = Digit(settings_->persisted.fan_on_min_period, 0, 999, false, "m", display_, &flasher);
      Digit duty = Digit(settings_->persisted.fan_on_duty, 0, 99, false, "%", display_, &flasher);

      uint8_t field = 0;
      constexpr uint8_t kTotalFields = 2;

      // Setup the display line.
      ResetLine();
      display_->print("Fan dt: ");
      auto update = [&]() {
        display_->SetCursor(7, 1);
        mins.Print(field == 0);
        display_->write(' ');
        duty.Print(field == 1);
      };
      // First see if the user wants to change this item.
      update();
      {
        const Button button = WaitBeforeEdit();
        if (button != Button::SELECT) {
          return button;
        }
      }

      while (true) {
        Button button = waiter.Wait();

        // Exit when we get a left arrow in the edit menu.
        int increment = 1;
        switch (button) {
          case Button::DOWN:
            increment = -1;
          // FALLTHRU
          case Button::UP:
            mins.Increment(field == 0, increment);
            duty.Increment(field == 1, increment);
            break;
          case Button::RIGHT:
            field = (field + 1) % kTotalFields;
            break;
          case Button::SELECT:
            // Update the settings data.
            settings_->persisted.fan_on_min_period = mins.Value();
            settings_->persisted.fan_on_duty = duty.Value();
            SetChangedAndPersist(settings_, storer_);
            PrintUpdatedAndWait();
            return Button::NONE;
            break;
          case Button::NONE:
            // Do nothing.
            break;
          default:
            // Exit.
            return button;
        }
        update();
      }
    };

    Button SetFan() {
      const bool initial_fan_setting = settings_->persisted.fan_always_on;
      bool fan = settings_->persisted.fan_always_on;

      uint8_t field = 0;
      constexpr uint8_t kTotalFields = 2;
      uint16_t fan_extend_mins = settings_->persisted.fan_extend_mins;

      uint32_t flash_ms = clock_->Millis();
      bool flash_state = false;

      int timeout_counter = 0;

      // Setup the display
      // "Fan: EXT:060m"
      // "Fan: ON     "
      ResetLine();
      display_->print("Fan: ");
      auto update = [&]() {
        display_->SetCursor(5, 1);
        if (clock_->millisSince(flash_ms) > 500) {
          flash_state = !flash_state;
          flash_ms = clock_->Millis();
        }

        // Print the Fan On/Off status.
        if (field == 0 && flash_state == true) {
          display_->print("___ ");
        } else {
          display_->print(fan ? "ON  " : "EXT:");
        }

        // Print the extended fan time.
        //
        // When the fan is always on, hide the extended minutes.
        if (fan == true) {
          display_->print("    ");
        } else if (field == 1 && flash_state == true) {
          display_->print("___m");
        } else {
          if (fan_extend_mins < 100) {
            display_->print("0");
          }
          if (fan_extend_mins < 10) {
            display_->print("0");
          }
          display_->print(fan_extend_mins);
          display_->print("m");
        }
      };
      // First see if the user wants to change this item.
      update();
      Button button = WaitBeforeEdit();
      if (button != Button::SELECT) {
        return button;
      }

      while (true) {
        // Only after 10 seconds of timeouts do we pass the timeout button.
        Button button = wait_for_button_press_(500);
        if (button == Button::TIMEOUT) {
          timeout_counter++;
          if (timeout_counter < 20) {
            // Stay in the menu for now.
            button = Button::NONE;
          }
        } else {
          timeout_counter = 0;
        }

        // Exit when we get a left arrow in the edit menu.
        int increment = 1;
        switch (button) {
          case Button::DOWN:
            if (field == 0) {
              fan = false;
              // Also change the fan temporarily. This makes it easer to change the fan
              // speed.
              settings_->persisted.fan_always_on = fan;
              settings_->persisted.fan_extend_mins = fan_extend_mins;
              SetChanged(settings_);
              break;
            }
            if (field == 1) {
              increment = -1;
            }
          /* FALLTHRU */
          case Button::UP:
            if (field == 0) {
              fan = true;

              // Change the fan temporarily. This makes it easer to perform special fan
              // speed changes.
              settings_->persisted.fan_always_on = fan;
              settings_->persisted.fan_extend_mins = fan_extend_mins;
              SetChanged(settings_);
            }
            if (field == 1) {
              fan_extend_mins += increment;
            }
            break;
          case Button::RIGHT:
            // Only allow changing the extended fan time if the fan setting is set to auto
            // (false).
            if ((field == 0 && fan == false) || (field == 1)) {
              field = (field + 1) % kTotalFields;
            }
            break;
          case Button::SELECT:
            // Update the settings data.
            settings_->persisted.fan_always_on = fan;
            settings_->persisted.fan_extend_mins = fan_extend_mins;
            SetChangedAndPersist(settings_, storer_);

            PrintUpdatedAndWait();
            return Button::NONE;
            break;
          case Button::NONE:
            // Do nothing.
            break;
          default:
            // Restore the fan setting to the initial value when cancelled.
            settings_->persisted.fan_always_on = initial_fan_setting;
            SetChanged(settings_);

            return button;
        }
        update();
      }
    }

    Button SetMode() {
      uint8_t mode =
        settings_->persisted.heat_enabled << 1 | settings_->persisted.cool_enabled;

      // Setup the display
      ResetLine();
      display_->print("Mode: ");

      // Draw the value.
      auto update = [&]() {
        display_->SetCursor(5, 1);
        display_->print(mode == 3 ? "BOTH" : mode == 2 ? "HEAT" : mode == 1 ? "COOL" : "OFF ");
      };
      update();

      while (true) {
        const Button button = wait_for_button_press_(10000);

        // Exit when we get a left arrow in the edit menu.
        switch (button) {
          case Button::DOWN:
            if (mode == 0) {
              mode = 3;
              break;
            };
            --mode;
            break;
          case Button::UP:
            if (mode == 3) {
              mode = 0;
              break;
            }
            ++mode;

            break;
          case Button::SELECT:
            // Update the settings data.
            settings_->persisted.heat_enabled = mode & 0x02 ? true : false;
            settings_->persisted.cool_enabled = mode & 0x01 ? true : false;
            SetChangedAndPersist(settings_, storer_);

            PrintUpdatedAndWait();
            return Button::NONE;
            break;
          default:
            return button;
        }
        update();
      }
    }

    //    Button SetDateNew() {
    //      constexpr uint8_t kMaxFields = 3;
    //      uint8_t field = 0;
    //      // 1234567890123456
    //      // H1: XX.X° XX:XX
    //      Date date = clock_->Now();
    //
    //      Flasher flasher(clock_);
    //      Waiter waiter(&wait_for_button_press_);
    //      Digit hrs = Digit(date.hour, 0, 23, false, ":", display_, &flasher);
    //      Digit mins = Digit(date.minute, 0, 99, false, "", display_, &flasher);
    //
    //      ResetLine();
    //      display_->print((mode == Mode::HEAT) ? "H" : "C");
    //      display_->print(setpoint + 1);
    //      display_->print(":");
    //      auto update = [&]() {
    //        display_->SetCursor(3, 1);
    //        temp.Print(field == 1);
    //
    //        // Print custom degree symbol.
    //        display_->write(uint8_t(0));
    //
    //        display_->write(' ');
    //        hrs.Print(field == 2);
    //        //display_->print(":");
    //        mins.Print(field==3);
    //      };
    //
    //      // First see if the user wants to change this item.
    //      update();
    //      Button button = WaitBeforeEdit();
    //      if (button != Button::SELECT) {
    //        return button;
    //      }
    //      // Start at field 1 if edit chosen.
    //      field = 1;
    //      flasher.Underline();
    //
    //      while (true) {
    //        // Update the display.
    //        update();
    //
    //        Button button = waiter.Wait();
    //
    //        // Exit when we get a left arrow in the edit menu.
    //        int increment = 1;
    //        switch (button) {
    //          case Button::LEFT:
    //            // Left is the exit button.
    //            return button;
    //          case Button::RIGHT:
    //            ++field;
    //            if (field > kMaxFields) { field = 1; }
    //            flasher.Underline();
    //            break;
    //          case Button::DOWN:
    //            // Use the same logic as Button::UP, except that the value will be decremented
    //            // instead of incremented.
    //            increment = -1;
    //          // Fall through.
    //          case Button::UP:
    //            temp.Increment(field == 1, increment);
    //            hrs.Increment(field == 2, increment);
    //            mins.Increment(field == 3, increment);
    //            break;
    //          case Button::SELECT:
    //            if (mode == Mode::HEAT) {
    //              settings_->persisted.heat_setpoints[setpoint].temperature_x10 =
    //                temp.Value();
    //              settings_->persisted.heat_setpoints[setpoint].hour = hrs.Value();
    //              settings_->persisted.heat_setpoints[setpoint].minute = mins.Value();
    //            } else {
    //              settings_->persisted.cool_setpoints[setpoint].temperature_x10 =
    //                temp.Value();
    //              settings_->persisted.cool_setpoints[setpoint].hour = hrs.Value();
    //              settings_->persisted.cool_setpoints[setpoint].minute = mins.Value();
    //            }
    //            // Update the settings data.
    //            SetChangedAndPersist(settings_, storer_);
    //            PrintUpdatedAndWait();
    //            return Button::NONE;
    //            break;
    //          case Button::NONE:
    //            // Do nothing, this is likely due to a timeout override.
    //            break;
    //          default:
    //            return button;
    //        }
    //      }
    //    }

    Button SetDate() {
      uint8_t field = 0;
      Date date = clock_->Now();
      uint32_t flash_ms = clock_->Millis();
      bool flash_state = false;

      ResetLine();
      display_->print("Date: ");
      auto update = [&]() {
        display_->SetCursor(6, 1);
        if (clock_->millisSince(flash_ms) > 500) {
          flash_state = !flash_state;
          flash_ms = clock_->Millis();
        }

        // 00:00 Mo
        if (field == 0 && flash_state == true) {
          display_->print("__");
        } else {
          if (date.hour < 10) {
            display_->print("0");
          }
          display_->print(static_cast<uint16_t>(date.hour));
        }

        display_->print(":");

        if (field == 1 && flash_state == true) {
          display_->print("__");
        } else {
          if (date.minute < 10) {
            display_->print("0");
          }
          display_->print(static_cast<uint16_t>(date.minute));
        }
        display_->write(' ');

        if (field == 2 && flash_state == true) {
          display_->print("__");
        } else {
          display_->print(daysOfTheWeek[date.day_of_week]);
        }
      };

      // First see if the user wants to change this item.
      update();
      Button button = WaitBeforeEdit();
      if (button != Button::SELECT) {
        return button;
      }

      int timeout_counter = 0;

      while (true) {
        // Only after 10 seconds of timeouts do we pass the timeout button.
        Button button = wait_for_button_press_(500);
        if (button == Button::TIMEOUT) {
          timeout_counter++;
          if (timeout_counter < 20) {
            button = Button::NONE;
          }
        } else {
          timeout_counter = 0;
        }

        // Exit when we get a left arrow in the edit menu.
        int increment = 1;
        switch (button) {
          case Button::LEFT:
            // Left is the exit button.
            return button;
          case Button::RIGHT:
            field = (field + 1) % 3;
            break;
          case Button::DOWN:
            increment = -1;
          // Fall through.
          case Button::UP:
            // Stop flashing while updating.
            flash_state = false;
            flash_ms = clock_->Millis();

            switch (field) {
              case 0:
                date.hour += increment;
                break;
              case 1:
                date.minute += increment;
                break;
              case 2:
                date.day_of_week += increment;
                break;
              default:
                break;
            }
            break;
          case Button::SELECT:
            // Update the settings data.
            clock_->Set(date);
            SetChangedAndPersist(settings_, storer_);
            PrintUpdatedAndWait();
            return Button::NONE;
            break;
          case Button::NONE:
            // Do nothing, this is likely due to a timeout override.
            break;
          default:
            return button;
        }
        // Update the display.
        update();
      }
    }

    Button SetSetpoint(const uint8_t setpoint, const Mode mode) {
      constexpr uint8_t kMaxFields = 3;
      uint8_t field = 0;

      // Copy the current settings for editing.
      int temperature_x10 =
        (mode == Mode::HEAT)
        ? settings_->persisted.heat_setpoints[setpoint].temperature_x10
        : settings_->persisted.cool_setpoints[setpoint].temperature_x10;
      ;
      int hour = (mode == Mode::HEAT) ? settings_->persisted.heat_setpoints[setpoint].hour
                 : settings_->persisted.cool_setpoints[setpoint].hour;
      int minute = (mode == Mode::HEAT)
                   ? settings_->persisted.heat_setpoints[setpoint].minute
                   : settings_->persisted.cool_setpoints[setpoint].minute;

      // Flash state.
      uint32_t flash_ms = clock_->Millis();
      bool flash_state = false;

      ResetLine();
      display_->print((mode == Mode::HEAT) ? "H" : "C");
      auto update = [&]() {
        display_->SetCursor(1, 1);
        display_->print(setpoint + 1);
        display_->print(":");

        if (clock_->millisSince(flash_ms) > 500) {
          flash_state = !flash_state;
          flash_ms = clock_->Millis();
        }

        // 00.0^ 00:00
        if (field == 0 && flash_state == true) {
          display_->print("__");
        } else {
          if (temperature_x10 / 10 < 10) {
            display_->write('0');
          }
          display_->print(temperature_x10 / 10);
        }

        display_->write('.');

        if (field == 0 && flash_state == true) {
          display_->print("_");
        } else {
          display_->print(temperature_x10 % 10);
        }
        // Print custom degree symbol.
        display_->write(uint8_t(0));

        display_->write(' ');
        if (field == 1 && flash_state == true) {
          display_->print("__");
        } else {
          if (hour < 10) {
            display_->print("0");
          }
          display_->print(hour);
        }
        display_->print(":");
        if (field == 2 && flash_state == true) {
          display_->print("__");
        } else {
          if (minute < 10) {
            display_->print("0");
          }
          display_->print(minute);
        }
      };

      // First see if the user wants to change this item.
      update();
      Button button = WaitBeforeEdit();
      if (button != Button::SELECT) {
        return button;
      }

      int timeout_counter = 0;

      while (true) {
        // Only after 10 seconds of timeouts do we pass the timeout button.
        Button button = wait_for_button_press_(500);
        if (button == Button::TIMEOUT) {
          timeout_counter++;
          if (timeout_counter < 20) {
            button = Button::NONE;
          }
        } else {
          timeout_counter = 0;
        }

        // Exit when we get a left arrow in the edit menu.
        int increment = 1;
        switch (button) {
          case Button::LEFT:
            // Left is the exit button.
            return button;
          case Button::RIGHT:
            field = (field + 1) % kMaxFields;
            break;
          case Button::DOWN:
            // Use the same logic as Button::UP, except that the value will be decremented
            // instead of incremented.
            increment = -1;
          // Fall through.
          case Button::UP:
            // Stop flashing while updating.
            flash_state = false;
            flash_ms = clock_->Millis();

            switch (field) {
              case 0:
                temperature_x10 += increment;
                break;
              case 1:
                hour += increment;
                break;
              case 2:
                minute += increment;
                break;
              default:
                break;
            }
            break;
          case Button::SELECT:
            if (mode == Mode::HEAT) {
              settings_->persisted.heat_setpoints[setpoint].temperature_x10 =
                temperature_x10;
              settings_->persisted.heat_setpoints[setpoint].hour = hour;
              settings_->persisted.heat_setpoints[setpoint].minute = minute;
            } else {
              settings_->persisted.cool_setpoints[setpoint].temperature_x10 =
                temperature_x10;
              settings_->persisted.cool_setpoints[setpoint].hour = hour;
              settings_->persisted.cool_setpoints[setpoint].minute = minute;
            }
            // Update the settings data.
            SetChangedAndPersist(settings_, storer_);
            PrintUpdatedAndWait();
            return Button::NONE;
            break;
          case Button::NONE:
            // Do nothing, this is likely due to a timeout override.
            break;
          default:
            return button;
        }
        // Update the display.
        update();
      }
    }

    Button SetSetpointNew(const uint8_t setpoint, const Mode mode) {
      constexpr uint8_t kMaxFields = 3;
      uint8_t field = 0;
      // 1234567890123456
      // H1: XX.X° XX:XX
      Flasher flasher(clock_);
      Waiter waiter(&wait_for_button_press_);
      Digit temp = Digit((mode == Mode::HEAT)
                         ? settings_->persisted.heat_setpoints[setpoint].temperature_x10
                         : settings_->persisted.cool_setpoints[setpoint].temperature_x10, 0, 999, true, "", display_, &flasher);
      Digit hrs = Digit((mode == Mode::HEAT) ? settings_->persisted.heat_setpoints[setpoint].hour
                        : settings_->persisted.cool_setpoints[setpoint].hour, 0, 23, false, ":", display_, &flasher);
      Digit mins = Digit((mode == Mode::HEAT)
                         ? settings_->persisted.heat_setpoints[setpoint].minute
                         : settings_->persisted.cool_setpoints[setpoint].minute, 0, 99, false, "", display_, &flasher);

      ResetLine();
      display_->print((mode == Mode::HEAT) ? "H" : "C");
      display_->print(setpoint + 1);
      display_->print(":");
      auto update = [&]() {
        display_->SetCursor(3, 1);
        temp.Print(field == 1);

        // Print custom degree symbol.
        display_->write(uint8_t(0));

        display_->write(' ');
        hrs.Print(field == 2);
        //display_->print(":");
        mins.Print(field == 3);
      };

      // First see if the user wants to change this item.
      update();
      Button button = WaitBeforeEdit();
      if (button != Button::SELECT) {
        return button;
      }
      // Start at field 1 if edit chosen.
      field = 1;
      flasher.Underline();

      while (true) {
        // Update the display.
        update();

        Button button = waiter.Wait();

        // Exit when we get a left arrow in the edit menu.
        int increment = 1;
        switch (button) {
          case Button::LEFT:
            // Left is the exit button.
            return button;
          case Button::RIGHT:
            ++field;
            if (field > kMaxFields) {
              field = 1;
            }
            flasher.Underline();
            break;
          case Button::DOWN:
            // Use the same logic as Button::UP, except that the value will be decremented
            // instead of incremented.
            increment = -1;
          // Fall through.
          case Button::UP:
            temp.Increment(field == 1, increment);
            hrs.Increment(field == 2, increment);
            mins.Increment(field == 3, increment);
            break;
          case Button::SELECT:
            if (mode == Mode::HEAT) {
              settings_->persisted.heat_setpoints[setpoint].temperature_x10 =
                temp.Value();
              settings_->persisted.heat_setpoints[setpoint].hour = hrs.Value();
              settings_->persisted.heat_setpoints[setpoint].minute = mins.Value();
            } else {
              settings_->persisted.cool_setpoints[setpoint].temperature_x10 =
                temp.Value();
              settings_->persisted.cool_setpoints[setpoint].hour = hrs.Value();
              settings_->persisted.cool_setpoints[setpoint].minute = mins.Value();
            }
            // Update the settings data.
            SetChangedAndPersist(settings_, storer_);
            PrintUpdatedAndWait();
            return Button::NONE;
            break;
          case Button::NONE:
            // Do nothing, this is likely due to a timeout override.
            break;
          default:
            return button;
        }
      }
    }


    Button SetTolerance() {
      int val = settings_->persisted.tolerance_x10;
      uint32_t flash_ms = clock_->Millis();
      bool flash_state = false;

      ResetLine();
      display_->print("Tolerance: ");
      // Updates the display for the changed fields.
      auto update = [&]() {
        display_->SetCursor(11, 1);

        if (clock_->millisSince(flash_ms) > 500) {
          flash_state = !flash_state;
          flash_ms = clock_->Millis();
        }

        // Tolerance: 00.0^
        if (flash_state == true) {
          display_->print("__._");
        } else {
          if (val / 10 < 10) {
            display_->print("0");
          }
          display_->print(val / 10);
          display_->print(".");
          display_->print(val % 10);
        }
        // Print custom degree symbol.
        display_->write(0);
      };

      update();

      int timeout_counter = 0;

      while (true) {
        // Only after 10 seconds of timeouts do we pass the timeout button.
        Button button = wait_for_button_press_(500);
        if (button == Button::TIMEOUT) {
          timeout_counter++;
          if (timeout_counter < 20) {
            button = Button::NONE;
          }
        } else {
          timeout_counter = 0;
        }

        // Exit when we get a left arrow in the edit menu.
        int increment = 1;
        switch (button) {
          case Button::LEFT:
            // Left is the exit button.
            return button;
          case Button::RIGHT:
            return button;
            break;
          case Button::DOWN:
            increment = -1;
          // Fall through.
          case Button::UP:
            // Stop flashing while updating.
            flash_state = false;
            flash_ms = clock_->Millis();

            val += increment;
            break;
          case Button::SELECT:
            // Update the settings data.
            settings_->persisted.tolerance_x10 = val;
            SetChangedAndPersist(settings_, storer_);
            PrintUpdatedAndWait();
            return Button::NONE;
            break;
          case Button::NONE:
            // Do nothing, this is likely due to a timeout override.
            break;
          default:
            return button;
        }
        // Update the display.
        update();
      }
    }

    Button SetHumidifierLevel() {
      int val = settings_->persisted.humidity;
      uint32_t flash_ms = clock_->Millis();
      bool flash_state = false;

      ResetLine();
      display_->print("Humidify: ");
      // Updates the display for the changed fields.
      auto update = [&]() {
        display_->SetCursor(10, 1);

        if (clock_->millisSince(flash_ms) > 500) {
          flash_state = !flash_state;
          flash_ms = clock_->Millis();
        }

        // Tolerance: 00%
        if (flash_state == true) {
          display_->print("__");
        } else {
          if (val < 10) {
            display_->print("0");
          }
          display_->print(val);
        }
        // Print custom degree symbol.
        display_->write('%');
      };

      update();

      int timeout_counter = 0;

      while (true) {
        // Only after 10 seconds of timeouts do we pass the timeout button.
        Button button = wait_for_button_press_(500);
        if (button == Button::TIMEOUT) {
          timeout_counter++;
          if (timeout_counter < 20) {
            button = Button::NONE;
          }
        } else {
          timeout_counter = 0;
        }

        // Exit when we get a left arrow in the edit menu.
        int increment = 1;
        switch (button) {
          case Button::LEFT:
            // Left is the exit button.
            return button;
          case Button::RIGHT:
            return button;
            break;
          case Button::DOWN:
            increment = -1;
          // Fall through.
          case Button::UP:
            // Stop flashing while updating.
            flash_state = false;
            flash_ms = clock_->Millis();

            // Limit the value to a max of 50%.
            if (val >= 50 && increment > 0) {
              break;
            }

            // Limit the value to a max of 50%.
            if (val <= 0 && increment < 0) {
              break;
            }
            val += increment;
            break;
          case Button::SELECT:
            // Update the settings data.
            settings_->persisted.humidity = val;
            SetChangedAndPersist(settings_, storer_);
            PrintUpdatedAndWait();
            return Button::NONE;
            break;
          case Button::NONE:
            // Do nothing, this is likely due to a timeout override.
            break;
          default:
            return button;
        }
        // Update the display.
        update();
      }
    }

    // Updates the second line of the display when a menu isn't active.
    Button InformationalState() {
      while (true) {
        // Display the status information.
        Date date = clock_->Now();
        int hours = date.hour;
        int minutes = date.minute;
        display_->SetCursor(0, 1);
        display_->print("Time: ");
        if (hours < 10) {
          display_->print("0");
        }
        display_->print(hours);
        display_->print(":");
        if (minutes < 10) {
          display_->print("0");
        }
        display_->print(minutes);

        display_->print("      ");

        Button button = wait_for_button_press_(2000);

        if (button != Button::NONE && button != Button::TIMEOUT) {
          return button;
        }
      }
    }

    // Allows changing the manual temperature override.
    //
    // This is called when the user presses Up/Down on the main informational status page.
    Button EditOverrideTemp() {
      while (true) {
        display_->SetCursor(0, 1);
        display_->print("Override: ");
        // print the number of seconds since reset:
        display_->SetCursor(10, 1);
        int temp = GetOverrideTemp(*settings_);
        display_->print(static_cast<int>(temp) / 10);
        display_->print(".");
        display_->print(static_cast<int>(temp) % 10);
        display_->write(0);
        display_->write(' ');

        const Button button = wait_for_button_press_(10000);
        switch (button) {
          case Button::UP:
            SetOverrideTemp(1, settings_, clock_->Millis());
            continue;
          case Button::DOWN:
            SetOverrideTemp(-1, settings_, clock_->Millis());
            continue;
          default:
            break;
        }
        return button;
      }
    }

  private:
    // Helper to reset the menu managed second row.
    void ResetLine() {
      display_->SetCursor(0, 1);
      display_->print("                ");
      display_->SetCursor(0, 1);
    }

    Button WaitBeforeEdit() {
      while (true) {
        Button button = wait_for_button_press_(10000);
        // Up and Down imply selection and go to editing.
        if (button == Button::UP || button == Button::DOWN) {
          return Button::SELECT;
        } else {
          return button;
        }
      }
    }

    void PrintUpdatedAndWait() {
      ResetLine();
      display_->print("Updated...");
      wait_for_button_press_(1000);
    }

    Settings *settings_;
    SettingsStorer *storer_;
    WaitForButtonPressFn wait_for_button_press_;
    Clock *clock_;
    Display *display_;
};

}
#endif  // MENUS_H_
