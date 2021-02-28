#ifndef EVENTS_H_
#define EVENTS_H_
// Helpers to manage the Events struct for performing calculations and creating new events
// whena applicable.

#include "settings.h"

namespace thermostat {

constexpr float kTenMinuteAdjustmentMins = 9.5;
constexpr uint32_t kEventHorizon = Clock::DaysToMillis(1);

static HvacMode Sanitize(const HvacMode mode) {
  if (mode == HvacMode::HEAT || mode == HvacMode::COOL) {
    return mode;
  }
  return HvacMode::IDLE;
}

static FanMode Sanitize(const FanMode mode) {
  if (mode == FanMode::ON) {
    return mode;
  }
  return FanMode::OFF;
}

static float GetHeatTempPerMin(const Settings& settings, const Clock& clock) {
  const uint32_t now = clock.Millis();
  // Find the average 10 minute temperature difference when heating (in the last 2 days).
  uint8_t count = 0;
  uint32_t sum = 0;
  // Find any events in the last 2 days that have heat and 10 minute temperatures.
  for (uint8_t i = 0; i < EVENT_SIZE; ++i) {
    const Event* const event = &settings.events[i];

    if (event->empty()) {
      continue;
    }

    if (Clock::MillisDiff(event->start_time, now) > kEventHorizon) {
      continue;
    }

    if (event->hvac != HvacMode::HEAT) {
      continue;
    }
    if (event->temperature_10min_x10 == 0) {
      continue;
    }
    count++;
    sum += (event->temperature_10min_x10 - event->temperature_x10);
  }

  // Use this heat/time as the basis for the temperature.
  // Complete guesses atm.
  // if ~ 1*/min set to 15%
  // if ~ 2*/min set to 25%
  // if ~ 3*/min set to 35%

  return static_cast<float>(sum) / /*x10*/ 10.0 / count /
         kTenMinuteAdjustmentMins /*mins (30 seconds? for heater warmup)*/;
}

// Returns Zero when empty, otherwise the length of time for the event.
static uint32_t GetEventDuration(const uint8_t index, const Settings& settings,
                                 const uint32_t now) {

  // If the event is empty, we don't have a duration.
  if (index >= EVENT_SIZE) {
    return 0;
  }

  if (settings.events[index].empty()) {
    return 0;
  }

  const uint8_t next_index = (index + 1) % EVENT_SIZE;
  // When we don't have an event after this, then we use the current time.
  if (settings.events[next_index].empty()) {
    return Clock::MillisDiff(settings.events[index].start_time, now);
  }
  return Clock::MillisDiff(settings.events[index].start_time,
                           settings.events[next_index].start_time);
}

//// Returns true if found and value passed back in diff parameter.
// static bool GetEventTempDiff(const uint8_t index, const Settings& settings,
//                      const uint32_t now, int* temperature_diff_x10) {
//  // If the event is empty, we don't have a duration.
//  if (settings.events[index].empty) {
//    return 0;
//  }
//
//  // When we don't have an event after this, then we use the current time.
//  if (settings.events[(index + 1) % EVENT_SIZE].empty) {
//    return Clock::MillisDiff(settings.events[index].start_time, now);
//  }
//  return Clock::MillisDiff(settings.events[index].start_time,
//                    settings.events[(index + 1) % EVENT_SIZE].start_time);
//}

// This checks if we should be in a 5 minute lockout when switching from cooling to
// heating or heating to cooling.
static bool IsInLockoutMode(const HvacMode mode, const Event* events, const uint32_t now) {
  constexpr uint32_t kLockoutMs = 5UL * 60UL * 1000UL;
  uint8_t index = 0;
  uint32_t millis_since = 0xFFFFFFFF;  // some big number

  // Find the newest event.
  for (int i = 0; i < EVENT_SIZE; ++i) {
    if (events[i].empty()) {
      continue;
    }
    if (Clock::MillisDiff(events[i].start_time, now) < millis_since) {
      millis_since = Clock::MillisDiff(events[i].start_time, now);
      index = i;
    }
  }

  // Are there any events found?
  if (millis_since == 0xFFFFFFFF) {
    return false;
  }
  // If the current event is greater than we're out of the window.
  if (millis_since > kLockoutMs) {
    return false;
  }

  // Are we actively heating or cooling?
  if (mode == HvacMode::COOL && millis_since < kLockoutMs &&
      events[index].hvac == HvacMode::HEAT) {
    return true;
  }
  if (mode == HvacMode::HEAT && millis_since < kLockoutMs &&
      events[index].hvac == HvacMode::COOL) {
    return true;
  }

  // Keep looking back until we're beyond the window..
  while (true) {
    index = (index - 1) % EVENT_SIZE;
    // The previous event is empty.
    if (events[index].empty()) {
      return false;
    }

    // We use the started time of the newer event to know when it stopped.
    if (mode == HvacMode::COOL && events[index].hvac == HvacMode::HEAT) {
      return true;
    }

    if (mode == HvacMode::HEAT && events[index].hvac == HvacMode::COOL) {
      return true;
    }

    // use the start time for the next previous event as it's stop time.
    millis_since = Clock::MillisDiff(events[index].start_time, now);

    if (millis_since > kLockoutMs) {
      return false;
    }
  }

  return false;
}

static uint32_t OldestEventStart(const Settings& settings, const Clock& clock) {
  // Loop through all the stored events.
  const uint32_t now = clock.Millis();

  uint32_t oldest_start_time;
  uint32_t oldest_diff = 0;
  for (int idx = 0; idx < EVENT_SIZE; ++idx) {
    // Account for wrap around by using MillisDiff.
    if (Clock::MillisDiff(settings.events[idx].start_time, now) > oldest_diff) {
      oldest_start_time = settings.events[idx].start_time;
    }
  }
  return oldest_start_time;
}

static uint32_t CalculateDurationSinceTime(const uint32_t history_start, const uint32_t event_start, const uint32_t duration) {
  const uint32_t event_end = event_start + duration;

  // Do we need to clip to the amount during the history window.
  //
  // We clip when the event straddle the history start point.
  if (MillisSubtract(event_end, history_start) >= 0 &&
      MillisSubtract(event_start, history_start) < 0) {
    return event_end - history_start;
  }
  return duration;
}

// Returns how long the system has been either running or not running.
static uint32_t CalculateSeconds(const FanMode fan, const Settings& settings,
                                 const uint32_t history_window_ms, const Clock& clock) {
  uint32_t total_seconds = 0;
  const uint32_t now = clock.Millis();

  // Loop through all the stored events.
  for (int idx = 0; idx < EVENT_SIZE; ++idx) {
    const uint32_t duration_ms = CalculateDurationSinceTime(
                                   now - history_window_ms,
                                   settings.events[idx].start_time,
                                   GetEventDuration(idx, settings, now));
    //
    //    LOG(INFO) << "idx:" << idx
    //              << " start: " << settings.events[idx].start_time / 60 / 1000
    //              << " duration:" << duration_ms / 60 / 1000
    //              << " fan:" << ((settings.events[idx].fan == FanMode::ON) ? "ON" : "OFF");

    // Only sum events that have a duration.
    if (duration_ms == 0) {
      continue;
    }

    // Only sum events that match the desired running state.
    if (settings.events[idx].fan != fan) {
      continue;
    }

    //LOG(INFO) << "Total:" << total_seconds / 60 << " Summing " << duration_ms / 1000 / 60 << " for fan:" << (settings.events[idx].fan == FanMode::ON);
    total_seconds += duration_ms / 1000;
  }

  return total_seconds;
};

// Returns how long the system has been either running or not running.
static uint32_t CalculateSeconds(const HvacMode hvac, const Settings& settings,
                                 const uint32_t history_window_ms, const Clock& clock) {
  uint32_t total_seconds = 0;
  const uint32_t now = clock.Millis();

  // Loop through all the stored events.
  for (int idx = 0; idx < EVENT_SIZE; ++idx) {
    uint32_t duration_ms = GetEventDuration(idx, settings, now);

    // Clip to the amount during the history window.
    //
    // TODO: Fix the millis wrap around issue if this is used for more
    // than simple user output.
    if (now - history_window_ms < settings.events[idx].start_time + duration_ms &&
        now - history_window_ms >= settings.events[idx].start_time) {
      duration_ms =
        (settings.events[idx].start_time + duration_ms) - (now - history_window_ms);
    }

    // Only sum events that valid and have a duration.
    if (duration_ms == 0) {
      continue;
    }

    // Only sum events that match the desired running state.
    if (settings.events[idx].hvac != hvac) {
      continue;
    }

    total_seconds += duration_ms / 1000;
  }

  return total_seconds;
};
static uint32_t HeatRise(const Settings& settings, const Clock& clock) {
  uint32_t now = clock.Millis();
  // Iterate backward for the latest two heat events or up to 12 hours.
  uint8_t heatrate_count = 0;
  int32_t heatrate = 0;
  uint8_t last_idx = (settings.event_index - 1) % EVENT_SIZE;
  for (uint8_t idx = settings.event_index; idx != last_idx; --idx) { 
    if (settings.events[idx].empty()) {
      break;
    }
    if (Clock::MillisDiff(settings.events[idx].start_time, now) >
        Clock::DaysToMillis(24)) {
      break;
    }

    if (settings.events[idx].hvac == HvacMode::HEAT &&
        settings.events[idx].temperature_10min_x10 != 0) {
      const int16_t rate = settings.events[idx].temperature_10min_x10 - settings.events[idx].temperature_x10;
      if (rate > 0) {
        heatrate += rate;
        ++heatrate_count;
      }
    }

    if (heatrate_count >= 2) {
      break;
    }
    
    idx = (idx - 1) % EVENT_SIZE;
  }

  return heatrate/heatrate_count;  
}
static int16_t OutdoorTemperatureEstimate(const Settings& settings, const Clock& clock) {
  const uint32_t oldest_start_time = cmin(OldestEventStart(settings, clock), Clock::HoursToMillis(24));
  const uint32_t heat_seconds = CalculateSeconds(HvacMode::HEAT, settings, Clock::HoursToMillis(24), clock);
  const float heat_ratio = heat_seconds / Clock::MillisToSeconds(oldest_start_time);

  // Focus on 20F to -20F since this is where humidity control needs to change.
  if (heat_ratio < 20) {
    return 200;  // 20F
    // Set humidity to 40%
  }
  if (heat_ratio < 25) {
    return 100;  // 10F
  }
  if (heat_ratio < 32) {
    return 000;  // 0F
  }
  if (heat_ratio < 40) {
    return -100;  // -10F
  } else {
    return -200;  // -20F
  }
}

}  // namespace thermostat
#endif  // EVENTS_H_
