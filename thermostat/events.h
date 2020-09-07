#ifndef EVENTS_H_
#define EVENTS_H_
// Helpers to manage the Events struct for performing calculations and creating new events
// whena applicable.

#include "settings.h"

namespace thermostat {

static void AddOrUpdateEvent(const uint32_t now, Settings* const settings) {
  // Clear any events that are over a month old.
  for (uint8_t i = 0; i < EVENT_SIZE; ++i) {
    if (settings->events[i].empty()) {
      continue;
    }
    if (Clock::millisDiff(settings->events[i].start_time, now) > Clock::DaysToMillis(30)) {
      settings->events[i].set_empty();
    }
  }

  // Check to see if the last settings match the current window.
  bool make_new_event = false;

  if (settings->event_index == 255) {
    make_new_event = true;
  }

  Event* event = &settings->events[settings->event_index];
  if (settings->GetHvacMode() != event->hvac) {
    make_new_event = true;
  }
  
  if (settings->GetFanMode() != event->fan) {
    make_new_event = true;
  }

  if (make_new_event) {
    settings->event_index = (settings->event_index + 1) % EVENT_SIZE;
    Event* new_event = &settings->events[settings->event_index];
    new_event->start_time = now;

    new_event->temperature_x10 = settings->current_mean_temperature_x10;
    new_event->hvac = settings->GetHvacMode();
    new_event->fan = settings->GetFanMode();
  }
}

// Returns Zero when empty, otherwise the length of time for the event.
static uint32_t GetEventDuration(const uint8_t index, const Settings& settings, const uint32_t now) {
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
    return Clock::millisDiff(settings.events[index].start_time, now);
  }
  return Clock::millisDiff(settings.events[index].start_time,
                           settings.events[next_index].start_time);
}

//// Returns true if found and value passed back in diff parameter.
//static bool GetEventTempDiff(const uint8_t index, const Settings& settings,
//                      const uint32_t now, int* temperature_diff_x10) {
//  // If the event is empty, we don't have a duration.
//  if (settings.events[index].empty) {
//    return 0;
//  }
//
//  // When we don't have an event after this, then we use the current time.
//  if (settings.events[(index + 1) % EVENT_SIZE].empty) {
//    return Clock::millisDiff(settings.events[index].start_time, now);
//  }
//  return Clock::millisDiff(settings.events[index].start_time,
//                    settings.events[(index + 1) % EVENT_SIZE].start_time);
//}

// This checks if we should be in a 5 minute lockout when switching from cooling to
// heating or heating to cooling.
static bool IsInLockoutMode(const HvacMode mode, const Event* events,
                            const uint8_t events_size, const uint32_t now) {
  constexpr uint32_t kLockoutMs = 5UL * 60UL * 1000UL;
  uint8_t index = 0;
  uint32_t millis_since = 0xFFFFFFFF;  // some big number

  // Find the newest event.
  for (int i = 0; i < events_size; ++i) {
    if (events[i].empty()) {
      continue;
    }
    if (Clock::millisDiff(events[i].start_time, now) < millis_since) {
      millis_since = Clock::millisDiff(events[i].start_time, now);
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
  if (mode == HvacMode::COOL && millis_since < kLockoutMs && events[index].hvac == HvacMode::HEAT) {
    return true;
  }
  if (mode == HvacMode::HEAT && millis_since < kLockoutMs && events[index].hvac == HvacMode::COOL) {
    return true;
  }

  // Keep looking back until we're beyond the window..
  while (true) {
    index = (index - 1) % events_size;
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
    millis_since = Clock::millisDiff(events[index].start_time, now);

    if (millis_since > kLockoutMs) {
      return false;
    }
  }

  return false;
}


static uint32_t CalculateSeconds(bool running, const Settings& settings, uint8_t* events, uint32_t now);

static float OnPercent(const Settings& settings, const uint32_t now) {
  const int current_index = settings.CurrentEventIndex();
  if (current_index == -1) {
    return 0;
  }
  uint8_t event_count_noop;
  const uint32_t on_seconds = CalculateSeconds(true, settings, &event_count_noop, now);
  const uint32_t off_seconds = CalculateSeconds(false, settings, &event_count_noop, now);
  if (off_seconds == 0 && on_seconds == 0) {
    return 0;
  }
  return static_cast<float>(on_seconds) / (on_seconds + off_seconds);
}

// Returns how long the system has been either running or not running.
static uint32_t CalculateSeconds(const bool running, const Settings& settings,
                                 uint8_t* const events, const uint32_t now) {
  uint32_t total_seconds = 0;
  uint32_t total_events = 0;

  // Loop through all the stored events.
  for (int idx = 0; idx < EVENT_SIZE; ++idx) {
    const uint32_t duration = GetEventDuration(idx, settings, now);

    // Only sum events that valid and have a duration.
    if (duration == 0) {
      continue;
    }

    const bool is_running = settings.events[idx].hvac == HvacMode::HEAT || settings.events[idx].hvac == HvacMode::COOL;

    // Only sum events that match the desired running state.
    if (running != is_running) {
      continue;
    }

    total_seconds += duration / 1000;
    ++total_events;
  }

  *events = total_events;
  return total_seconds;
};

//static float TempDeltaPerMinute(const bool running, const Settings& settings, const uint32_t now) {
//  const int current_index = settings.CurrentEventIndex();
//  if (current_index == -1) {
//    return 0;
//  }
//
//  const int oldest_index = OldestIndex(settings, now);
//  if (oldest_index == -1) {
//    return 0;
//  }
//
//  int32_t total_seconds = 0;
//  int32_t total_events = 0;
//  double total_temperature_diff = 0;
//
//  int index = current_index;
//  while (true) {
//    // Skip the current index, but use it's start time as the end time for the previous.
//    const uint32_t end_ms = settings.events[index].start_time;
//    const int end_temp = settings.events[index].temperature_x10;
//
//    // Stop if we have reached the oldest index.
//    if (index == oldest_index) {
//      break;
//    }
//
//    // Since we check for oldest entry, we shouldn't reach an empty entry.
//    index = index - 1;
//
//    const uint32_t start_ms = settings.events[index].start_time;
//    const int start_temp = settings.events[index].temperature_x10;
//
//    const int32_t length_sec = Clock::millisDiff(start_ms, end_ms) / 1000;
//    const double temperature_diff = (end_temp - start_temp) / 10.0;
//
//    if (running && settings.events[index].heat && settings.events[index].cool) {
//      total_seconds += length_sec;
//      total_temperature_diff += temperature_diff;
//      ++total_events;
//    }
//    if (!running && (settings.events[index].heat || settings.events[index].cool)) {
//      total_seconds += length_sec;
//      ++total_events;
//    }
//  }
//
//  if (total_events > 0) {
//    return total_temperature_diff / (total_seconds / 60.0);
//  }
//
//  return 0x0;
//}

//static int OldestIndex(const Settings& settings, const uint32_t now) {
//  int index = -1;
//  uint32_t index_ms = 0;
//  for (int i = 0; i < EVENT_SIZE; ++i) {
//    if (settings.events[i].empty) {
//      continue;
//    }
//
//    const uint32_t ms = Clock::millisDiff(settings.events[i].start_time, now);
//
//    if (index == -1 || ms > index_ms) {
//      index = i;
//      index_ms = ms;
//    }
//  }
//  return index;
//}

}
#endif // EVENTS_H_
