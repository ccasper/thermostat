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
#include "thermostat/interfaces.h"
#include "thermostat/thermostat_tasks.h"
#include "thermostat/settings.h"

namespace thermostat {
namespace {

TEST(EventsTest, InitialConditions) {
  Settings settings;
  FakeClock clock;
  
  EXPECT_EQ(settings.CurrentEventIndex(), -1);
  for (int i = -1; i < EVENT_SIZE; ++i) {
    EXPECT_EQ(GetEventDuration(i, settings, clock.Millis()), 0);
  }
  EXPECT_FALSE(IsInLockoutMode(HvacMode::HEAT, settings.events, clock.Millis()));
  EXPECT_FALSE(IsInLockoutMode(HvacMode::COOL, settings.events, clock.Millis()));
  EXPECT_EQ(CalculateSeconds(HvacMode::HEAT, settings, Clock::HoursToMillis(24), clock), 0);
  EXPECT_EQ(CalculateSeconds(HvacMode::COOL, settings, Clock::HoursToMillis(24), clock), 0);
  EXPECT_EQ(CalculateSeconds(HvacMode::COOL_LOCKOUT, settings, Clock::HoursToMillis(24), clock), 0);
  EXPECT_EQ(CalculateSeconds(HvacMode::HEAT_LOCKOUT, settings, Clock::HoursToMillis(24), clock), 0);
  EXPECT_EQ(CalculateSeconds(FanMode::ON, settings, Clock::HoursToMillis(24), clock), 0);
  EXPECT_EQ(CalculateSeconds(FanMode::OFF, settings, Clock::HoursToMillis(24), clock), 0);
}

TEST(EventsTest, OutdoorTemperatureEstimateNoEvents) {
  Settings settings;	
  FakeClock clock; 
  // Default should be 20F.
  EXPECT_EQ(OutdoorTemperatureEstimate(settings, clock), 200);
}

TEST(EventsTest, HeatRiseNoEvents) {
  Settings settings;	
  FakeClock clock; 
  // Default should be 0F.
  EXPECT_EQ(HeatRise(settings,clock), 000);
}

TEST(EventsTest, SeveralEvents) {
  Settings settings;

  FakeClock clock;
  // Move forward so we don't start at 0.
  clock.Increment(Clock::HoursToMillis(48));
  
  int i = 0;
  {
    settings.event_index = i;
    settings.events[i].hvac = HvacMode::HEAT;
    settings.events[i].fan = FanMode::ON;
    settings.events[i].temperature_x10 = 75;
    settings.events[i].start_time = clock.Millis();
  }

  EXPECT_EQ(settings.CurrentEventIndex(), 0);
  EXPECT_FALSE(IsInLockoutMode(HvacMode::HEAT, settings.events, clock.Millis()));
  EXPECT_TRUE(IsInLockoutMode(HvacMode::COOL, settings.events, clock.Millis()));

  // Heating for 24 minutes.
  clock.Increment(Clock::MinutesToMillis(24));

  EXPECT_EQ(CalculateSeconds(FanMode::ON, settings, Clock::HoursToMillis(24), clock),
            Clock::MinutesToSeconds(24));
  EXPECT_EQ(CalculateSeconds(HvacMode::HEAT, settings, Clock::HoursToMillis(24), clock),
            Clock::MinutesToSeconds(24));

  {
    ++i;
    settings.event_index = i;
    settings.events[i].hvac = HvacMode::IDLE;
    settings.events[i].fan = FanMode::ON;
    settings.events[i].temperature_x10 = 75;
    settings.events[i].start_time = clock.Millis();
  }

  clock.Increment(Clock::MinutesToMillis(10));

  {
    ++i;
    settings.event_index = i;
    settings.events[i].hvac = HvacMode::IDLE;
    settings.events[i].fan = FanMode::OFF;
    settings.events[i].temperature_x10 = 73;
    settings.events[i].start_time = clock.Millis();
  }

  clock.Increment(Clock::MinutesToMillis(25));

  EXPECT_EQ(settings.CurrentEventIndex(), 2);

  EXPECT_EQ(CalculateSeconds(FanMode::ON, settings, Clock::HoursToMillis(24), clock),
            Clock::MinutesToSeconds(24 + 10));
  EXPECT_EQ(CalculateSeconds(HvacMode::HEAT, settings, Clock::HoursToMillis(2), clock),
            Clock::MinutesToSeconds(24));

  {
    ++i;
    settings.event_index = i;
    settings.events[i].hvac = HvacMode::COOL;
    settings.events[i].temperature_x10 = 75;
    settings.events[i].start_time = clock.Millis();
  }

  EXPECT_EQ(settings.CurrentEventIndex(), 3);

  EXPECT_TRUE(IsInLockoutMode(HvacMode::HEAT, settings.events, clock.Millis()));
  EXPECT_FALSE(IsInLockoutMode(HvacMode::COOL, settings.events, clock.Millis()));

  // Heat for 24 minutes, off for 35 mins, Cool for 17 mins.
  clock.Increment(Clock::MinutesToMillis(17));

  EXPECT_TRUE(IsInLockoutMode(HvacMode::HEAT, settings.events, clock.Millis()));
  EXPECT_FALSE(IsInLockoutMode(HvacMode::COOL, settings.events, clock.Millis()));

  EXPECT_EQ(GetEventDuration(0, settings, clock.Millis()), Clock::MinutesToMillis(24));
  EXPECT_EQ(GetEventDuration(1, settings, clock.Millis()), Clock::MinutesToMillis(10));
  EXPECT_EQ(GetEventDuration(2, settings, clock.Millis()), Clock::MinutesToMillis(25));
  EXPECT_EQ(GetEventDuration(3, settings, clock.Millis()), Clock::MinutesToMillis(17));
  for (int i = 4; i < EVENT_SIZE; ++i) {
    EXPECT_EQ(GetEventDuration(i, settings, clock.Millis()), 0);
  }

  EXPECT_EQ(CalculateSeconds(HvacMode::HEAT, settings, Clock::HoursToMillis(2), clock),
            Clock::MinutesToSeconds(24));
  EXPECT_EQ(CalculateSeconds(HvacMode::COOL, settings, Clock::HoursToMillis(2), clock),
            Clock::MinutesToSeconds(17));
  EXPECT_EQ(CalculateSeconds(FanMode::ON, settings, Clock::HoursToMillis(2), clock),
            Clock::MinutesToSeconds(24 + 10));

  //    event->empty = false;
  //    event->heat = settings->heat_running;
  //    event->cool = settings->cool_running;
  //    event->temperature_x10 = settings->current_mean_temperature_x10;
}

TEST(EventsTest, GetHeatTempPerMin) {
  Settings settings;
  FakeClock clock;
  
  clock.SetMillis(Clock::HoursToMillis(0));
  settings.events[3].hvac = HvacMode::HEAT;
  settings.events[3].start_time = clock.Millis();
  settings.events[3].temperature_x10 = 600;
  settings.events[3].temperature_10min_x10 = 800;
      
  float temp_per_min = GetHeatTempPerMin(settings, clock);
  
  EXPECT_FLOAT_EQ(temp_per_min, 20 / kTenMinuteAdjustmentMins); 

  clock.Increment(Clock::HoursToMillis(6));
  // Add a second event now.
  settings.events[4].hvac = HvacMode::HEAT;
  settings.events[4].start_time = clock.Millis();
  settings.events[4].temperature_x10 = 700;
  settings.events[4].temperature_10min_x10 = 750;

  {
    float temp_per_min = GetHeatTempPerMin(settings, clock);
    // Average of the two.
    EXPECT_FLOAT_EQ(temp_per_min, (20 + 5) / kTenMinuteAdjustmentMins / 2); 
  }

  // Move beyond the first event, so we only have the second event.
  clock.Increment(kEventHorizon);
  {
    float temp_per_min = GetHeatTempPerMin(settings, clock);
    // Only the newest value should be accounted for.
    EXPECT_FLOAT_EQ(temp_per_min, 5 / kTenMinuteAdjustmentMins); 
  }
  
}

TEST(InterfacesTest, MillisSubtract) {
    EXPECT_EQ(MillisSubtract(0x000FF000, 0x000F0000), 0x0000F000);
    EXPECT_EQ(MillisSubtract(0xF00FF000, 0xF00F0000), 0x0000F000);
    
    EXPECT_EQ(MillisSubtract(0xFFFFFFFF, 0x00000000), -1);
    EXPECT_EQ(MillisSubtract(0x00000000, 0xFFFFFFFF), 1);
    
    EXPECT_EQ(MillisSubtract(0x00001000, 0xFFFF0000), 69632);

    EXPECT_EQ(MillisSubtract(0xFFFF0000, 0x00001000), -69632);
    
    // Difference between a - b > UINT32_MAX / 2, so adjust.
    //
    // TODO: Is this correct?
    EXPECT_EQ(MillisSubtract(0x9FFFFFFF, 0x00001000), -1610616833);
    EXPECT_EQ(MillisSubtract(0x8FFFFFFF, 0x00001000), -1879052289);

    EXPECT_EQ(MillisSubtract(0x00001000, 0xDFFFFFFF), 536875009);
    EXPECT_EQ(MillisSubtract(0x00001000, 0x9FFFFFFF), 1610616833);
    EXPECT_EQ(MillisSubtract(0x00001000, 0x8FFFFFFF), 1879052289);
    EXPECT_EQ(MillisSubtract(0x00001000, 0x7FFFFFFF), -2147479551);
    
    // No wrap around on this.
    //
    // TODO: Is this correct?
    EXPECT_EQ(MillisSubtract(0x7FFFFFFF, 0x00001000), 0x7FFFEFFF);
    
    EXPECT_EQ(MillisSubtract(0x00001000, 0xFFFF0000), 69632);
      
      // when a = 0xFFFF0000 (-65535) and b = 0x00001000 (4096), output = -69632;
      // when a = 0x00001000  (4096) and b = 0xFFFF0000 (-65535), output = -69632;

}

TEST(EventsTest, FanSampleEvents) {
  Settings settings;
  uint8_t idx = 0;
  settings.events[idx].start_time = 1172ul * 60 * 1000;
  settings.events[idx].hvac = HvacMode::IDLE;
  settings.events[idx].fan = FanMode::ON;
  
  settings.events[++idx].start_time = 1202ul * 60 * 1000;
  settings.events[idx].hvac = HvacMode::HEAT;
  settings.events[idx].fan = FanMode::ON;

  settings.events[++idx].start_time = 1202 * 60 * 1000;
  settings.events[idx].hvac = HvacMode::IDLE;
  settings.events[idx].fan = FanMode::ON;

  settings.events[++idx].start_time = 100 * 60 * 1000;
  settings.events[idx].hvac = HvacMode::EMPTY;
  settings.events[idx].fan = FanMode::EMPTY;

  settings.events[++idx].start_time = 259 * 60 * 1000;
  settings.events[idx].hvac = HvacMode::IDLE;
  settings.events[idx].fan = FanMode::OFF;

  settings.events[++idx].start_time = 484 * 60 * 1000;
  settings.events[idx].hvac = HvacMode::IDLE;
  settings.events[idx].fan = FanMode::ON;

  settings.events[++idx].start_time = 490 * 60 * 1000;
  settings.events[idx].hvac = HvacMode::HEAT;
  settings.events[idx].fan = FanMode::ON;

  settings.events[++idx].start_time = 509 * 60 * 1000;
  settings.events[idx].hvac = HvacMode::IDLE;
  settings.events[idx].fan = FanMode::ON;

  settings.events[++idx].start_time = 541 * 60 * 1000;
  settings.events[idx].hvac = HvacMode::IDLE;
  settings.events[idx].fan = FanMode::OFF;

  settings.events[++idx].start_time = 912 * 60 * 1000;
  settings.events[idx].hvac = HvacMode::IDLE;
  settings.events[idx].fan = FanMode::ON;

  settings.events[++idx].start_time = 947 * 60 * 1000;
  settings.events[idx].hvac = HvacMode::IDLE;
  settings.events[idx].fan = FanMode::OFF;

  FakeClock clock;
  clock.SetMillis(1203 * 60 * 1000);

  uint32_t fan_on = CalculateSeconds(FanMode::ON, settings, Clock::HoursToMillis(24), clock); 
  uint32_t fan_off = CalculateSeconds(FanMode::OFF, settings, Clock::HoursToMillis(24), clock); 
  LOG(INFO) << "FanOn: " << fan_on << " FanOff: " << fan_off;
  EXPECT_GT(fan_on, fan_off * 0.10);
  EXPECT_LT(fan_on, fan_off * 0.30);
}

}  // namespace
}  // namespace thermostat
