#include <glog/logging.h>
#include <gtest/gtest.h>
#include <stdint.h>

#include <cmath>

#include "thermostat/buttons.h"

#include "absl/strings/str_cat.h"

namespace thermostat {
namespace {

TEST(ButtonsTest, GetButton) {
  EXPECT_EQ(Button::NONE, Buttons::GetButton(1023));
  EXPECT_EQ(Button::RIGHT, Buttons::GetButton(50));
  EXPECT_EQ(Button::UP, Buttons::GetButton(150));
  EXPECT_EQ(Button::DOWN, Buttons::GetButton(350));
  EXPECT_EQ(Button::LEFT, Buttons::GetButton(500));
  EXPECT_EQ(Button::SELECT, Buttons::GetButton(700));
}

TEST(ButtonsTest, GetSingleButton) {
  Buttons::GetSinglePress(Button::NONE, 0);

  EXPECT_EQ(Buttons::GetSinglePress(Button::LEFT, 0), Button::LEFT);
  EXPECT_EQ(Buttons::GetSinglePress(Button::LEFT, 0), Button::NONE);

  // Every 250 ms after 1 second of holding.
  EXPECT_EQ(Buttons::GetSinglePress(Button::LEFT, 900), Button::NONE);
  EXPECT_EQ(Buttons::GetSinglePress(Button::LEFT, 1000), Button::LEFT);
  EXPECT_EQ(Buttons::GetSinglePress(Button::LEFT, 1200), Button::NONE);
  EXPECT_EQ(Buttons::GetSinglePress(Button::LEFT, 1250), Button::LEFT);

  // Every 25 ms after 5 seconds of holding.
  EXPECT_EQ(Buttons::GetSinglePress(Button::LEFT, 5000), Button::LEFT);
  EXPECT_EQ(Buttons::GetSinglePress(Button::LEFT, 5020), Button::NONE);
  EXPECT_EQ(Buttons::GetSinglePress(Button::LEFT, 5025), Button::LEFT);
}

TEST(ButtonTest, StabilizedButtonPressed) {
  // Ensure we're stabilized since this uses a static.
  for (int i = 0; i < 10; ++i) {
    Buttons::StabilizedButtonPressed(Button::NONE);
  }

  // Oscillating input should delay stabilization.
  EXPECT_EQ(Buttons::StabilizedButtonPressed(Button::LEFT), Button::NONE);
  EXPECT_EQ(Buttons::StabilizedButtonPressed(Button::RIGHT), Button::NONE);
  EXPECT_EQ(Buttons::StabilizedButtonPressed(Button::NONE), Button::NONE);

  // After several matching button presses, it should stabilize.
  EXPECT_EQ(Buttons::StabilizedButtonPressed(Button::LEFT), Button::NONE);
  EXPECT_EQ(Buttons::StabilizedButtonPressed(Button::LEFT), Button::NONE);
  EXPECT_EQ(Buttons::StabilizedButtonPressed(Button::LEFT), Button::LEFT);

  // Same behavior going back to none pressed.
  EXPECT_EQ(Buttons::StabilizedButtonPressed(Button::NONE), Button::LEFT);
  EXPECT_EQ(Buttons::StabilizedButtonPressed(Button::NONE), Button::LEFT);
  EXPECT_EQ(Buttons::StabilizedButtonPressed(Button::NONE), Button::NONE);
}

}  // namespace
}  // namespace thermostat
