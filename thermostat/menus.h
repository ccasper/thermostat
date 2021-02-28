#ifndef MENUS_H_
#define MENUS_H_
// This header implements useful menus to allow the user to change settings and view statuses.

#include "buttons.h"
#include "events.h"
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

//using WaitForButtonPressFn = std::function<Button(uint32_t timeout)>;
using WaitForButtonPressFn = Button (*)(uint32_t timeout);
using SettingFn = Button (*)();
using GetDateFn = Date (*)();

// Set changed, and update the EEPROM.
static void SetChangedAndPersist(Settings *settings, SettingsStorer *writer) {
  settings->changed = true;
  writer->Write(*settings);
};

// Set changed, but don't update the EEPROM.
static void SetChanged(Settings* settings) {
  settings->changed = true;
};

// Helper for handling flashing behavior of a field.
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
      if (value_ < min_) { 
        value_ = min_;
      }
      if (value > max_) {
        value_ = max_;
      }
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
      constexpr uint8_t MENU_MAX = 9;

      while (true) {
        ResetLine();
        Button button;
        display_->SetCursor(0, 1);
        switch (menu_index) {
          case 0:
            //1234567890123456
            //H:00 C:00 F:00 %
            {
              display_->print("H:");
              const uint32_t window = cmin(OldestEventStart(*settings_, *clock_), Clock::HoursToMillis(24));
              const uint32_t heat = CalculateSeconds(HvacMode::HEAT, *settings_, window, *clock_);
              const int ratio = cmin(heat * 100 / Clock::MillisToSeconds(window), 99UL);
              if (ratio < 10) {
                display_->write('0');
              }
              display_->print(ratio);
            }
            {
              display_->print(" C:");
              const uint32_t window = cmin(OldestEventStart(*settings_, *clock_), Clock::HoursToMillis(24));
              const uint32_t cool = CalculateSeconds(HvacMode::COOL, *settings_, window, *clock_);
              const int ratio = cmin(cool * 100 / Clock::MillisToSeconds(window), 99UL);
              if (ratio < 10) {
                display_->write('0');
              }
              display_->print(ratio);
            }
            {
              display_->print(" F:");
              const uint32_t window = cmin(OldestEventStart(*settings_, *clock_), Clock::HoursToMillis(24));
              const uint32_t fan = CalculateSeconds(FanMode::ON, *settings_, window, *clock_);
              const int ratio = cmin(fan * 100 / Clock::MillisToSeconds(window), 99UL);
              if (ratio < 10) {
                display_->write('0');
              }
              display_->print(ratio);
            }
            display_->print('%');

            button = wait_for_button_press_(10000);
            if (button == Button::UP) {
              // 1234567890123456
              // c st:XXXXXm SF
              //   c = 'A' + index
              //   XXXXXX = millis/1000/60
              //   S = H/C/I
              //   F = F/I
              // c du:XXXXXm SF
              const uint32_t now = clock_->Millis();

              // Loop through all the stored events.
              for (int idx = 0; idx < EVENT_SIZE; ++idx) {
                const uint32_t duration_ms = CalculateDurationSinceTime(
                                               now - Clock::HoursToMillis(24),
                                               settings_->events[idx].start_time,
                                               GetEventDuration(idx, *settings_, now));

                // Only sum events that valid and have a duration.
                if (duration_ms == 0) {
                  continue;
                }

                ResetLine();
                display_->SetCursor(0, 1);
                display_->write('A' + idx);
                display_->print(" st:");
                display_->print(settings_->events[idx].start_time / 1000 / 60);
                display_->print("m ");
                display_->print(settings_->events[idx].fan == FanMode::ON ? "F" : "I");
                display_->print(settings_->events[idx].hvac == HvacMode::COOL ? "C" : settings_->events[idx].hvac == HvacMode::HEAT ? "H" : "I");
                wait_for_button_press_(1000);
                ResetLine();
                display_->SetCursor(0, 1);
                display_->write('A' + idx);
                display_->print(" du:");
                display_->print(duration_ms / 1000 / 60);
                display_->print("m ");
                display_->print(settings_->events[idx].fan == FanMode::ON ? "F" : "I");
                display_->print(settings_->events[idx].hvac == HvacMode::COOL ? "C" : settings_->events[idx].hvac == HvacMode::HEAT ? "H" : "I");
                wait_for_button_press_(1000);
              }
            }
            break;
          case 1:
            display_->print("BME Temp: ");
            display_->print(settings_->current_bme_temperature_x10);
            button = wait_for_button_press_(10000);
            break;
          case 2:
            display_->print("Dal Temp: ");
            display_->print(settings_->current_temperature_x10);
            button = wait_for_button_press_(10000);
            break;
          case 3:
            display_->print("IAQ: ");
            display_->print(settings_->air_quality_score);
            button = wait_for_button_press_(10000);
            break;
          case 4:
            display_->print("Heat T/m: ");

            display_->print(GetHeatTempPerMin(*settings_, *clock_));
            button = wait_for_button_press_(10000);
            break;
          case 5:
            {
              display_->print("H s: ");
              const uint32_t heat = CalculateSeconds(HvacMode::HEAT, *settings_, Clock::HoursToMillis(24), *clock_);
              display_->print(heat);
              button = wait_for_button_press_(10000);
              break;
            }
          case 6:
            {
              display_->print("F s.: ");
              const uint32_t fan = CalculateSeconds(FanMode::ON, *settings_, Clock::HoursToMillis(24), *clock_);

              display_->print(fan);
              button = wait_for_button_press_(10000);
              break;
            }
          case 8:
            {
              display_->print("Heat Rise: ");

              display_->print(HeatRise(*settings_, *clock_));
              button = wait_for_button_press_(10000);
              break;
            }
          case 7:
            {
              display_->print("Out T: ");

              display_->print(OutdoorTemperatureEstimate(*settings_, *clock_));
              button = wait_for_button_press_(10000);
              break;
            }
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
      constexpr uint8_t kMenuMax = 9;

      while (true) {
        Button button;
        switch (menu_index) {
          case 0:
            button = SetFan();
            break;
          case 1:
            button = SetMode();
            break;
          case 2:
            button = SetSetpoint(0, HvacMode::HEAT);
            break;
          case 3:
            button = SetSetpoint(1, HvacMode::HEAT);
            break;
          case 4:
            button = SetSetpoint(0, HvacMode::COOL);
            break;
          case 5:
            button = SetSetpoint(1, HvacMode::COOL);
            break;
          case 6:
            button = SetTolerance();
            break;
          case 7:
            button = SetDate();
            break;
          case 8:
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

      Flasher flasher(clock_);
      Waiter waiter(&wait_for_button_press_);
      Digit mins = Digit(settings_->persisted.fan_extend_mins, 1, 999, false, "m", display_, &flasher);

      // Determine the initial state.
      uint8_t fan_state = 0; // 0=OFF, 1=ON, 2=EXT
      if (fan) {
        fan_state = 0;
      } else if (settings_->persisted.fan_extend_mins == 0) {
        fan_state = 1;
      } else {
        fan_state = 2;
      }
      constexpr uint8_t kTotalFanStates = 3;

      uint8_t field = 0;
      constexpr uint8_t kTotalFields = 2;

      // Setup the display
      // "Fan:ON     "
      // "Fan:OFF    "
      // "Fan:EXT:060m"
      ResetLine();
      display_->print("Fan:");
      auto update = [&]() {
        display_->SetCursor(4, 1);
        // Print the Fan On/Off status.
        if (field == 0 && flasher.State()) {
          display_->print("___ ");
        } else {
          display_->print(fan_state == 0 ? "ON  " : fan_state == 1 ? "OFF " : "EXT:");
        }

        // Print the extended fan time.
        //
        // When the fan is in ext mode, show the extend minutes.
        if (fan_state == 2) {
          mins.Print(field == 1);
        } else {
          display_->print("    ");
        }
      };
      // First see if the user wants to change this item.
      update();
      Button button = WaitBeforeEdit();
      if (button != Button::SELECT) {
        return button;
      }

      while (true) {
        Button button = waiter.Wait();

        int increment = 1;
        switch (button) {
          case Button::DOWN:
            increment = -1;
          /* FALLTHRU */
          case Button::UP:
            if (field == 0) {
              if (fan_state == 0 && increment < 0) {
                fan_state = kTotalFanStates - 1;
              } else {
                fan_state = (fan_state + increment) % kTotalFanStates;
              }

              // Change the fan temporarily. This makes it easer to perform special fan
              // speed changes.
              switch (fan_state) {
                // Fan On.
                case 0:
                  settings_->persisted.fan_always_on = true;
                  break;
                // Fan Off.
                case 1:
                  settings_->persisted.fan_always_on = false;
                  settings_->persisted.fan_extend_mins = 0;
                  break;
                // Fan Off with extend.
                case 2:
                  settings_->persisted.fan_always_on = false;
                  settings_->persisted.fan_extend_mins = mins.Value();
                  break;
              }
              SetChanged(settings_);
            }
            mins.Increment(field == 1, increment);
            break;
          case Button::RIGHT:
            // Allow changing the field if extended fan time is selected or already editing the fan time.
            if ((field == 0 && fan_state == 2) || (field == 1)) {
              field = (field + 1) % kTotalFields;
            }
            break;
          case Button::SELECT:
            // Update the settings data.
            switch (fan_state) {
              // Fan On.
              case 0:
                settings_->persisted.fan_always_on = true;
                break;
              // Fan Off.
              case 1:
                settings_->persisted.fan_always_on = false;
                settings_->persisted.fan_extend_mins = 0;
                break;
              // Fan Off with extend.
              case 2:
                settings_->persisted.fan_always_on = false;
                settings_->persisted.fan_extend_mins = mins.Value();
                break;
            }
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

      Flasher flasher(clock_);
      Waiter waiter(&wait_for_button_press_);
      Digit mode = Digit(
                          settings_->persisted.heat_enabled << 1 | settings_->persisted.cool_enabled,
                          0, 3, false, "", display_, &flasher);

      // Setup the display
      ResetLine();
      display_->print("Mode: ");
      // Draw the value.
      auto update = [&]() {
        display_->SetCursor(5, 1);
        if (flasher.State()) {
          display_->print("____");
          return;
        }
        display_->print(mode.Value() == 3 ? "BOTH" : mode.Value() == 2 ? "HEAT" : mode.Value() == 1 ? "COOL" : "OFF ");
      };

      // First see if the user wants to change this item.
      update();
      Button button = WaitBeforeEdit();
      if (button != Button::SELECT) {
        return button;
      }

      while (true) {
        Button button = waiter.Wait();

        // Exit when we get a left arrow in the edit menu.
        int increment = 1;
        switch (button) {
          case Button::DOWN:
            increment = -1;
          case Button::UP:
            mode.Increment(true, increment);
            break;
          case Button::SELECT:
            // Update the settings data.
            settings_->persisted.heat_enabled = mode.Value() & 0x02 ? true : false;
            settings_->persisted.cool_enabled = mode.Value() & 0x01 ? true : false;
            SetChangedAndPersist(settings_, storer_);

            PrintUpdatedAndWait();
            return Button::NONE;
            break;
          case Button::NONE:
            // Do nothing.
            break;
          default:
            return button;
        }
        update();
      }
    }

    Button SetDate() {
      uint8_t field = 0;

      Flasher flasher(clock_);
      Waiter waiter(&wait_for_button_press_);

      Date date = clock_->Now();
      Digit hrs = Digit(date.hour, 0, 23, false, ":", display_, &flasher);
      Digit mins = Digit(date.minute, 0, 59, false, "", display_, &flasher);
      Digit dow = Digit(date.day_of_week, 0, 6, false, "", display_, &flasher);

      ResetLine();
      display_->print("Date: ");
      auto update = [&]() {
        display_->SetCursor(6, 1);
        // 00:00 Mo
        hrs.Print(field == 0);
        mins.Print(field == 1);

        display_->write(' ');

        if (field == 2 && flasher.State()) {
          display_->print("__");
        } else {
          display_->print(daysOfTheWeek[dow.Value()]);
        }
      };

      // First see if the user wants to change this item.
      update();
      Button button = WaitBeforeEdit();
      if (button != Button::SELECT) {
        return button;
      }

      while (true) {
        Button button = waiter.Wait();

        // Exit when we get a left arrow in the edit menu.
        int increment = 1;
        switch (button) {
          case Button::LEFT:
            // Left is the exit button.
            return button;
          case Button::RIGHT:
            field = (field + 1) % 3;
            flasher.Underline();
            break;
          case Button::DOWN:
            increment = -1;
          // Fall through.
          case Button::UP:
            // Stop flashing while updating.
            hrs.Increment(field == 0, increment);
            mins.Increment(field == 1, increment);
            dow.Increment(field == 2, increment);
            break;
          case Button::SELECT:
            // Update the settings data.
            date.day_of_week = dow.Value();
            date.minute = mins.Value();
            date.hour = hrs.Value();
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

    Button SetSetpoint(const uint8_t setpoint, const HvacMode mode) {
      constexpr uint8_t kMaxFields = 3;
      uint8_t field = 0;
      // 1234567890123456
      // H1: XX.X° XX:XX
      Flasher flasher(clock_);
      Waiter waiter(&wait_for_button_press_);
      Digit temp = Digit((mode == HvacMode::HEAT)
                         ? settings_->persisted.heat_setpoints[setpoint].temperature_x10
                         : settings_->persisted.cool_setpoints[setpoint].temperature_x10, 0, 999, true, "", display_, &flasher);
      Digit hrs = Digit((mode == HvacMode::HEAT) ? settings_->persisted.heat_setpoints[setpoint].hour
                        : settings_->persisted.cool_setpoints[setpoint].hour, 0, 23, false, ":", display_, &flasher);
      Digit mins = Digit((mode == HvacMode::HEAT)
                         ? settings_->persisted.heat_setpoints[setpoint].minute
                         : settings_->persisted.cool_setpoints[setpoint].minute, 0, 59, false, "", display_, &flasher);

      ResetLine();
      display_->print((mode == HvacMode::HEAT) ? "H" : "C");
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
            if (mode == HvacMode::HEAT) {
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
      Flasher flasher(clock_);
      Waiter waiter(&wait_for_button_press_);
      Digit val = Digit(settings_->persisted.tolerance_x10, 1, 99, true, "", display_, &flasher);

      ResetLine();
      display_->print("Tolerance: ");
      // Updates the display for the changed fields.
      auto update = [&]() {
        display_->SetCursor(11, 1);
        val.Print(true);
        // Print custom degree symbol.
        display_->write(0);
      };

      // First see if the user wants to change this item.
      update();
      Button button = WaitBeforeEdit();
      if (button != Button::SELECT) {
        return button;
      }

      while (true) {
       Button button = waiter.Wait();

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
            val.Increment(true, increment);
            break;
          case Button::SELECT:
            // Update the settings data.
            settings_->persisted.tolerance_x10 = val.Value();
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
      // If override is already set, clear it and return.
      if (IsOverrideTempActive(*settings_)) {
        ClearOverrideTemp(settings_);
        ResetLine();
        display_->print("Override cleared");
        wait_for_button_press_(1000);
        return Button::NONE;
      }
      Flasher flasher(clock_);
      Waiter waiter(&wait_for_button_press_);
      Digit temp = Digit(GetOverrideTemp(*settings_), 400, 999, true, "", display_, &flasher);

      ResetLine();
      display_->print("Override: ");
      // Updates the display for the changed fields.
      auto update = [&]() {
        display_->SetCursor(10, 1);
        temp.Print(true);
        // Print custom degree symbol.
        display_->write(0);
      };

      while (true) {
       update();
       Button button = waiter.Wait();

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
            temp.Increment(true, increment);
            break;
          case Button::SELECT:
            // Update the settings data.
            SetOverrideTemp(temp.Value(), settings_, clock_->Millis());
            PrintUpdatedAndWait();
            return Button::NONE;
            break;
          case Button::NONE:
            // An update for flashing behavior occurred.
            break;
          default:
            return button;
        }
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
