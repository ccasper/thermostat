#include <glog/logging.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdint.h>

#include <cmath>

#include "mock_impls.h"
#include "thermostat/comparison.h"
#include "thermostat/thermostat_tasks.h"
#include "thermostat/interfaces.h"
#include "thermostat/settings.h"

namespace thermostat {

namespace t = testing;

Settings DefaultSettings() {
  Settings defaults;
  defaults.persisted.version = VERSION;
  defaults.persisted.fan_always_on = false;
  defaults.fan = FanMode::OFF;
  defaults.hvac =  HvacMode::IDLE;
  defaults.persisted.fan_on_min_period = 120;
  defaults.persisted.fan_on_duty = 30;
  defaults.persisted.fan_extend_mins = 0;
  defaults.persisted.heat_enabled = true;
  return defaults;
};

class FanControllerThermostatTaskTest : public testing::Test {
 public:
  void SetUp() override {
	  EXPECT_CALL(wrapper, RunOnce(t::_)).Times(t::AtLeast(0));
  }

 protected:
  Settings settings = DefaultSettings();
  FakeClock clock;
  FakePrint print;

  MockThermostatTask wrapper;
  FanControllerThermostatTask task = FanControllerThermostatTask(&clock, &print, &wrapper);

};

TEST_F(FanControllerThermostatTaskTest, CallsWrapper) {  
  // We expect a call through to the wrapper.
  EXPECT_CALL(wrapper, RunOnce(t::_)).Times(1);
  
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk);   
	
}

TEST_F(FanControllerThermostatTaskTest, EnablesAfterIdleLength) {
  settings.hvac = HvacMode::IDLE;
  
  // Do an update with the fan off.
  clock.SetMillis(Clock::MinutesToMillis(0));
  settings.now = clock.Millis();
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk);   

  // Idle for 120 minutes.
  clock.Increment(Clock::MinutesToMillis(120));
  settings.now = clock.Millis();
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk);   
 
  // The fan should enable after 120 minutes.
  EXPECT_EQ(settings.fan, FanMode::ON);

  clock.Increment(Clock::MinutesToMillis(1));
  settings.now = clock.Millis();
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk);   

  // Fan should still be on.
  EXPECT_EQ(settings.fan, FanMode::ON);


  // Wait >30 minutes.
  clock.Increment(Clock::MinutesToMillis(35));
  settings.now = clock.Millis();
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk);   

  // Fan should turn off due to the duty cycle.
  EXPECT_EQ(settings.fan, FanMode::OFF);

}

TEST_F(FanControllerThermostatTaskTest, FanEnabledWhenExtendingHeatCycle) {
  settings.persisted.fan_extend_mins = 7;

  settings.hvac = HvacMode::HEAT;
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk);   
  
  // Simulate heating for 10 minutes.
  clock.Increment(Clock::MinutesToMillis(10));
  settings.now = clock.Millis();
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk);   

  // Swith to Heating stopped.
  clock.Increment(Clock::SecondsToMillis(2));
  settings.now = clock.Millis();
  settings.hvac = HvacMode::IDLE;
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk);   

  clock.Increment(Clock::SecondsToMillis(2));
  settings.now = clock.Millis();
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk);   

  EXPECT_EQ(settings.fan, FanMode::ON); 

  clock.Increment(Clock::MinutesToMillis(8));
  settings.now = clock.Millis();
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk);   

  EXPECT_EQ(settings.fan, FanMode::OFF); 
}

}  // namespace thermostat
