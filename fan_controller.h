#ifndef FAN_CONTROLLER_H_
#define FAN_CONTROLLER_H_
# This helper manages the HVAC fan control for extending circulation and periodically running the fan even when HVAC is off.

#include "settings.h"

namespace thermostat {

// Length of time to extend the fan after HVAC ends.
constexpr int kExtendFanRunMinutes = 5;

// Determines when the fan should be running based on the events structure.
//
// This allows the fan to run for 5 minutes for better room balancing after the HVAC stops
// running.
class FanController {
  public:    
    void Maintain(Settings* const settings, const uint32_t now) {
      // Get the persisted fan setting.
      bool fan_enable = settings->persisted.fan_always_on;

      // Allow the fan to run 30 minutes every 3 hours unless heating and cooling are in effect.
      constexpr uint32_t fan_period = Clock::HoursToMillis(3);
      constexpr uint32_t fan_duration = Clock::MinutesToMillis(30);
      
      const uint32_t last_on_diff = Clock::millisDiff(last_fan_on_time, now);
      if (last_on_diff > Clock::MinutesToMillis(fan_period) && last_on_diff < Clock::MinutesToMillis(fan_period+fan_duration)) {
        fan_enable = true;
      }
      
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
        if (Clock::MinutesDiff(curr_event->start_time, now) < settings->persisted.fan_extend_mins) {
          fan_enable = true;
        }
      }

      if (settings->fan_running && !fan_enable) {
        last_fan_on_time = now;
      }

      // Store the new setting.
      settings->fan_running = fan_enable;
    }
  private:
    uint32_t last_fan_on_time = 0;
    
};
}
#endif  // FAN_CONTROLLER_H_
