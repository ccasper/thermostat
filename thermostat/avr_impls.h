#ifndef AVR_IMPLS_H_
#define AVR_IMPLS_H_
// This header file implements the AVR and hardware logic needed to run on the actual thermostat.

#include <LiquidCrystal.h>
#include <Adafruit_BME680.h>
#include <DallasTemperature.h>
#include "interfaces.h"
#include "uRTCLib.h"
#include "print.h"

#define ONE_WIRE_BUS 33
#define UNUSED(x) (void)(x)

namespace thermostat {

#ifdef DEV_BOARD
class CannedSensor : public Sensor {
  public:
    CannedSensor(Print *print) : print_(print) {};
    void SetUp() {};
    void StartRequestAsync() override {};
    float GetTemperature() override {
      if (temp_ > 85.0 || temp_ < 60.0) {
        temp_ = 60.0;
      }
      temp_ += .1;
      return temp_;
    };
    float GetHumidity() override {
      return temp_;
    }
  private:
    float temp_;
    Print *print_;
};

class CannedDisplay : public Display {
  public:
    CannedDisplay(Print *print) : print_(print) {
      for (uint8_t r = 0; r < 2; ++r) {
        for (uint8_t c = 0; c < 16; ++c) {
          bytes_[r][c] = ' ';
        }
      }
      // Mark end of string.
      bytes_[0][16] = '\0';
      bytes_[1][16] = '\0';
    };
    void SetUp() {};

    void write(uint8_t ch) override {
      if (col_pos_ >= 16) {
        return;
      }
      bytes_[row_pos_][col_pos_] = ch;
      ++col_pos_;
    };

    void SetCursor(const int col, const int row) override {
      //    LOG(INFO) << "SetCursor: c:" << col << " r:" << row;
      row_pos_ = row;
      col_pos_ = col;
      if (row == col == 0) {
        char str[18];
      }
    }
    uint8_t GetChar(const uint8_t row, const uint8_t col) {
      return bytes_[row][col];
    };

    void Printer() {
      char str[17];
      print_->print("\n================\n");
      GetString(str, 0, 0, 16);
      print_->print(str);
      print_->write('\n');

      GetString(str, 1, 0, 16);
      print_->print(str);
      print_->write('\n');

    }
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

  private:

    // Row + Column written.
    uint8_t bytes_[2][17];

    // Where the next character should be written.
    uint8_t row_pos_;
    uint8_t col_pos_;

    Print *print_;
};

#endif

class DallasSensor : public Sensor {
  public:
    DallasSensor(Print *print) : print_(print) {
      sensor_.begin();

      // We only have one dallas temperature sensor on the bus. Store this to avoid needing to
      // scan the bus on each read.
      sensor_.getAddress(temperature_address_, static_cast<uint8_t>(0));

      // When requesting a conversion, don't wait for the data, we'll collect the metrics
      // later.
      sensor_.setWaitForConversion(false);
    };
    // TODO: Finish filling this in.
    void StartRequestAsync() override {
      sensor_.requestTemperatures();
    };
    float GetTemperature() override {
      return sensor_.getTempF(temperature_address_);
    };
    void EnableGasHeater(const bool ) override { };
    uint32_t GetGasResistance() override {
      return 0;
    };

  private:
    DeviceAddress temperature_address_;
    OneWire temperature_one_wire_ = OneWire(ONE_WIRE_BUS);

    DallasTemperature sensor_ = DallasTemperature(&temperature_one_wire_);
    Print *print_;


};


constexpr int rs = 8, en = 9, d4 = 4, d5 = 5, d6 = 6, d7 = 7;

class Lcd : public Display {
  public:
    void SetUp() {

      // set up the LCD's number of columns and rows:
      lcd_.begin(16, 2);

      // create the new characters
      lcd_.createChar(0, custom_degree);
      lcd_.createChar(1, custom_backslash);
    }
    void write(uint8_t ch) override {
      if (ch == '\247') { // '°'
        lcd_.write(byte(0));
        return;
      }
      if (ch == '\134') { // '\'
        lcd_.write(byte(1));
        return;
      }

      lcd_.write(ch);
    }
    void SetCursor(const int row, const int column) override {
      lcd_.setCursor(row, column);
      // This is reverse of LiquidCrystal since it's more intuitive to talk about row first.
    };

  private:
    // Set up the LCD.
    LiquidCrystal lcd_ = LiquidCrystal(rs, en, d4, d5, d6, d7);

    // Create a custom degree '°' symbol for the 1602 Serial LCD display.
    //
    // This isn't const/constexpr because lcd.createChar wants an uint8_t* type.
    byte custom_degree[8] = {0b01000, //
                             0b10100, //
                             0b01000, //
                             0b00000, //
                             0b00000, //
                             0b00000, //
                             0b00000, //
                             0b00000
                            };

    byte custom_backslash[8] = {0b00000, //
                                0b10000, //
                                0b01000, //
                                0b00100, //
                                0b00010, //
                                0b00001, //
                                0b00000, //
                                0b00000
                               };

};

class BmeSensor : public Sensor {
  public:
    BmeSensor(Print *print) : print_(print) {};

    void SetUp() override {
      if (!bme_.begin()) {
        print_->print("Could not find a valid BME680 sensor, check wiring!\r\n");
        while (1)
          ;
      }

      // Set up oversampling and filter initialization
      bme_.setTemperatureOversampling(BME680_OS_8X);
      bme_.setHumidityOversampling(BME680_OS_2X);
      bme_.setPressureOversampling(BME680_OS_4X);
      bme_.setIIRFilterSize(BME680_FILTER_SIZE_3);
      bme_.setGasHeater(320, 150);  // 320°C for 150 ms

    };

    bool EndReading() override {
      return bme_.endReading();
    }
    float GetHumidity() override {
      return bme_.humidity;
    }
    float GetPressure() override {
      return bme_.pressure;
    }

    void EnableGasHeater(bool enable) override {
      if (enable) {
        bme_.setGasHeater(320, 150);  // 320°C for 150 ms
        return;
      }

      bme_.setGasHeater(320, 0 /*Off*/);
    }

    uint32_t GetGasResistance() {
      return bme_.gas_resistance;
    }

    // TODO: Finish filling this in.
    void StartRequestAsync() override {
      bme_.beginReading();
    };
    float GetTemperature() override {
      return bme_.temperature * 1.8 + 32.0;
    };

  private:
    Adafruit_BME680 bme_;
    Print *print_;

};

class RealClock : public Clock {
  public:
    RealClock() : rtc_(0x68, URTCLIB_MODEL_DS1307) {};

    uint32_t Millis() const override {
      return millis();
    };

    static Date SanitizeDate(Date date) {
      if (date.hour >= 24) {
        date.hour = 0;
      }
      if (date.minute >= 60) {
        date.minute = 0;
      }
      if (date.day_of_week >= 7) {
        date.day_of_week = 0;
      }
      return date;
    }
    
    Date Now() override {
      rtc_.refresh();
      Date date;
      date.hour = rtc_.hour();
      date.minute = rtc_.minute();
      date.day_of_week = rtc_.dayOfWeek();

      return SanitizeDate(date);
    }

    void Set(const Date& date) override {
      const Date new_date = SanitizeDate(date);
      rtc_.set(/*seconds=*/0, new_date.minute, new_date.hour, new_date.day_of_week, /*dayOfMonth=*/1,
                           /*month=*/1, /*year*/ 20);
    }

  private:
    uRTCLib rtc_;

};

class SsdRelays: public Relays {
  public:
    void SetUp() {
      digitalWrite(SSR1_PIN, OFF);  // Relay (1)
      digitalWrite(SSR2_PIN, OFF);  // Relay (2)
      digitalWrite(SSR4_PIN, OFF);  // Relay (4)
      digitalWrite(SSR3_PIN, OFF);  // Relay (3)
      pinMode(SSR1_PIN, OUTPUT);
      pinMode(SSR2_PIN, OUTPUT);
      pinMode(SSR4_PIN, OUTPUT);
      pinMode(SSR3_PIN, OUTPUT);

    }
    void Set(const RelayType relay, const RelayState relay_state) {
      const int state = ConvertToOutputState(relay_state);
      switch (relay) {
        case RelayType::kHeat:
          if (state == digitalRead(SSR1_PIN)) {
            return;
          }
          digitalWrite(SSR1_PIN, state);
          break;
        case RelayType::kCool:
          if (state == digitalRead(SSR2_PIN)) {
            return;
          }
          digitalWrite(SSR2_PIN, state);
          break;
        case RelayType::kFan:
          if (state == digitalRead(SSR3_PIN)) {
            return;
          }
          digitalWrite(SSR3_PIN, state);
          break;
        case RelayType::kHeatHigh:
          if (state == digitalRead(SSR4_PIN)) {
            return;
          }
          digitalWrite(SSR4_PIN, state);
          break;
        default:
          // Do nothing.
          break;
      }
    };
  private:
    int ConvertToOutputState(const RelayState state) {
      if (state == RelayState::kOn) {
        return LOW;
      }
      return HIGH;
    };

    // Customize these pins for your own board.
    static constexpr int SSR1_PIN = 34;
    static constexpr int SSR2_PIN = 35;
    static constexpr int SSR4_PIN = 36;
    static constexpr int SSR3_PIN = 37;

    static constexpr int ON = LOW;
    static constexpr int OFF = HIGH;

};

// Abstract interface for Serial/Debug output.
class Output : public Print {
  public:
    virtual void SetUp() {
      ::Serial.begin(38400);
    }
    // Note: Arduinos Print uses a return size_t, but we need
    // this to be consistent.
    void write(uint8_t ch) override {
      ::Serial.write(ch);
    };
};

}
#endif  // AVR_IMPLS_H_
