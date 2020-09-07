#ifndef FAN_CONTROLLER_H_
#define FAN_CONTROLLER_H_
// This helper manages the HVAC fan control for extending circulation and periodically running the fan even when HVAC is off.

#include "settings.h"
#include "comparison.h"

namespace thermostat {

// Determines when the fan should be running based on the events structure.
//
// This allows the fan to run for 5 minutes for better room balancing after the HVAC stops
// running.
class FanController {
  public:
    explicit FanController(Clock* clock) : clock_(clock), last_maintain_time_(clock->Millis()) {}
    
    void Maintain(Settings* const settings) {
      // Keep the last_hvac_on time set.
      if (settings->GetHvacMode() == HvacMode::HEAT || settings->GetHvacMode() == HvacMode::COOL) {
        last_hvac_on_set_ = true;
        last_hvac_on_ = clock_->Millis();
      }
      
      // Get the persisted fan setting.
      bool fan_enable = settings->persisted.fan_always_on;
      
      // Allow the fan to run 30 minutes every X minutes.
      const uint32_t fan_period_sec = static_cast<uint32_t>(settings->persisted.fan_on_min_period) * 60;

      // Keep running the fan for extended minutes after a heat/cool cycle.
      if (last_hvac_on_set_ && clock_->minutesSince(last_hvac_on_) < settings->persisted.fan_extend_mins) {
        //LOG(INFO) << "Extended fan on";
        fan_enable = true;
      }

      const bool fan_is_running = settings->fan_running;
      if (fan_is_running) {
        const uint32_t subtract_seconds = clock_->secondsSince(last_maintain_time_) / (settings->persisted.fan_on_duty / 100.0);
        cycle_seconds -= subtract_seconds;
        //LOG(INFO) << "Subtract Seconds: " << subtract_seconds << " seconds since: " << clock_->secondsSince(last_maintain_time_) << " Duty: " << (settings->persisted.fan_on_duty / 100.0);
      } else {
        //Add fan off time.
        const uint32_t add_seconds = clock_->secondsSince(last_maintain_time_);
        cycle_seconds += add_seconds;
      }

      // Bound the range of cycle_seconds.
      if (cycle_seconds < -1.0 * fan_period_sec) {
        //LOG(INFO) << "SIF Cycle seconds: " << cycle_seconds;
        cycle_seconds = -1 * fan_period_sec;
      }
      // Apply upper bound and turn on fan if we hit the limit.
      if (cycle_seconds >= fan_period_sec) {
        //LOG(INFO) << "Cycle Period fan on";
        cycle_seconds = fan_period_sec;
        fan_enable = true;
      }

      last_maintain_time_= clock_->Millis();

      //LOG(INFO) << "Cycle seconds: " << cycle_seconds;
      
      // Keep the fan running until we meet the desired duty cycle run length.
      if (fan_is_running && cycle_seconds > 0) {
        //LOG(INFO) << "Cycle Duty fan on";
        fan_enable = true;
      }

      // Store the new setting.
      settings->fan_running = fan_enable;
    }
  private:
    // Ensures the fan meets the fan running duty cycle. Each time the fan runs, the fan will continue running until cycle_seconds returns to zero.
    //
    // We increment for each second the fan is off and decrement (1 second / duty%) for every second fan on.
    // Each time the hvac runs, we run until this counter reaches zero. To handle hvac off cases, when the cycle counter reaches the cycle period, the fan will be forced on.
    Clock* clock_;
    
    float cycle_seconds = 0;
    
    uint32_t last_maintain_time_ = 0;
    bool last_hvac_on_set_ = false;
    uint32_t last_hvac_on_ = 0;
};

}
#endif  // FAN_CONTROLLER_H_
