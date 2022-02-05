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

class HvacControllerThermostatTaskTest : public testing::Test {
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
  HvacControllerThermostatTask task = HvacControllerThermostatTask(&clock, &print, &wrapper);

};

TEST_F(HvacControllerThermostatTaskTest, CallsWrapper) {
  MockThermostatTask wrapper;
  HvacControllerThermostatTask task = HvacControllerThermostatTask(&clock, &print, &wrapper);
  
  // We expect a call through to the wrapper.
  EXPECT_CALL(wrapper, RunOnce(testing::_)).Times(1);
  
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk);   
	
}

TEST_F(HvacControllerThermostatTaskTest, HeatDisabled) {
  settings.persisted.heat_enabled = false;
  settings.persisted.cool_enabled = false;

  // Set the temperature lower than the 70.
  settings.current_mean_temperature_x10 = 60.0;

  EXPECT_EQ(task.RunOnce(&settings), Status::kOk);
 
  EXPECT_EQ(settings.hvac, HvacMode::IDLE);
}

TEST_F(HvacControllerThermostatTaskTest, HeatEnabled) {
  settings.persisted.heat_enabled = true;
  settings.persisted.cool_enabled = false;

  EXPECT_EQ(task.RunOnce(&settings), Status::kOk); 
  EXPECT_EQ(settings.hvac, HvacMode::IDLE);

  settings.current_mean_temperature_x10 = 600;

  EXPECT_EQ(task.RunOnce(&settings), Status::kOk); 
  EXPECT_EQ(settings.hvac, HvacMode::HEAT);

  // Set the temperature equal to the current setpoint.
  settings.current_mean_temperature_x10 = 700;
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk); 
  EXPECT_EQ(settings.hvac, HvacMode::HEAT);

  // Set the temperature within the tolerance.
  settings.current_mean_temperature_x10 = 710;
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk); 
  EXPECT_EQ(settings.hvac, HvacMode::HEAT);

  // Set the temperature above to the setpoint tolerance.
  settings.current_mean_temperature_x10 = 720;
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk); 
  EXPECT_EQ(settings.hvac, HvacMode::IDLE);

  // Set the setpoint temperature higher.
  settings.persisted.heat_setpoints[0].temperature_x10 = 800;
  settings.persisted.heat_setpoints[1].temperature_x10 = 800;

  // Run the next cycle.
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk);
  EXPECT_EQ(settings.hvac, HvacMode::HEAT);

  // Set the setpoint temperature lower.
  settings.persisted.heat_setpoints[0].temperature_x10 = 600;
  settings.persisted.heat_setpoints[1].temperature_x10 = 600;

  // Run the next cycle.
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk);

  // Verify that heat was disabled.
  EXPECT_EQ(settings.hvac, HvacMode::IDLE);
}

TEST_F(HvacControllerThermostatTaskTest, CoolEnabled) {
  settings.persisted.heat_enabled = false;
  settings.persisted.cool_enabled = true;

  EXPECT_EQ(task.RunOnce(&settings), Status::kOk); 
  EXPECT_EQ(settings.hvac, HvacMode::IDLE);

  // Set above the cool tolerance.
  settings.current_mean_temperature_x10 = 810;
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk); 
  EXPECT_EQ(settings.hvac, HvacMode::COOL);

  // Set the temperature equal to the current setpoint.
  settings.current_mean_temperature_x10 = 800;
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk); 
  EXPECT_EQ(settings.hvac, HvacMode::COOL);

  // Set the temperature within the tolerance.
  settings.current_mean_temperature_x10 = 790;
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk); 
  EXPECT_EQ(settings.hvac, HvacMode::COOL);

  // Set the temperature above to the setpoint tolerance.
  settings.current_mean_temperature_x10 = 770;
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk); 
  EXPECT_EQ(settings.hvac, HvacMode::IDLE);

  // Set the setpoint temperature lower.
  settings.persisted.cool_setpoints[0].temperature_x10 = 750;
  settings.persisted.cool_setpoints[1].temperature_x10 = 750;

  // Cooling should still be enabled.
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk);
  EXPECT_EQ(settings.hvac, HvacMode::COOL);

  // Set the setpoint temperature higher.
  settings.persisted.cool_setpoints[0].temperature_x10 = 900;
  settings.persisted.cool_setpoints[1].temperature_x10 = 900;

  // Verify that heat was disabled.
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk);
  EXPECT_EQ(settings.hvac, HvacMode::IDLE);
}

TEST_F(HvacControllerThermostatTaskTest, HeatAndCoolToggling) {
  settings.persisted.heat_enabled = true;
  settings.persisted.cool_enabled = true;
  
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk); 
  EXPECT_EQ(settings.hvac, HvacMode::IDLE);

  // Set the current temperature below the heating tolerance.
  settings.current_mean_temperature_x10 = 600;

  // We should be heating.
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk); 
  EXPECT_EQ(settings.hvac, HvacMode::HEAT);

  // Set the current temperature above the cooling tolerance.
  settings.current_mean_temperature_x10 = 801;
  
  // Verify that cool is enabled.
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk);
  EXPECT_EQ(settings.hvac, HvacMode::COOL); 

  // Set the current temperature below the heating tolerance.
  // Note: this test case doesn't test lockout.
  settings.current_mean_temperature_x10 = 600;

  // We should be heating.
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk); 
  EXPECT_EQ(settings.hvac, HvacMode::HEAT);

  // Set the temperature within the idle window.
  settings.current_mean_temperature_x10 = 720;
  EXPECT_EQ(task.RunOnce(&settings), Status::kOk); 
  EXPECT_EQ(settings.hvac, HvacMode::IDLE);
}

}  // namespace thermostat
