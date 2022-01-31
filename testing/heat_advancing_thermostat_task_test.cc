
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

class HeatAdvancingThermostatTaskTest : public testing::Test {
 public:
  void SetUp() override {
    clock.SetMillis(10000);
    // Set the time to 10:10am
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
  
  uint8_t event_ = 0;

  MockThermostatTask wrapper;
  HeatAdvancingThermostatTask task = HeatAdvancingThermostatTask(&wrapper);

};

TEST_F(HeatAdvancingThermostatTaskTest, CallsWrapper) {
  MockThermostatTask wrapper;
  HeatAdvancingThermostatTask task = HeatAdvancingThermostatTask(&wrapper);
  
  // We expect a call through to the wrapper.
  EXPECT_CALL(wrapper, RunOnce(testing::_)).Times(1);
  
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk);   
	
}

TEST_F(HeatAdvancingThermostatTaskTest, HighHeat) {
  settings.persisted.heat_enabled = true;
  settings.persisted.cool_enabled = true;
  
  // Default the curren temp to 69° with heat triggered at 67°.
  settings.current_mean_temperature_x10 = 690;
  settings.persisted.tolerance_x10 = 20;
  
  // 7am-9pm -> 70.0° ; 9pm-7am -> 65°
  settings.persisted.heat_setpoints[0].hour = 7;
  settings.persisted.heat_setpoints[0].temperature_x10 = 700;
  
  // Emulate first run behavior.
  {
    settings.first_run = true;
  
    // Start in heating mode
    settings.hvac = HvacMode::IDLE;
    {
      Event* new_event = &settings.events[event_++];
      new_event->start_time = clock.Millis();
      new_event->hvac = settings.hvac;
    }
    EXPECT_EQ(task.RunOnce(&settings), Status::kOk); 
    EXPECT_EQ(settings.hvac, HvacMode::IDLE);
    
    settings.first_run = false;
  }

  // Start heating at 10 minutes.
  clock.Increment(Clock::MinutesToMillis(10));
  settings.now = clock.Millis();
  settings.hvac = HvacMode::HEAT;
  {
	  Event* new_event = &settings.events[event_++];
	  new_event->start_time = clock.Millis();
	  new_event->hvac = settings.hvac;
  }
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk);  
  EXPECT_EQ(settings.hvac, HvacMode::HEAT);
  EXPECT_FALSE(settings.heat_high);

  // Drop the current temperature after 20 minutes. 
  clock.Increment(Clock::MinutesToMillis(20));
  settings.now = clock.Millis();

  settings.current_mean_temperature_x10 = 670;

  EXPECT_EQ(task.RunOnce(&settings), Status::kOk);  
  EXPECT_EQ(settings.hvac, HvacMode::HEAT);
  EXPECT_TRUE(settings.heat_high);

  // Increase the temp just below heat turning off.
  clock.Increment(Clock::MinutesToMillis(20));
  settings.now = clock.Millis();
  settings.current_mean_temperature_x10 = 699;

  // We should stay in high heat until we're up to the desired temp.
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk);  
  EXPECT_EQ(settings.hvac, HvacMode::HEAT);
  EXPECT_TRUE(settings.heat_high);
}

}  // namespace thermostat
