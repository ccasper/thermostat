// apt install libgoogle-glog-dev
#include <glog/logging.h>
// apt install libgtest-dev
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdint.h>

#include <cmath>

#include "mock_impls.h"
#include "thermostat/comparison.h"
#include "thermostat/events.h"
#include "thermostat/fan_controller.h"
#include "thermostat/interfaces.h"
#include "thermostat/maintain_hvac.h"
#include "thermostat/settings.h"

namespace thermostat {
namespace {

TEST(EventsTest, InitialConditions) {
  Settings settings;
  ClockStub clock;
  
  EXPECT_EQ(settings.CurrentEventIndex(), -1);
  for (int i = -1; i < EVENT_SIZE; ++i) {
    EXPECT_EQ(GetEventDuration(i, settings, clock), 0);
  }
  EXPECT_FALSE(IsInLockoutMode(HvacMode::HEAT, settings.events, EVENT_SIZE, clock));
  EXPECT_FALSE(IsInLockoutMode(HvacMode::COOL, settings.events, EVENT_SIZE, clock));
  uint8_t events = 0;
  EXPECT_EQ(CalculateSeconds(true, settings, &events, clock), 0);
  EXPECT_EQ(events, 0);
  EXPECT_EQ(CalculateSeconds(false, settings, &events, clock), 0);
  EXPECT_EQ(events, 0);
  EXPECT_EQ(OnPercent(settings, clock), 0);
}

TEST(EventsTest, SeveralEvents) {
  Settings settings;

  ClockStub clock;
  int i = 0;
  {
    clock.Increment(Clock::MinutesToMillis(71));
    settings.event_index = i;
    settings.events[i].hvac = HvacMode::HEAT;
    settings.events[i].temperature_x10 = 75;
    settings.events[i].start_time = clock.Millis();
  }
  EXPECT_EQ(settings.CurrentEventIndex(), 0);
  EXPECT_FALSE(IsInLockoutMode(HvacMode::HEAT, settings.events, EVENT_SIZE, clock));
  EXPECT_TRUE(IsInLockoutMode(HvacMode::COOL, settings.events, EVENT_SIZE, clock));

  {
    ++i;
    clock.Increment(Clock::MinutesToMillis(24));
    settings.event_index = i;
    settings.events[i].hvac = HvacMode::IDLE;
    settings.events[i].temperature_x10 = 75;
    settings.events[i].start_time = clock.Millis();
  }

  EXPECT_EQ(settings.CurrentEventIndex(), 1);

  {
    ++i;
    clock.Increment(Clock::MinutesToMillis(35));
    settings.event_index = i;
    settings.events[i].hvac = HvacMode::COOL;
    settings.events[i].temperature_x10 = 75;
    settings.events[i].start_time = clock.Millis();
  }

  EXPECT_EQ(settings.CurrentEventIndex(), 2);

  EXPECT_TRUE(IsInLockoutMode(HvacMode::HEAT, settings.events, EVENT_SIZE, clock));
  EXPECT_FALSE(IsInLockoutMode(HvacMode::COOL, settings.events, EVENT_SIZE, clock));

  // Heat for 24 minutes, off for 35 mins, Cool for 17 mins.
  clock.Increment(Clock::MinutesToMillis(17));

  EXPECT_FALSE(IsInLockoutMode(HvacMode::HEAT, settings.events, EVENT_SIZE, clock));
  EXPECT_FALSE(IsInLockoutMode(HvacMode::COOL, settings.events, EVENT_SIZE, clock));

  EXPECT_EQ(GetEventDuration(0, settings, clock), Clock::MinutesToMillis(24));
  EXPECT_EQ(GetEventDuration(1, settings, clock), Clock::MinutesToMillis(35));
  EXPECT_EQ(GetEventDuration(2, settings, clock), Clock::MinutesToMillis(17));
  for (int i = 3; i < EVENT_SIZE; ++i) {
    EXPECT_EQ(GetEventDuration(i, settings, clock), 0);
  }

  uint8_t events = 0;
  EXPECT_EQ(CalculateSeconds(true, settings, &events, clock),
            Clock::MinutesToMillis(24 + 17) / 1000);
  EXPECT_EQ(events, 2);
  EXPECT_EQ(CalculateSeconds(false, settings, &events, clock),
            Clock::MinutesToMillis(35) / 1000);
  EXPECT_EQ(events, 1);
  EXPECT_NEAR(OnPercent(settings, clock), static_cast<double>(24 + 17) / (24 + 17 + 35),
              0.00001);

  //    event->empty = false;
  //    event->heat = settings->heat_running;
  //    event->cool = settings->cool_running;
  //    event->temperature_x10 = settings->current_mean_temperature_x10;
}

TEST(EventsTest, GetHeatTempPerMin) {
  Settings settings;
  ClockStub clock;
  
  clock.SetMillis(Clock::HoursToMillis(0));
  settings.events[3].hvac = HvacMode::HEAT;
  settings.events[3].start_time = clock.Millis();
  settings.events[3].temperature_x10 = 600;
  settings.events[3].temperature_10min_x10 = 800;

  // Move to just before 2 days between events.
  clock.SetMillis(Clock::HoursToMillis(47));
      
  float temp_per_min = GetHeatTempPerMin(settings, clock);
  
  EXPECT_FLOAT_EQ(temp_per_min, 20 / 9.5); 

  // Add a second event now.
  settings.events[5].hvac = HvacMode::HEAT;
  settings.events[5].start_time = clock.Millis();
  settings.events[5].temperature_x10 = 700;
  settings.events[5].temperature_10min_x10 = 750;

  {
    float temp_per_min = GetHeatTempPerMin(settings, clock);
    // Average of the two.
    EXPECT_FLOAT_EQ(temp_per_min, (20 + 5) / 9.5 / 2); 
  }

  // Move beyond the first event, so we only have the second event.
  clock.SetMillis(Clock::HoursToMillis(49));
  {
    float temp_per_min = GetHeatTempPerMin(settings, clock);
    // Only the newest value should be accounted for.
    EXPECT_FLOAT_EQ(temp_per_min, 5 / 9.5); 
  }
  
}

}  // namespace
}  // namespace thermostat
