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

constexpr uint8_t kHvacField = 13;
Settings DefaultSettings() {
  Settings defaults;
  defaults.persisted.version = VERSION;
  // 7am-9pm -> 70.0° ; 9pm-7am -> 68°
  defaults.persisted.heat_setpoints[0].hour = 7;
  defaults.persisted.heat_setpoints[0].temperature_x10 = 700;
  defaults.persisted.heat_setpoints[1].hour = 21;
  defaults.persisted.heat_setpoints[1].temperature_x10 = 680;

  // 7am-9pm -> 77.0° ; 9pm-7am -> 72°
  defaults.persisted.cool_setpoints[0].hour = 7;
  defaults.persisted.cool_setpoints[0].temperature_x10 = 770;
  defaults.persisted.cool_setpoints[1].hour = 21;
  defaults.persisted.cool_setpoints[1].temperature_x10 = 720;
  return defaults;
};

class MaintainHvacTest : public testing::Test {
 public:
  void SetUp() override {
    last_hvac_update = 0;
    clock.SetMillis(10000);
    Date d;
    d.hour = 10;
    d.minute = 10;
    d.day_of_week = 3;
    clock.SetDate(d);
    bme_sensor.SetTemperature(56.789);
    bme_sensor.SetHeaterValue(5000);
    bme_sensor.SetHumidity(50.0);
    dallas_sensor.SetTemperature(70.11);
  };

  void TearDown() override{};

  Settings settings = DefaultSettings();

  ClockStub clock;

  FanController fan{&clock};

  PrintStub print;
  DisplayStub display;
  RelaysStub relays;
  SensorStub bme_sensor;
  SensorStub dallas_sensor;
};

TEST_F(MaintainHvacTest, MaintainHvacDelayed) {
  // bme_sensor.EnableAsyncAssert();

  Error error = MaintainHvac(&settings, &clock, &display, &relays, &fan, &bme_sensor,
                             &dallas_sensor, &print);
  EXPECT_EQ(error, Error::STATUS_OK);

  // Rerunning too soon.
  error = MaintainHvac(&settings, &clock, &display, &relays, &fan, &bme_sensor,
                       &dallas_sensor, &print);
  EXPECT_EQ(error, Error::STATUS_NONE);

  // Increment slightly less than than 2.5 seconds.
  clock.Increment(2400);

  // Rerunning too soon.
  error = MaintainHvac(&settings, &clock, &display, &relays, &fan, &bme_sensor,
                       &dallas_sensor, &print);
  EXPECT_EQ(error, Error::STATUS_NONE);

  // Increment slightly more than than 2.5 seconds.
  clock.Increment(200);

  // Rerunning too soon.
  error = MaintainHvac(&settings, &clock, &display, &relays, &fan, &bme_sensor,
                       &dallas_sensor, &print);
  EXPECT_EQ(error, Error::STATUS_OK);
}

TEST_F(MaintainHvacTest, HeatEnabled) {
  EXPECT_EQ(MaintainHvac(&settings, &clock, &display, &relays, &fan, &bme_sensor,
                         &dallas_sensor, &print),
            Error::STATUS_OK);

  // Increment slightly more than 2.5 seconds.
  clock.Increment(3000);

  // We should run through the full HVAC code.
  EXPECT_EQ(MaintainHvac(&settings, &clock, &display, &relays, &fan, &bme_sensor,
                         &dallas_sensor, &print),
            Error::STATUS_OK);

  // Verify the display output.
  char string[17];
  EXPECT_STREQ(display.GetString(string, 0, 0, 4), "70.1");
  EXPECT_EQ(display.GetChar(0, 4), '\0');  // Degree symbol.
  EXPECT_STREQ(display.GetString(string, 0, 5, 11), " 50.0%  __ ");

  // Increment slightly more than 2.5 seconds.
  clock.Increment(3000);

  // Set the setpoint temperature higher.
  settings.persisted.heat_setpoints[0].temperature_x10 = 800;
  settings.persisted.heat_setpoints[1].temperature_x10 = 800;

  // Run the next cycle.
  EXPECT_EQ(MaintainHvac(&settings, &clock, &display, &relays, &fan, &bme_sensor,
                         &dallas_sensor, &print),
            Error::STATUS_OK);

  // Verify that heat was enabled.
  EXPECT_EQ(display.GetChar(0, kHvacField), 'H');
  EXPECT_TRUE(settings.heat_running);

  // Increment slightly more than 2.5 seconds.
  clock.Increment(3000);

  // Set the setpoint temperature lower.
  settings.persisted.heat_setpoints[0].temperature_x10 = 600;
  settings.persisted.heat_setpoints[1].temperature_x10 = 600;

  // Run the next cycle.
  EXPECT_EQ(MaintainHvac(&settings, &clock, &display, &relays, &fan, &bme_sensor,
                         &dallas_sensor, &print),
            Error::STATUS_OK);

  // Verify that heat was disabled.
  EXPECT_FALSE(settings.heat_running);
  EXPECT_EQ(display.GetChar(0, kHvacField), '_');
}

TEST_F(MaintainHvacTest, CoolEnabled) {
  settings.persisted.cool_enabled = true;

  Error error = MaintainHvac(&settings, &clock, &display, &relays, &fan, &bme_sensor,
                             &dallas_sensor, &print);
  EXPECT_EQ(error, Error::STATUS_OK);

  // Rerunning too soon.
  error = MaintainHvac(&settings, &clock, &display, &relays, &fan, &bme_sensor,
                       &dallas_sensor, &print);
  EXPECT_EQ(error, Error::STATUS_NONE);

  // Increment slightly more than 2.5 seconds.
  clock.Increment(2501);

  // We should run through the full HVAC code.
  error = MaintainHvac(&settings, &clock, &display, &relays, &fan, &bme_sensor,
                       &dallas_sensor, &print);
  EXPECT_EQ(error, Error::STATUS_OK);

  // Verify the display output.
  char string[17];
  EXPECT_STREQ(display.GetString(string, 0, 0, 4), "70.1");
  EXPECT_EQ(display.GetChar(0, 4), '\0');  // Degree symbol.
  EXPECT_STREQ(display.GetString(string, 0, 5, 11), " 50.0%  __ ");

  // Increment slightly more than 2.5 seconds.
  clock.Increment(3000);

  // Set the setpoint temperature higher.
  settings.persisted.cool_setpoints[0].temperature_x10 = 600;
  settings.persisted.cool_setpoints[1].temperature_x10 = 600;

  // Run the next cycle.
  EXPECT_EQ(MaintainHvac(&settings, &clock, &display, &relays, &fan, &bme_sensor,
                         &dallas_sensor, &print),
            Error::STATUS_OK);

  // Verify that cool was enabled.
  EXPECT_EQ(display.GetChar(0, kHvacField), 'C');
  EXPECT_TRUE(settings.cool_running);

  // Increment slightly more than 2.5 seconds.
  clock.Increment(3000);

  // Set the setpoint temperature lower.
  settings.persisted.cool_setpoints[0].temperature_x10 = 800;
  settings.persisted.cool_setpoints[1].temperature_x10 = 800;

  // Run the next cycle.
  EXPECT_EQ(MaintainHvac(&settings, &clock, &display, &relays, &fan, &bme_sensor,
                         &dallas_sensor, &print),
            Error::STATUS_OK);

  // Verify that heat was disabled.
  EXPECT_FALSE(settings.cool_running);
  EXPECT_EQ(display.GetChar(0, kHvacField), '_');
}

TEST_F(MaintainHvacTest, CoolThenHeat) {
  // Prime the async requests.
  EXPECT_EQ(MaintainHvac(&settings, &clock, &display, &relays, &fan, &bme_sensor,
                         &dallas_sensor, &print),
            Error::STATUS_OK);
  clock.Increment(3000);  // 3 seconds off.

  settings.persisted.cool_enabled = true;
  settings.persisted.heat_enabled = true;

  // Force cool on.
  settings.persisted.cool_setpoints[0].temperature_x10 = 650;
  settings.persisted.cool_setpoints[1].temperature_x10 = 650;
  // Heat off.
  settings.persisted.heat_setpoints[0].temperature_x10 = 600;
  settings.persisted.heat_setpoints[1].temperature_x10 = 600;

  // 3 seconds of cooling.
  EXPECT_EQ(MaintainHvac(&settings, &clock, &display, &relays, &fan, &bme_sensor,
                         &dallas_sensor, &print),
            Error::STATUS_OK);
  clock.Increment(3000);  // 3 seconds on.

  EXPECT_TRUE(settings.cool_running);
  EXPECT_FALSE(settings.heat_running);

  // Cooling off.
  settings.persisted.cool_setpoints[0].temperature_x10 = 800;
  settings.persisted.cool_setpoints[1].temperature_x10 = 800;

  // Heat on.
  settings.persisted.heat_setpoints[0].temperature_x10 = 750;
  settings.persisted.heat_setpoints[1].temperature_x10 = 750;

  // Attempt to heat, forced into lockout (3 seconds off).
  EXPECT_EQ(MaintainHvac(&settings, &clock, &display, &relays, &fan, &bme_sensor,
                         &dallas_sensor, &print),
            Error::STATUS_OK);
  clock.Increment(10 * 60 * 1000);  // 10 minutes off.

  // Verify that heat is in lockout.
  EXPECT_EQ(display.GetChar(0, kHvacField), 'h');
  EXPECT_FALSE(settings.heat_running);
  EXPECT_FALSE(settings.cool_running);

  // Attempt heating again.
  EXPECT_EQ(MaintainHvac(&settings, &clock, &display, &relays, &fan, &bme_sensor,
                         &dallas_sensor, &print),
            Error::STATUS_OK);

  // Verify that heat is enabled.
  EXPECT_EQ(display.GetChar(0, kHvacField), 'H');
  EXPECT_TRUE(settings.heat_running);
  EXPECT_FALSE(settings.cool_running);
}

TEST_F(MaintainHvacTest, HeatThenCool) {
  // Prime the async requests.
  EXPECT_EQ(MaintainHvac(&settings, &clock, &display, &relays, &fan, &bme_sensor,
                         &dallas_sensor, &print),
            Error::STATUS_OK);
  clock.Increment(3000);  // 3 seconds off.

  settings.persisted.cool_enabled = true;
  settings.persisted.heat_enabled = true;

  // Force heat on.
  settings.persisted.heat_setpoints[0].temperature_x10 = 800;
  settings.persisted.heat_setpoints[1].temperature_x10 = 800;

  // Heat on @3000 ms.
  EXPECT_EQ(MaintainHvac(&settings, &clock, &display, &relays, &fan, &bme_sensor,
                         &dallas_sensor, &print),
            Error::STATUS_OK);
  clock.Increment(3000);  // 3 seconds on.

  EXPECT_TRUE(settings.heat_running);
  EXPECT_FALSE(settings.cool_running);

  // Heat off,
  settings.persisted.heat_setpoints[0].temperature_x10 = 600;
  settings.persisted.heat_setpoints[1].temperature_x10 = 600;
  // Cooling on.
  settings.persisted.cool_setpoints[0].temperature_x10 = 620;
  settings.persisted.cool_setpoints[1].temperature_x10 = 620;

  // Switch from heating to cooling
  EXPECT_EQ(MaintainHvac(&settings, &clock, &display, &relays, &fan, &bme_sensor,
                         &dallas_sensor, &print),
            Error::STATUS_OK);
  clock.Increment(4 * 60 * 1000);  // 4 minutes off.

  // Check the events queue.
  uint8_t events;
  EXPECT_EQ(CalculateSeconds(true, settings, &events, clock.Millis()), 3);
  EXPECT_EQ(CalculateSeconds(false, settings, &events, clock.Millis()), 3 + 4 * 60);
  EXPECT_EQ(events, 2);

  // Verify that cool is in lockout.
  EXPECT_EQ(display.GetChar(0, kHvacField), 'c');
  EXPECT_FALSE(settings.heat_running);
  EXPECT_FALSE(settings.cool_running);

  // Should still be in lockout 4 minutes later.
  EXPECT_EQ(MaintainHvac(&settings, &clock, &display, &relays, &fan, &bme_sensor,
                         &dallas_sensor, &print),
            Error::STATUS_OK);
  // Increment 2 minutes
  clock.Increment(2 * 60 * 1000);  // 2 minutes off

  // Verify that cool is in lockout.
  EXPECT_EQ(display.GetChar(0, kHvacField), 'c');

  // Start cooling.
  EXPECT_EQ(MaintainHvac(&settings, &clock, &display, &relays, &fan, &bme_sensor,
                         &dallas_sensor, &print),
            Error::STATUS_OK);
  clock.Increment(6000);  // 6 seconds on.

  // Verify that cool is enabled.
  EXPECT_EQ(display.GetChar(0, kHvacField), 'C');
  EXPECT_FALSE(settings.heat_running);
  EXPECT_TRUE(settings.cool_running);

  // Check seconds running in the events queue.
  EXPECT_EQ(events, 2);
  EXPECT_EQ(CalculateSeconds(true, settings, &events, clock.Millis()), 3 + 6);
  // Check seconds not running in the events queue.
  EXPECT_EQ(CalculateSeconds(false, settings, &events, clock.Millis()), 6 * 60 + 3);
}

TEST_F(MaintainHvacTest, TemperatureStability) {
  // Prime the async requests.
  EXPECT_EQ(MaintainHvac(&settings, &clock, &display, &relays, &fan, &bme_sensor,
                         &dallas_sensor, &print),
            Error::STATUS_OK);

  settings.persisted.cool_enabled = true;
  settings.persisted.heat_enabled = true;

  for (int i = 0; i < 20; ++i) {
    // Have a spike high and low in temperature, otherwise 70.0.
    int temp_x10 = 700;
    {
      if (i == 10) {
        temp_x10 = 999;
      }
      if (i == 15) {
        temp_x10 = 400;
      }
      dallas_sensor.SetTemperature(temp_x10 / 10.0);
    }

    // Increment 1 minute.
    clock.Increment(1 * 60 * 1000);
    EXPECT_EQ(MaintainHvac(&settings, &clock, &display, &relays, &fan, &bme_sensor,
                           &dallas_sensor, &print),
              Error::STATUS_OK);

    // The spike should normalize out.
    EXPECT_LT(settings.current_mean_temperature_x10, 750);
    EXPECT_GT(settings.current_mean_temperature_x10, 650);

    // The current temperature should match set temperature.
    EXPECT_EQ(settings.current_temperature_x10, temp_x10);
  }
}

}  // namespace thermostat
