#ifndef FAN_CONTROLLER_H_
#define FAN_CONTROLLER_H_

#include "settings.h"

// Length of time to extend the fan after HVAC ends.
constexpr int kExtendFanRunMinutes = 5;

// Determines when the fan should be running based on the events structure.
//
// This allows the fan to run for 5 minutes for better room balancing after the HVAC stops
// running.
class FanController {
  public:
    void UpdateSetting(Settings* const settings) {
      // Get the persisted fan setting.
      bool fan_enable = settings->persisted.fan_always_on;

      // Allow extending the fan after a heat/cool cycle.
      Event* curr_event;
      Event* prev_event;
      {
        int current_idx = settings->CurrentEventIndex();

        int prev_idx = settings->PrevEventIndex(current_idx);

        if (current_idx == -1 || prev_idx == -1) {
          // Store the current setting.
          settings->fan_running = fan_enable;
          return;
        }

        curr_event = &settings->events[current_idx];
        prev_event = &settings->events[prev_idx];
      }

      // Keep running the fan for extended minutes after a heat/cool cycle.
      if (prev_event->heat || prev_event->cool) {
        if (minutesSince(curr_event->start_time) < settings->persisted.fan_extend_mins) {
          fan_enable = true;
        }
      }

      // Store the new setting.
      settings->fan_running = fan_enable;
    }
};

#endif  // FAN_CONTROLLER_H_
