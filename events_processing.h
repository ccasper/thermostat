#ifndef DRIFT_DETECTOR_H_
#define DRIFT_DETECTOR_H_
// Helpers to manage the Events struct for performing calculations and creating new events whena applicable.

#include "settings.h"

void AddOrUpdateEvent(const uint32_t time_ms, Settings* const settings) {
  // Clear any events that are over a month old.
  for (uint8_t i = 0; i < EVENT_SIZE; ++i) {
    if (settings->events[i].empty) {
      continue;
    }
    if (daysSince(settings->events[i].start_time) > 30) {
      settings->events[i].empty = true;
    }
  }

  // Check to see if the last settings match the current window.
  bool new_event = false;

  if (settings->event_index == 255) {
    new_event = true;
  }

  Event *event = &settings->events[settings->event_index];
  if (settings->heat_running != event->heat) {
    new_event = true;
  }
  if (settings->cool_running != event->cool) {
    new_event = true;
  }

  if (new_event) {
    settings->event_index = (settings->event_index + 1) % EVENT_SIZE;
    Event *event = &settings->events[settings->event_index];
    event->start_time = millis();
    event->empty = false;
    event->heat = settings->heat_running;
    event->cool = settings->cool_running;
    event->temp_x10 = settings->current_mean_temp_x10;

    Serial.println("######## NEW EVENT: " + String(settings->event_index) + " Mode: " + String(event->heat) + String(event->cool));
  }

}

// Returns Zero whenn empty, otherwise the length of time for the event.
uint32_t GetEventDuration(const uint8_t index, const Settings& settings) {
  // If the event is empty, we don't have a duration.
  if (settings.events[index].empty) {
    return 0;
  }

  // When we don't have an event after this, then we use the current time.
  if (settings.events[(index + 1) % EVENT_SIZE].empty) {
    return millisSince(settings.events[index].start_time);
  }
  return millisDiff(settings.events[index].start_time, settings.events[(index + 1) % EVENT_SIZE].start_time);
}

// Returns true if found and value passed back in diff parameter.
bool GetEventTempDiff(const uint8_t index, const Settings& settings, int* temp_diff_x10) {
  // If the event is empty, we don't have a duration.
  if (settings.events[index].empty) {
    return 0;
  }

  // When we don't have an event after this, then we use the current time.
  if (settings.events[(index + 1) % EVENT_SIZE].empty) {
    return millisSince(settings.events[index].start_time);
  }
  return millisDiff(settings.events[index].start_time, settings.events[(index + 1) % EVENT_SIZE].start_time);
}




int OldestIndex(const Settings& settings) {
  int index = -1;
  uint32_t index_ms = 0;
  for (int i = 0; i < EVENT_SIZE; ++i) {
    if (settings.events[i].empty) {
      continue;
    }

    const uint32_t ms = millisSince(settings.events[i].start_time);

    if (index == -1 || ms > index_ms) {
      index = i;
      index_ms = ms;
    }
  }
  return index;
}

uint32_t CalculateSeconds(bool running, const Settings& settings, uint8_t *events);
float OnPercent(const Settings& settings) {
  const int current_index = settings.CurrentEventIndex();
  if (current_index == -1) {
    return 0;
  }
  uint8_t event_count_noop;
  const uint32_t on_seconds = CalculateSeconds(true, settings, &event_count_noop);
  const uint32_t off_seconds = CalculateSeconds(false, settings, &event_count_noop);
  if (off_seconds == 0 && on_seconds == 0) {
    return 0;
  }
  return on_seconds / (on_seconds + off_seconds);
}

float TempDeltaPerMinute(const bool running, const Settings& settings) {
  const int current_index = settings.CurrentEventIndex();
  if (current_index == -1) {
    return 0;
  }

  const int oldest_index = OldestIndex(settings);
  if (oldest_index == -1) {
    return 0;
  }

  int32_t total_seconds = 0;
  int32_t total_events = 0;
  double total_temp_diff = 0;

  int index = current_index;
  while (true) {
    // Skip the current index, but use it's start time as the end time for the previous.
    const uint32_t end_ms = settings.events[index].start_time;
    const int end_temp = settings.events[index].temp_x10;

    // Stop if we have reached the oldest index.
    if (index == oldest_index) {
      break;
    }

    // Since we check for oldest entry, we shouldn't reach an empty entry.
    index = index - 1;

    const uint32_t start_ms = settings.events[index].start_time;
    const int start_temp = settings.events[index].temp_x10;

    const int32_t length_sec = millisDiff(end_ms, start_ms) / 1000;
    const double temp_diff = (end_temp - start_temp) / 10.0;

    if (running && settings.events[index].heat && settings.events[index].cool) {
      total_seconds += length_sec;
      total_temp_diff += temp_diff;
      ++total_events;
    }
    if (!running && (settings.events[index].heat || settings.events[index].cool)) {
      total_seconds += length_sec;
      ++total_events;
    }
  }

  if (total_events > 0) {
    return total_temp_diff / (total_seconds / 60.0);
  }

  return 0x0;
}

// Returns how long the system has been either running or not running.
uint32_t CalculateSeconds(const bool running, const Settings& settings, uint8_t *const events) {
  uint32_t total_seconds = 0;
  uint32_t total_events = 0;

  // Loop through all the stored events.
  for (int idx = 0; idx < EVENT_SIZE; ++idx) {
    const uint32_t duration = GetEventDuration(idx, settings);

    // Only sum events that valid and have a duration.
    if (duration == 0) {
      continue;
    }

    const bool is_running = settings.events[idx].heat || settings.events[idx].cool;

    // Only sum events that match the desired running state.
    if (running != is_running) {
      continue;
    }

    total_seconds += duration / 1000;
    ++total_events;
  }

  *events = total_events;
  return total_seconds;
}

#endif
