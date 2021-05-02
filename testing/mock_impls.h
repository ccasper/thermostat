#ifndef MOCK_IMPLS_H_
#define MOCK_IMPLS_H_
#include <gtest/gtest.h>
#include "gmock/gmock.h"  // Brings in gMock.

#include "settings.h"
#include "thermostat/interfaces.h"

namespace thermostat {

class FakeSensor : public Sensor {
 public:
  void SetTemperature(float temp) { temp_ = temp; };
  void EnableAsyncAssert() { enable_async_assert_ = true; };
  void SetHeaterValue(uint32_t temp) { heater_value_ = temp; };
  void StartRequestAsync() override {
    EXPECT_TRUE(!enable_async_assert_ || !request_active_);
    request_active_ = true;
  };

  float GetTemperature() override {
    EXPECT_TRUE(enable_async_assert_ == false || request_active_ == true);
    request_active_ = false;
    return temp_;
  };
  void EnableGasHeater(const bool enable) override { heater_enabled_ = enable; };
  float GetHumidity() override { return humidity_; }
  float GetPressure() override { return 5.432; };

  void SetHumidity(uint32_t humidity) { humidity_ = humidity; }
  uint32_t GetGasResistance() override {
    return heater_value_;
  };

 private:
  float temp_ = 12.3456789;
  float humidity_ = 0.0;
  bool heater_enabled_ = false;
  uint32_t heater_value_ = 0;
  bool request_active_ = false;
  bool enable_async_assert_ = false;
};

class FakeClock : public Clock {
 public:
  uint32_t Millis() const override { return millis_; };
  void Increment(uint32_t millis) { millis_ += millis; }
  void SetMillis(uint32_t millis) { millis_ = millis; };
  void SetDate(const Date &date) { date_ = date; }

  Date Now() override { return date_; }

  void Set(const Date &date) override { date_ = date; }

 private:
  Date date_;
  uint32_t millis_ = 0;
};

class RelaysStub : public Relays {
 public:
  void Set(const RelayType relay, const RelayState relay_state) override {
    relay_[static_cast<int>(relay)] = relay_state;
  };
  RelayState Get(const RelayType relay) { return relay_[static_cast<int>(relay)]; };

 private:
  RelayState relay_[static_cast<int>(RelayType::kMax)];
};


class MockThermostatTask : public ThermostatTask {
 public:
  MOCK_METHOD(Status, RunOnce, (Settings* settings), (override));
};

// Abstract interface for Serial/Debug output.
class FakePrint : public Print {
 public:
  virtual void Setup() {}
  void write(uint8_t ch) override { std::cout << ch; };
};

class FakeDisplay : public Display {
 public:
  FakeDisplay() {
    for (uint8_t r = 0; r < 2; ++r) {
      for (uint8_t c = 0; c < 16; ++c) {
        bytes_[r][c] = ' ';
      }
    }
    // Mark end of string.
    bytes_[0][16] = '\0';
    bytes_[1][16] = '\0';
  };

  void write(uint8_t ch) override {
    if (col_pos_ >= 16) {
      return;
    }
    bytes_[row_pos_][col_pos_] = ch;
    ++col_pos_;
  };

  void SetCursor(const int col, const int row) override {
    LOG(INFO) << "SetCursor: c:" << col << " r:" << row;
    row_pos_ = row;
    col_pos_ = col;
  };

  uint8_t CompareRow(const uint8_t row, char *str_row) { return 0; };

  char *GetString(char *str, const uint8_t row, const uint8_t col, const uint8_t length) {
    for (int i = 0; i < length; ++i) {
      str[i] = GetChar(row, col + i);
      if (str[i] == '\0') {
        str[i] = '\247';  // '°'
      }
      if (str[i] == '\1') {
        str[i] = '\134';  // '\'
      }
    }
    str[length] = '\0';
    return str;
  };
  uint8_t GetChar(const uint8_t row, const uint8_t col) { return bytes_[row][col]; };

 private:
  // Row + Column written.
  uint8_t bytes_[2][17];
  // Where the next character should be written.
  uint8_t row_pos_;
  uint8_t col_pos_;
};

class FakeSettingsStorer : public SettingsStorer {
 public:
  void write(const Settings &settings) { stored_settings = settings; };
  void Read(Settings *settings) override { *settings = stored_settings; };

  Settings stored_settings;
};

}  // namespace thermostat

#endif  // MOCK_IMPLS_H_
