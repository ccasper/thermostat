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
    void Setup() {

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
    // Setup the LCD.
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

    void Setup() {
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

    uint32_t Millis() override {
      return millis();
    };

    Date Now() override {
      rtc_.refresh();
      Date date;
      date.hour = rtc_.hour();
      date.minute = rtc_.minute();
      date.day_of_week = rtc_.dayOfWeek();
      return date;
    }

    void Set(const Date& date) override {
      rtc_.set(/*seconds=*/0, date.minute, date.hour, date.day_of_week, /*dayOfMonth=*/1,
                           /*month=*/1, /*year*/ 20);
    }

  private:
    uRTCLib rtc_;

};

class SsdRelays: public Relays {
  public:
    void Setup() {
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
        case RelayType::kHumidifier:
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
    virtual void Setup() {
      ::Serial.begin(9600);
    }
    // Note: Arduinos Print uses a return size_t, but we need
    // this to be consistent.
    void write(uint8_t ch) override {
      ::Serial.write(ch);
    };
};

}
#endif  // AVR_IMPLS_H_
