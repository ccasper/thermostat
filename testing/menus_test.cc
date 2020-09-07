#include <glog/logging.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdint.h>

#include <cmath>
#include <functional>

#include "absl/strings/str_cat.h"
#include "testing/mock_impls.h"
#include "thermostat/buttons.h"
#include "thermostat/interfaces.h"
#include "thermostat/menus.h"
#include "thermostat/print.h"
#include "thermostat/settings.h"

namespace thermostat {
namespace {

using ::testing::_;
using namespace std::placeholders;

class MockSettingsStorer : public SettingsStorer {
 public:
  MOCK_METHOD1(Write, void(const Settings& settings));
  MOCK_METHOD1(Read, void(Settings* settings));
};

static int counter;
static ClockStub clock;
static DisplayStub display;

class MenusTest : public testing::Test {
 public:
  void SetUp() override {
    clock.SetMillis(10000);
    Date d;
    d.hour = 10;
    d.minute = 10;
    d.day_of_week = 3;
    clock.SetDate(d);

    // Setup the settings.
    settings.persisted.version = VERSION;
    // 7am-9pm -> 70.0° ; 9pm-7am -> 68°
    settings.persisted.heat_setpoints[0].hour = 7;
    settings.persisted.heat_setpoints[0].temperature_x10 = 700;
    settings.persisted.heat_setpoints[1].hour = 21;
    settings.persisted.heat_setpoints[1].temperature_x10 = 680;

    // 7am-9pm -> 77.0° ; 9pm-7am -> 72°
    settings.persisted.cool_setpoints[0].hour = 7;
    settings.persisted.cool_setpoints[0].temperature_x10 = 770;
    settings.persisted.cool_setpoints[1].hour = 21;
    settings.persisted.cool_setpoints[1].temperature_x10 = 720;
  };

  void TearDown() override{};

  Settings settings;
  PrintStub print;
  MockSettingsStorer mock_storer;
};

TEST_F(MenusTest, EditExitsWithLeftPress) {
  Menus menu = Menus(
      &settings, [](uint32_t) -> Button { return Button::LEFT; }, &clock, &display,
      &mock_storer);
  menu.EditSettings();
}

static Button CycleThroughEditSettings(uint32_t timeout) {
  char string[17];
  ++counter;
  switch (counter) {
    case 0:
      return Button::RIGHT;
    case 1:
    case 11:
      EXPECT_STREQ(display.GetString(string, 1, 0, 16), "Humidify: 30%   ");
      if (counter == 11) {
        return Button::LEFT;
      }
      break;
    case 2:
      EXPECT_STREQ(display.GetString(string, 1, 0, 16), "Fan: EXT:005m   ");
      break;
    case 3:
      EXPECT_STREQ(display.GetString(string, 1, 0, 16), "Mode:BOTH       ");
      break;
    case 4:
      EXPECT_STREQ(display.GetString(string, 1, 0, 16), "H1:70.0\xA7 07:00  ");
      break;
    case 5:
      EXPECT_STREQ(display.GetString(string, 1, 0, 16), "H2:68.0\xA7 21:00  ");
      break;
    case 6:
      EXPECT_STREQ(display.GetString(string, 1, 0, 16), "C1:77.0\xA7 07:00  ");
      break;
    case 7:
      EXPECT_STREQ(display.GetString(string, 1, 0, 16), "C2:72.0\xA7 21:00  ");
      break;
    case 8:
      EXPECT_STREQ(display.GetString(string, 1, 0, 16), "Tolerance: 01.5\xA7");
      break;
    case 9:
      EXPECT_STREQ(display.GetString(string, 1, 0, 16), "Date: 10:10 We  ");
      break;
    case 10:
      EXPECT_STREQ(display.GetString(string, 1, 0, 16), "Fan dt:180m 15% ");
      break;
    default:
      EXPECT_FALSE(true);
      return Button::NONE;
  }
  return Button::RIGHT;
}

TEST_F(MenusTest, CycleThroughEditSettings) {
  counter = 0;
  clock.SetMillis(0);

  Menus menu =
      Menus(&settings, &CycleThroughEditSettings, &clock, &display, &mock_storer);
  menu.EditSettings();
}

static Button SetpointEdit(uint32_t timeout) {
  char string[17];
  ++counter;
  if (counter < 4) {
    return Button::RIGHT;
  }
  int seq = 4;
  if (counter == seq++) {
    EXPECT_STREQ(display.GetString(string, 1, 0, 16), "H1:70.0\xA7 07:00  ");
    return Button::SELECT;
  }
  if (counter == seq++) {
    EXPECT_STREQ(display.GetString(string, 1, 0, 16), "H1:__._\xA7 07:00  ");
    clock.Increment(600);
    return Button::UP;
  }
  if (counter == seq++) {
    EXPECT_STREQ(display.GetString(string, 1, 0, 16), "H1:70.1\xA7 07:00  ");
    return Button::DOWN;
  }
  if (counter == seq++) {
    EXPECT_STREQ(display.GetString(string, 1, 0, 16), "H1:70.0\xA7 07:00  ");
    // Move to the next field.
    return Button::RIGHT;
  }
  if (counter == seq++) {
    EXPECT_STREQ(display.GetString(string, 1, 0, 16), "H1:70.0\xA7 __:00  ");
    return Button::UP;
  }
  if (counter == seq++) {
    EXPECT_STREQ(display.GetString(string, 1, 0, 16), "H1:70.0\xA7 08:00  ");
    return Button::DOWN;
  }
  if (counter == seq++) {
    EXPECT_STREQ(display.GetString(string, 1, 0, 16), "H1:70.0\xA7 07:00  ");
    // Move to the next field.
    return Button::RIGHT;
  }
  if (counter == seq++) {
    EXPECT_STREQ(display.GetString(string, 1, 0, 16), "H1:70.0\xA7 07:__  ");
    return Button::UP;
  }
  if (counter == seq++) {
    EXPECT_STREQ(display.GetString(string, 1, 0, 16), "H1:70.0\xA7 07:01  ");
    return Button::DOWN;
  }
  if (counter == seq++) {
    EXPECT_STREQ(display.GetString(string, 1, 0, 16), "H1:70.0\xA7 07:00  ");
    return Button::SELECT;
  }
  if (counter == seq++) {
    EXPECT_STREQ(display.GetString(string, 1, 0, 16), "Updated...      ");
  }
  // Exit Menu.
  return Button::LEFT;
}

TEST_F(MenusTest, SetpointEdit) {
  counter = 0;
  clock.SetMillis(0);

  // Persisting the settings should get called once.
  EXPECT_CALL(mock_storer, Write(_)).Times(1);

  Menus menu = Menus(&settings, &SetpointEdit, &clock, &display, &mock_storer);
  menu.EditSettings();
}

}  // namespace
}  // namespace thermostat
