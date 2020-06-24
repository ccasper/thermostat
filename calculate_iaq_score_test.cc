#include <gtest/gtest.h>
#include <stdint.h>

#include <cmath>

#include "calculate_iaq_score.h"

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

TEST(CalculateIaqScore, WithTypicalValues) {
  {
    const float score = CalculateIaqScore(0, 0);
    EXPECT_LT(score, 10.0) << "poor gas, no humidity";
  }
  {
    const float score = CalculateIaqScore(0, 50000);
    EXPECT_GT(score, 60.0) << "excellent gas, no humidity";
    EXPECT_LT(score, 90.0) << "excellent gas, no humidity";
  }
  {
    const float score = CalculateIaqScore(100, 50000);
    EXPECT_GT(score, 60.0) << "high humidity, excellent gas";
    EXPECT_LT(score, 90.0) << "high humidity, excellent gas";
  }

  {
    const float score = CalculateIaqScore(45, 50000);
    EXPECT_GT(score, 95.0) << "ideal humidity, excellent gas";
  }

  {
    const float score = CalculateIaqScore(45, 5000);
    EXPECT_LT(score, 30.0) << "ideal humidity, poor gas";
  }

  {
    const float score = CalculateIaqScore(45.0, 10000);
    EXPECT_LT(score, 60) << "ideal humidity, moderate gas";
    EXPECT_GT(score, 40) << "ideal humidity, moderate gas";
  }
}
