#include <glog/logging.h>
#include <gtest/gtest.h>

#include <cmath>

#include "thermostat/comparison.h"

namespace thermostat {
namespace {

TEST(ComparisonTest, Max) {
  EXPECT_EQ(cmax(1,20), 20);
  EXPECT_EQ(cmax(20,20), 20);
  EXPECT_EQ(cmax(100,2), 100);
  EXPECT_EQ(cmax(100,-100), 100);
  EXPECT_EQ(cmax(-100,100), 100);
}

TEST(ComparisonTest, Min) {
  EXPECT_EQ(cmin(1,20), 1);
  EXPECT_EQ(cmin(20,20), 20);
  EXPECT_EQ(cmin(100,2), 2);
  EXPECT_EQ(cmin(100,-100), -100);
  EXPECT_EQ(cmin(-100,100), -100);
}
}  // namespace
}  // namespace thermostat
