#include <glog/logging.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdint.h>

#include <cmath>

#include "mock_impls.h"
#include "thermostat/comparison.h"
#include "thermostat/interfaces.h"
#include "thermostat/thermostat_tasks.h"
#include "thermostat/settings.h"

namespace thermostat {

namespace t = testing;

constexpr uint8_t kHvacField = 13;

Settings DefaultSettings() {
  Settings defaults;
  defaults.persisted.version = VERSION;
  // Default in between the heat and cool ranges.
  defaults.current_mean_temperature_x10 = 720;
  defaults.persisted.tolerance_x10 = 20;
  
  // 7am-9pm -> 70.0° ; 9pm-7am -> 65°
  defaults.persisted.heat_setpoints[0].hour = 7;
  defaults.persisted.heat_setpoints[0].temperature_x10 = 700;
  defaults.persisted.heat_setpoints[1].hour = 21;
  defaults.persisted.heat_setpoints[1].temperature_x10 = 650;

  // 7am-9pm -> 80.0° ; 9pm-7am -> 75°
  defaults.persisted.cool_setpoints[0].hour = 7;
  defaults.persisted.cool_setpoints[0].temperature_x10 = 800;
  defaults.persisted.cool_setpoints[1].hour = 21;
  defaults.persisted.cool_setpoints[1].temperature_x10 = 750;
  return defaults;
};

class LockoutControllingThermostatTaskTest : public testing::Test {
 public:
  void SetUp() override {
    clock.SetMillis(10000);
    Date d;
    d.hour = 10;
    d.minute = 10;
    d.day_of_week = 3;
    clock.SetDate(d);
    
    // Cover the default case of the wrapper RunOnce being called.
    EXPECT_CALL(wrapper, RunOnce(t::_)).Times(t::AtLeast(0));
  };

  void TearDown() override{};

  Settings settings = DefaultSettings();

  FakeClock clock;

  FakePrint print;

  MockThermostatTask wrapper;
  LockoutControllingThermostatTask task = LockoutControllingThermostatTask(&wrapper);

};

TEST_F(LockoutControllingThermostatTaskTest, CallsWrapper) {
  MockThermostatTask wrapper;
  LockoutControllingThermostatTask task = LockoutControllingThermostatTask(&wrapper);
  
  // We expect a call through to the wrapper.
  EXPECT_CALL(wrapper, RunOnce(testing::_)).Times(1);
  
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk);   
	
}

TEST_F(LockoutControllingThermostatTaskTest, HeatLockout) {
  settings.persisted.heat_enabled = true;
  settings.persisted.cool_enabled = true;
  
  // Emulate first run behavior.
  settings.first_run = true;
  
  // Try setting HEAT, with 10 min startup lockout hvac will be IDLE.
  settings.hvac = HvacMode::HEAT;
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk); 
  EXPECT_EQ(settings.hvac, HvacMode::HEAT_LOCKOUT);
  settings.first_run = false;

  // Create a cool event starting at 0 min.
  {
	  Event* new_event = &settings.events[0];
	  new_event->start_time = clock.Millis();
	  new_event->hvac = HvacMode::COOL;
  }

  // Try going from cool to heat 20 minutes later, we should see lockout
  clock.Increment(Clock::MinutesToMillis(20));

  // Immediately after cooling we should lockout
  clock.Increment(Clock::MinutesToMillis(1));
  settings.hvac = HvacMode::HEAT;
  settings.now = clock.Millis();
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk); 
  EXPECT_EQ(settings.hvac, HvacMode::HEAT_LOCKOUT);
  
  // Create an idle event startig at 20 min.
  {
	  Event* new_event = &settings.events[1];
	  new_event->start_time = clock.Millis();
	  new_event->hvac = HvacMode::IDLE;
  }
  
  // Immediately after havin gthe idle even we should still lockout.
  clock.Increment(Clock::MinutesToMillis(1));
  settings.hvac = HvacMode::HEAT;
  settings.now = clock.Millis();
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk); 
  EXPECT_EQ(settings.hvac, HvacMode::HEAT_LOCKOUT);

  
  // 1 minute after cooling we should see lockout.
  clock.Increment(Clock::MinutesToMillis(1));
  settings.hvac = HvacMode::HEAT;
  settings.now = clock.Millis();
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk); 
  EXPECT_EQ(settings.hvac, HvacMode::HEAT_LOCKOUT);

  // 5 minutes later, we should be able to heat again.
  clock.Increment(Clock::MinutesToMillis(10));
  settings.hvac = HvacMode::HEAT;
  settings.now = clock.Millis();
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk); 
  EXPECT_EQ(settings.hvac, HvacMode::HEAT);

}

TEST_F(LockoutControllingThermostatTaskTest, CoolLockout) {
  settings.persisted.heat_enabled = true;
  settings.persisted.cool_enabled = true;
  
  // Emulate first run behavior.
  settings.first_run = true;
  
  // Try setting heat, with 10 min startup lockout hvac will be IDLE.
  settings.hvac = HvacMode::HEAT;
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk); 
  EXPECT_EQ(settings.hvac, HvacMode::HEAT_LOCKOUT);
  settings.first_run = false;

  // Create a heat event starting at 0 min.
  {
	  Event* new_event = &settings.events[0];
	  new_event->start_time = clock.Millis();
	  new_event->hvac = HvacMode::HEAT;
  }

  // Try going from heat to cool 20 minutes later, we should see lockout
  clock.Increment(Clock::MinutesToMillis(20));

  // Immediately after heating we should lockout.
  clock.Increment(Clock::MinutesToMillis(1));
  settings.hvac = HvacMode::COOL;
  settings.now = clock.Millis();
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk); 
  EXPECT_EQ(settings.hvac, HvacMode::COOL_LOCKOUT);
  
  // Create an idle event starting at 20 min.
  {
	  Event* new_event = &settings.events[1];
	  new_event->start_time = clock.Millis();
	  new_event->hvac = HvacMode::IDLE;
  }
  
  // Immediately after having the idle event we should still lockout.
  clock.Increment(Clock::MinutesToMillis(1));
  settings.hvac = HvacMode::COOL;
  settings.now = clock.Millis();
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk); 
  EXPECT_EQ(settings.hvac, HvacMode::COOL_LOCKOUT);

  
  // 1 minute after cooling we should see lockout.
  clock.Increment(Clock::MinutesToMillis(1));
  settings.hvac = HvacMode::COOL;
  settings.now = clock.Millis();
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk); 
  EXPECT_EQ(settings.hvac, HvacMode::COOL_LOCKOUT);

  // 5 minutes later, we should be able to cool again.
  clock.Increment(Clock::MinutesToMillis(10));
  settings.hvac = HvacMode::COOL;
  settings.now = clock.Millis();
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk); 
  EXPECT_EQ(settings.hvac, HvacMode::COOL);
}

}  // namespace thermostat
