#ifndef MILLIS_SINCE_H_
#define MILLIS_SINCE_H_

// Calculate the millis since the previous time accounting for wrap around.
uint32_t millisDiff(const uint32_t previous, const uint32_t next) {
  // Millis rolls over after about 50 days, the unsigned subtraction accounts for this.
  return next - previous;
}

// Calculate the millis since the previous time accounting for wrap around.
uint32_t millisSince(const uint32_t previous) {
  return millisDiff(previous, millis());
}

// Calculate the millis since the previous time accounting for wrap around.
int minutesSince(const uint32_t previous) {
  return millisDiff(previous, millis()) / 1000 /*sec*/ / 60 /*minutes*/;
}

// Calculate the millis since the previous time accounting for wrap around.
int daysSince(const uint32_t previous) {
  return minutesSince(previous) / 60 /*hours*/ / 24 /*hours*/;
}

// Converts Millis to Seconds for easier comparison.
uint32_t SecondsToMillis(const uint32_t& seconds) {
  return seconds * 1000;
}

// Converts Millis to Minutes for easier comparison.
uint32_t MinutesToMillis(const uint32_t& minutes) {
  return minutes * 60 * 1000;
}

// Convert Millis to Hours for easier comparison.
uint32_t HoursToMillis(const uint32_t& hours) {
 return hours * 60 * 60 * 1000;
}

#endif
