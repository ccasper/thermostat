
#include <glog/logging.h>
#include <gtest/gtest.h>
#include <stdint.h>

#include <cmath>

#include "thermostat/print.h"

#include "absl/strings/str_cat.h"

namespace thermostat {
namespace {

class PrintStub : public Print {
 public:
  // Write the characters to the array.
  void write(uint8_t ch) override {
    assert(index < 100);
    arr[index++] = ch;
  };

  char arr[100] = {0};

 private:
  uint8_t index = 0;
};

TEST(PrintTest, Integer) {
  {
    PrintStub print;
    print.print(static_cast<int>(-23));
    EXPECT_STREQ(print.arr, "-23");
  }
  {
    PrintStub print;
    print.print(static_cast<int>(23));
    EXPECT_STREQ(print.arr, "23");
  }
  {
    PrintStub print;
    print.print(static_cast<int>(0));
    EXPECT_STREQ(print.arr, "0");
  }
  {
    PrintStub print;
    print.print(static_cast<int>(INT_MAX));
    EXPECT_EQ(print.arr, absl::StrCat(INT_MAX));
  }
  {
    PrintStub print;
    print.print(static_cast<int>(INT_MIN));
    EXPECT_EQ(print.arr, absl::StrCat(INT_MIN));
  }
}

TEST(PrintTest, UnsignedInteger) {
  {
    PrintStub print;
    print.print(static_cast<unsigned int>(23));
    EXPECT_STREQ(print.arr, "23");
  }
  {
    PrintStub print;
    print.print(static_cast<unsigned int>(0));
    EXPECT_STREQ(print.arr, "0");
  }
  {
    PrintStub print;
    print.print(static_cast<unsigned int>(UINT_MAX));
    EXPECT_EQ(print.arr, absl::StrCat(UINT_MAX));
  }
}

TEST(PrintTest, Long) {
  // We're testing "long", but x86 long is 8 bytes, and avr is 4 bytes.
  // Therefore we use int32_t instead.
  {
    PrintStub print;
    print.print(static_cast<int32_t>(23));
    EXPECT_STREQ(print.arr, "23");
  }
  {
    PrintStub print;
    print.print(static_cast<int32_t>(0));
    EXPECT_STREQ(print.arr, "0");
  }
  {
    PrintStub print;
    print.print(static_cast<int32_t>(INT32_MAX));
    EXPECT_EQ(print.arr, absl::StrCat(INT32_MAX));
  }
  {
    PrintStub print;
    print.print(static_cast<int32_t>(INT32_MIN));
    EXPECT_EQ(print.arr, absl::StrCat(INT32_MIN));
  }
}

TEST(PrintTest, Float) {
  // We're testing "long", but x86 long is 8 bytes, and avr is 4 bytes.
  // Therefore we use int32_t instead.
  {
    PrintStub print;
    print.print(static_cast<double>(23.45));
    EXPECT_STREQ(print.arr, "23.45");
  }
  {
    PrintStub print;
    print.print(static_cast<double>(0));
    EXPECT_STREQ(print.arr, "0.0");
  }
  {
    PrintStub print;
    print.print(static_cast<double>(-23.54));
    EXPECT_STREQ(print.arr, "-23.54");
  }
}
TEST(PrintTest, FloatRoundsNearest) {
  {
    PrintStub print;
    print.print(static_cast<double>(12.346));
    EXPECT_STREQ(print.arr, "12.35");
  }
  {
    PrintStub print;
    print.print(static_cast<double>(12.999));
    EXPECT_STREQ(print.arr, "13.0");
  }
}

}  // namespace
}  // namespace thermostat
