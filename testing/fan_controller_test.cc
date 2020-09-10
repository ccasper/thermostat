#include <glog/logging.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdint.h>

#include <cmath>

#include "mock_impls.h"
#include "thermostat/comparison.h"
#include "thermostat/fan_controller.h"
#include "thermostat/interfaces.h"
#include "thermostat/maintain_hvac.h"
#include "thermostat/settings.h"

namespace thermostat {

Settings DefaultSettings() {
  Settings defaults;
  defaults.persisted.version = VERSION;
  defaults.persisted.fan_always_on = false;
  defaults.fan = FanMode::OFF;
  defaults.hvac =  HvacMode::IDLE;
  defaults.persisted.fan_on_min_period = 120;
  defaults.persisted.fan_on_duty = 30;
  defaults.persisted.fan_extend_mins = 5;
  defaults.persisted.heat_enabled = true;
  return defaults;
};

class FanControllerTest : public testing::Test {
 public:
  void SetUp() override {}

 protected:
  ClockStub clock_;
  FanController fan_{&clock_};
  Settings settings_{DefaultSettings()};
};

TEST_F(FanControllerTest, Basic) {
  // Do an update with the fan off.
  {
    clock_.SetMillis(Clock::MinutesToMillis(0));
    fan_.Maintain(&settings_);
  }

  // Start running heat @ 10 minutes.
  {
    clock_.SetMillis(Clock::MinutesToMillis(10));
    settings_.hvac = HvacMode::HEAT;
    fan_.Maintain(&settings_);
  }

  // Stop heat after 10 minutes.
  {
    clock_.SetMillis(Clock::MinutesToMillis(20) - Clock::SecondsToMillis(2));

    // Update the existing state 2 seconds before turning off heat.
    fan_.Maintain(&settings_);

    clock_.SetMillis(Clock::MinutesToMillis(20) - Clock::SecondsToMillis(2));
    // Turn off heat.
    settings_.hvac = HvacMode::IDLE;
    fan_.Maintain(&settings_);

    EXPECT_EQ(settings_.fan, FanMode::ON);
  }

  // Ensure fan runs for 5 additional minutes.
  {
    clock_.SetMillis(Clock::MinutesToMillis(21));
    fan_.Maintain(&settings_);
    EXPECT_EQ(settings_.fan, FanMode::ON);
  }

  {
    clock_.SetMillis(Clock::MinutesToMillis(24));
    fan_.Maintain(&settings_);
    EXPECT_EQ(settings_.fan, FanMode::ON);
  }

  // Fan should be off 6 minutes after hvac on.
  {
    clock_.SetMillis(Clock::MinutesToMillis(26));
    fan_.Maintain(&settings_);
    EXPECT_EQ(settings_.fan, FanMode::OFF);
  }
}

TEST_F(FanControllerTest, DutyRuns) {
  clock_.SetMillis(0);
  fan_.Maintain(&settings_);
  ASSERT_EQ(settings_.fan, FanMode::OFF);

  // Fan should cycle on if over the fan_on_min_period (120 mins).
  {
    clock_.SetMillis(Clock::HoursToMillis(3) - Clock::SecondsToMillis(2));
    fan_.Maintain(&settings_);

    clock_.SetMillis(Clock::HoursToMillis(3));
    fan_.Maintain(&settings_);
    EXPECT_EQ(settings_.fan, FanMode::ON);
  }

  // After 29 minutes, the fan should still run.
  {
    clock_.Increment(Clock::MinutesToMillis(29));
    fan_.Maintain(&settings_);
    EXPECT_EQ(settings_.fan, FanMode::ON);
  }

  // Fan should still be running at 35 minutes.
  {
    clock_.Increment(Clock::MinutesToMillis(6));
    fan_.Maintain(&settings_);
    EXPECT_EQ(settings_.fan, FanMode::ON);
  }

  // After >36 minutes (120m x .3), the fan should have stopped.
  {
    clock_.Increment(Clock::MinutesToMillis(2));
    fan_.Maintain(&settings_);
    EXPECT_EQ(settings_.fan, FanMode::OFF);
  }

  // After >3 more hours, it should run again.
  {
    clock_.Increment(Clock::HoursToMillis(3) - Clock::SecondsToMillis(2));
    fan_.Maintain(&settings_);

    clock_.SetMillis(Clock::HoursToMillis(3));
    fan_.Maintain(&settings_);
    EXPECT_EQ(settings_.fan, FanMode::ON);
  }
}

}  // namespace thermostat
