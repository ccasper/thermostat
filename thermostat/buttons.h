#ifndef BUTTONS_H_
#define BUTTONS_H_
// This header converts analog resistor changes into button presses, debounces them, and allows for automatic presses when a button is held down.

#include "interfaces.h"

namespace thermostat {

enum class Button { NONE, SELECT, RIGHT, LEFT, UP, DOWN, TIMEOUT };

// Helper for managing the 1602 shield analog buttons. This performs button debouncing and
// enables two levels of automatic button presses when a button is held longer than a few
// seconds to allow quicker cycling through the values.
class Buttons {
  public:
    // Returns the button associated with the analog input specified based on the resistive level.
    static Button GetButton(const int16_t analogValue) {
      if (analogValue < 100) {
        return Button::RIGHT;
      }
      if (analogValue < 200) {
        return Button::UP;
      }
      if (analogValue < 400) {
        return Button::DOWN;
      }
      if (analogValue < 600) {
        return Button::LEFT;
      }
      if (analogValue < 800) {
        return Button::SELECT;
      }
      return Button::NONE;
    }

    // Stabilizes the current pressed button with a debouncing window.
    static Button StabilizedButtonPressed(const Button button) {
      static Button window[3];
      static int8_t index = 0;
      static Button active = Button::NONE;

      // Round robin through the debouncing window .
      index = (index + 1) % 3;
      window[index] = button;

      // When all the hysteresis values align, we record the press.
      if (window[0] != window[1]) {
        return active;
      }
      if (window[1] != window[2]) {
        return active;
      }

      if (window[0] != active) {
        active = window[0];
      }

      return active;
    }

    // Returns a single press event per button down and starts 4Hz auto-presses after
    // holding 1 second and excalates to 40Hz auto-presses after 5 seconds.
    //
    // The user specified button argument should be stabilized with hysteresis.
    static Button GetSinglePress(const Button button, const uint32_t now) {
      static Button active = Button::NONE;
      static uint32_t started_at_ms = now;
      static uint32_t held_counter = 0;

      // The active button has changed.
      if (button != active) {
        started_at_ms = now;
        held_counter = 0;
        active = button;
        return active;
      }

      // Return a button press at 40Hz it's held after 5 seconds.
      const uint32_t fourth_counts = Clock::millisDiff(started_at_ms, now) / 250;
      // Auto press at a 25ms rate after holding for 5 seconds.
      if (fourth_counts >= 4 /*counts per second*/ * 5) {
        const uint32_t fortieth_counts = Clock::millisDiff(started_at_ms, now) / 25;
        if (fortieth_counts > held_counter) {
          held_counter = fortieth_counts;
          return active;
        }
        // Return, we shouldn't fall into the slower auto-press mode.
        return Button::NONE;
      }

      // Auto press at 4 Hz after holding for 1 second.
      if (fourth_counts >= 4 /*counts per second*/ * 1) {
        if (fourth_counts > held_counter) {
          held_counter = fourth_counts;
          return active;
        }
        return Button::NONE;
      }

      return Button::NONE;
    }

    // Returns a human readable button character for serial output debugging.
    static char GetButtonName(const Button button) {
      switch (button) {
        case Button::SELECT:
          return 'S';
        case Button::UP:
          return 'U';
        case Button::DOWN:
          return 'D';
        case Button::LEFT:
          return 'L';
        case Button::RIGHT:
          return 'R';
        default:
          return '_';
      }
    }

};

}
#endif  // BUTTONS_H_
