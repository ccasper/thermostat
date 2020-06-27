#ifndef MENUS_H_
#define MENUS_H_

#include "buttons.h"
#include "events.h"
#include "avr_impls.h"
#include "settings.h"
#include "interfaces.h"
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

using WaitForButtonPressFn = Button (*)(uint32_t timeout);
using SettingFn = Button (*)();
using GetDateFn = Date (*)();

// This menu operates on the second row of the LCD whereas the update function operates
// the HVAC and first row of the LCD.
class Menus {
  public:
    Menus(Settings *settings, WaitForButtonPressFn wait_for_button_press, Clock *clock,
          Display *display)
      : settings_(settings),
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
            display_->Print("On ratio: ");
            display_->Print(OnPercent(*settings_, clock_->Millis()));
            button = wait_for_button_press_(10000);
            break;
          case 1:
            display_->SetCursor(0, 1);
            display_->Print("BME Temp: ");
            display_->Print(settings_->current_bme_temperature_x10);
            button = wait_for_button_press_(10000);
            break;
          case 2:
            display_->SetCursor(0, 1);
            display_->Print("Dal Temp: ");
            display_->Print(settings_->current_temperature_x10);
            button = wait_for_button_press_(10000);
            break;
          case 3:
            display_->SetCursor(0, 1);
            display_->Print("IAQ: ");
            display_->Print(settings_->air_quality_score);
            button = wait_for_button_press_(10000);
            break;
          case 4:
            display_->SetCursor(0, 1);
            display_->Print("Status 4: ");
            button = wait_for_button_press_(10000);
            break;
          case 5:
            display_->SetCursor(0, 1);
            display_->Print("Status 5: ");
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
      constexpr uint8_t MENU_MAX = 11;

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
            button = SetSetpoint(0, Mode::HEAT);
            break;
          case 4:
            button = SetSetpoint(1, Mode::HEAT);
            break;
          case 5:
            button = SetSetpoint(0, Mode::COOL);
            break;
          case 6:
            button = SetSetpoint(1, Mode::COOL);
            break;
          case 8:
            button = SetTolerance();
            break;
          case 9:
            button = SetDate();
            break;
          case 10:
            button = SetSaveAllSettingsForDebug();
            break;
          case MENU_MAX:
            break;
        }
        switch (button) {
          case Button::LEFT:
            // Left is the exit button.
            return;
          case Button::RIGHT:
            menu_index = (menu_index + 1) % MENU_MAX;
            break;
          case Button::NONE:
          case Button::TIMEOUT:
          default:
            return;
        }
      }
    }

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
      display_->Print("Fan: ");
      auto update = [&]() {
        display_->SetCursor(5, 1);
        if (clock_->millisSince(flash_ms) > 500) {
          flash_state = !flash_state;
          flash_ms = millis();
        }

        // Print the Fan On/Off status.
        if (field == 0 && flash_state == true) {
          display_->Print("___ ");
        } else {
          display_->Print(fan ? "ON  " : "EXT:");
        }

        // Print the extended fan time.
        //
        // When the fan is always on, hide the extended minutes.
        if (fan == true) {
          display_->Print("    ");
        } else if (field == 1 && flash_state == true) {
          display_->Print("___m");
        } else {
          if (fan_extend_mins < 100) {
            display_->Print("0");
          }
          if (fan_extend_mins < 10) {
            display_->Print("0");
          }
          display_->Print(fan_extend_mins);
          display_->Print("m");
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
              SetChanged(settings_);
              break;
            }
            if (field == 1) {
              increment = -1;
              // Fall through.
            }
          case Button::RIGHT:
            // Only allow changing the extended fan time if the fan setting is set to auto
            // (false).
            if ((field == 0 && fan == false) || (field == 1)) {
              field = (field + 1) % kTotalFields;
            }
            break;
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
          case Button::SELECT:
            // Update the settings data.
            settings_->persisted.fan_always_on = fan;
            settings_->persisted.fan_extend_mins = fan_extend_mins;
            SetChangedAndPersist(settings_);

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

    Button SetSaveAllSettingsForDebug() {
      // Setup the display
      ResetLine();
      display_->Print("Save for debug?");

      while (true) {
        const Button button = wait_for_button_press_(10000);

        // Exit when we get a left arrow in the edit menu.
        switch (button) {
          case Button::SELECT:
            SaveAllSettingsForDebug(settings_);
            PrintUpdatedAndWait();
            return Button::NONE;
            break;
          default:
            return button;
        }
      }
    }
    Button SetPrintAllAllSettingsForDebug() {
      // Setup the display
      ResetLine();
      display_->Print("Print debug settings?");

      while (true) {
        const Button button = wait_for_button_press_(10000);

        // Exit when we get a left arrow in the edit menu.
        switch (button) {
          case Button::SELECT:
            OutputAllSettingsForDebug();
            ResetLine();
            display_->Print("See serial output...");
            wait_for_button_press_(1000);
            return Button::NONE;
            break;
          default:
            return button;
        }
      }
    }

    Button SetMode() {
      uint8_t mode =
        settings_->persisted.heat_enabled << 1 | settings_->persisted.cool_enabled;

      // Setup the display
      ResetLine();
      display_->Print("Mode: ");

      // Draw the value.
      auto update = [&]() {
        display_->SetCursor(5, 1);
        display_->Print(mode == 3 ? "BOTH" : mode == 2 ? "HEAT" : mode == 1 ? "COOL" : "OFF ");
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
            SetChangedAndPersist(settings_);

            PrintUpdatedAndWait();
            return Button::NONE;
            break;
          default:
            return button;
        }
        update();
      }
    }

    Button SetDate() {
      uint8_t field = 0;
      Date date = clock_->Now();
      uint32_t flash_ms = millis();
      bool flash_state = false;

      ResetLine();
      display_->Print("Date: ");
      auto update = [&]() {
        display_->SetCursor(6, 1);
        if (clock_->millisSince(flash_ms) > 500) {
          flash_state = !flash_state;
          flash_ms = millis();
        }

        // 00:00 Mo
        if (field == 0 && flash_state == true) {
          display_->Print("__");
        } else {
          if (date.hour < 10) {
            display_->Print("0");
          }
          display_->Print(date.hour);
        }

        display_->Print(":");

        if (field == 1 && flash_state == true) {
          display_->Print("__");
        } else {
          if (date.minute < 10) {
            display_->Print("0");
          }
          display_->Print(date.minute);
        }
        display_->Write(' ');

        if (field == 2 && flash_state == true) {
          display_->Print("__");
        } else {
          display_->Print(daysOfTheWeek[date.day_of_week]);
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
          Serial.print("Timer: ");
          if (timeout_counter < 20) {
            Serial.print("Setting to none ");
            button = Button::NONE;
          }
          Serial.println(timeout_counter);
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
            flash_ms = millis();

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
            SetChangedAndPersist(settings_);
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
      uint32_t flash_ms = millis();
      bool flash_state = false;

      ResetLine();
      display_->Print((mode == Mode::HEAT) ? "H" : "C");
      auto update = [&]() {
        display_->SetCursor(1, 1);
        display_->Print(setpoint + 1);
        display_->Print(":");

        if (clock_->millisSince(flash_ms) > 500) {
          flash_state = !flash_state;
          flash_ms = millis();
        }

        // 00.0^ 00:00
        if (field == 0 && flash_state == true) {
          display_->Print("__");
        } else {
          if (temperature_x10 / 10 < 10) {
            display_->Write('0');
          }
          display_->Print(temperature_x10 / 10);
        }

        display_->Write('.');

        if (field == 0 && flash_state == true) {
          display_->Print("_");
        } else {
          display_->Print(temperature_x10 % 10);
        }
        // Print custom degree symbol.
        display_->Write(byte(0));

        display_->Write(' ');
        if (field == 1 && flash_state == true) {
          display_->Print("__");
        } else {
          if (hour < 10) {
            display_->Print("0");
          }
          display_->Print(hour);
        }
        display_->Print(":");
        if (field == 2 && flash_state == true) {
          display_->Print("__");
        } else {
          if (minute < 10) {
            display_->Print("0");
          }
          display_->Print(minute);
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
          Serial.print("Timer: ");
          if (timeout_counter < 20) {
            Serial.print("Setting to none ");
            button = Button::NONE;
          }
          Serial.println(timeout_counter);
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
            flash_ms = millis();

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
            SetChangedAndPersist(settings_);
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

    Button SetTolerance() {
      int val = settings_->persisted.tolerance_x10;
      uint32_t flash_ms = millis();
      bool flash_state = false;

      ResetLine();
      display_->Print("Tolerance: ");
      // Updates the display for the changed fields.
      auto update = [&]() {
        display_->SetCursor(11, 1);

        if (clock_->millisSince(flash_ms) > 500) {
          flash_state = !flash_state;
          flash_ms = millis();
        }

        // Tolerance: 00.0^
        if (flash_state == true) {
          display_->Print("__._");
        } else {
          if (val / 10 < 10) {
            display_->Print("0");
          }
          display_->Print(val / 10);
          display_->Print(".");
          display_->Print(val % 10);
        }
        // Print custom degree symbol.
        display_->Write(byte(0));
      };

      update();

      int timeout_counter = 0;

      while (true) {
        // Only after 10 seconds of timeouts do we pass the timeout button.
        Button button = wait_for_button_press_(500);
        if (button == Button::TIMEOUT) {
          timeout_counter++;
          Serial.print("Timer: ");
          if (timeout_counter < 20) {
            Serial.print("Setting to none ");
            button = Button::NONE;
          }
          Serial.println(timeout_counter);
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
            flash_ms = millis();

            val += increment;
            break;
          case Button::SELECT:
            // Update the settings data.
            settings_->persisted.tolerance_x10 = val;
            SetChangedAndPersist(settings_);
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
      uint32_t flash_ms = millis();
      bool flash_state = false;

      ResetLine();
      display_->Print("Humidify: ");
      // Updates the display for the changed fields.
      auto update = [&]() {
        display_->SetCursor(10, 1);

        if (clock_->millisSince(flash_ms) > 500) {
          flash_state = !flash_state;
          flash_ms = millis();
        }

        // Tolerance: 00%
        if (flash_state == true) {
          display_->Print("__");
        } else {
          if (val < 10) {
            display_->Print("0");
          }
          display_->Print(val);
        }
        // Print custom degree symbol.
        display_->Write('%');
      };

      update();

      int timeout_counter = 0;

      while (true) {
        // Only after 10 seconds of timeouts do we pass the timeout button.
        Button button = wait_for_button_press_(500);
        if (button == Button::TIMEOUT) {
          timeout_counter++;
          Serial.print("Timer: ");
          if (timeout_counter < 20) {
            Serial.print("Setting to none ");
            button = Button::NONE;
          }
          Serial.println(timeout_counter);
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
            flash_ms = millis();

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
            SetChangedAndPersist(settings_);
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
        display_->Print("Time: ");
        if (hours < 10) {
          display_->Print("0");
        }
        display_->Print(hours);
        display_->Print(":");
        if (minutes < 10) {
          display_->Print("0");
        }
        display_->Print(minutes);

        display_->Print("      ");

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
      const Date date = clock_->Now();
      while (true) {
        display_->SetCursor(0, 1);
        display_->Print("Override: ");
        // print the number of seconds since reset:
        display_->SetCursor(10, 1);
        int temp = GetOverrideTemp(*settings_);
        display_->Print(static_cast<int>(temp) / 10);
        display_->Print(".");
        display_->Print(static_cast<int>(temp) % 10);
        display_->Write(byte(0));
        display_->Write(' ');

        const Button button = wait_for_button_press_(10000);
        switch (button) {
          case Button::UP:
            SetOverrideTemp(1, settings_, clock_->Millis(), date);
            continue;
          case Button::DOWN:
            SetOverrideTemp(-1, settings_, clock_->Millis(), date);
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
      display_->Print("                ");
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
      display_->Print("Updated...");
      wait_for_button_press_(1000);
    }

    Settings *settings_;
    WaitForButtonPressFn wait_for_button_press_;
    Clock *clock_;
    Display *display_;
};

}
#endif  // MENUS_H_
