#include <Arduino.h>

#include <LiquidCrystal.h>
#include <SPI.h>
#include <Wire.h>
#include "Adafruit_BME680.h"

#include "comparison.h"
#include "rtc_date.h"
#include "buttons.h"
#include "menus.h"
#include "settings.h"
#include "dallas.h"
#include "millis_since.h"
#include "events_processing.h"
#include "fan_controller.h"

// Revision History:
// 2020-05-27 - Added separate cooling temp, Load/Store Settings debug, OnPercent display. (Did not program it yet)
      
// Dallas 1-wire temp sensor
namespace {
}

// SCR relays
namespace {
constexpr int ON = LOW;
constexpr int OFF = HIGH;

constexpr int SSR1_PIN = 34;
constexpr int SSR2_PIN = 35;
constexpr int SSR4_PIN = 36;
constexpr int SSR3_PIN = 37;

void SetupRelays() {
  digitalWrite(34, HIGH); // Relay (1)
  digitalWrite(35, HIGH);// Relay (2)
  digitalWrite(36, HIGH); // Relay (4)
  digitalWrite(37, HIGH); // Relay (3)
  pinMode(34, OUTPUT); // turn off backlight
  pinMode(35, OUTPUT); // turn off backlight
  pinMode(36, OUTPUT); // turn off backlight
  pinMode(37, OUTPUT); // turn off backlight
}

void SetHeatRelay(const int state) {
  if (state == digitalRead(SSR1_PIN)) {
    return;
  }
  digitalWrite(SSR1_PIN, state);
}
void SetCoolRelay(const int state) {
  if (state == digitalRead(SSR2_PIN)) {
    return;
  }
  digitalWrite(SSR2_PIN, state);
}
void SetFanRelay(const int state) {
  if (state == digitalRead(SSR3_PIN)) {
    return;
  }
  digitalWrite(SSR3_PIN, state);
}
void SetHumidifierRelay(const int state) {
  if (state == digitalRead(SSR4_PIN)) {
    return;
  }
  digitalWrite(SSR4_PIN, state);
}

}

// Interrupt Logic.
namespace {
void CustomInterruptSetup() {
  // Ensure the backlight is set low by default to prevent a VCC to GND short through the transistor.
  digitalWrite(10, LOW);

  // Timer0 is already used for millis() - we'll just interrupt somewhere
  // in the middle and call the "Compare A" function below
  OCR0A = 0xAF;
  TIMSK0 |= _BV(OCIE0A);
}

SIGNAL(TIMER0_COMPA_vect) {
  static uint8_t isr_counter = 0;

  // Dim the backlight to 33% brightness.
  isr_counter = (isr_counter + 1) % 3;
  // Backlight off 0-2 (3 ms)
  if (isr_counter == 0) {
    pinMode(10, OUTPUT); // turn off backlight
  }
  // Backlight on 3 (1 ms)
  if (isr_counter == 2) {
    pinMode(10, INPUT); // turn on backlight
  }

}

} // namespace


byte degree[8] = {
  0b01000,
  0b10100,
  0b01000,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000
};

RtcDate rtc_date;

const int rs = 8, en = 9, d4 = 4, d5 = 5, d6 = 6, d7 = 7;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// Ensure 'int' is 16 bits (0 - 65535). If this assumption changes, it may cause code bugs.
static_assert(sizeof(int) == 2);

Settings settings = GetEepromOrDefaultSettings();

FanController fan = FanController();

#define BME_SCK 13
#define BME_MISO 12
#define BME_MOSI 11
#define BME_CS 10

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME680 bme; // I2C

String CalculateIAQ(float score) {
  String IAQ_text = "Air quality is ";
  score = (100 - score) * 5;
  if      (score >= 301)                  IAQ_text += "Hazardous";
  else if (score >= 201 && score <= 300 ) IAQ_text += "Very Unhealthy";
  else if (score >= 176 && score <= 200 ) IAQ_text += "Unhealthy";
  else if (score >= 151 && score <= 175 ) IAQ_text += "Unhealthy for Sensitive Groups";
  else if (score >=  51 && score <= 150 ) IAQ_text += "Moderate";
  else if (score >=  00 && score <=  50 ) IAQ_text += "Good";
  return IAQ_text;
}

// See https://github.com/G6EJD/BME680-Example
float GetIAQScore(const Adafruit_BME680& bme) {
  const float hum_weight = 0.0; // so hum effect is 25% of the total air quality score
  const float gas_weight = 1.00; // so gas effect is 75% of the total air quality score

  float hum_score = 0 , gas_score = 0;
  float gas_resistance = bme.gas_resistance;
  const float hum_reference = 40;

  const float current_humidity = bme.humidity;
  if (hum_weight > 0.01) {
    //Calculate humidity contribution to IAQ index
    if (current_humidity >= 38 && current_humidity <= 42)
      hum_score = hum_weight * 100; // Humidity +/-5% around optimum
    else
    { //sub-optimal
      if (current_humidity < 38)
        hum_score = hum_weight / hum_reference * current_humidity * 100;
      else
      {
        hum_score = ((-hum_weight / (100 - hum_reference) * current_humidity) + 0.416666) * 100;
      }
    }
  }

  //Calculate gas contribution to IAQ index
  float gas_hum_corrected = log(gas_resistance) + 0.04 * current_humidity;
  {
    float gas_lower_limit = 5000;   // Bad air quality limit
    float gas_upper_limit = 50000;  // Good air quality limit
    if (gas_resistance > gas_upper_limit) gas_resistance = gas_upper_limit;
    if (gas_resistance < gas_lower_limit) gas_resistance = gas_lower_limit;
    gas_score = (gas_weight / (gas_upper_limit - gas_lower_limit) * gas_resistance - (gas_lower_limit * (gas_weight / (gas_upper_limit - gas_lower_limit)))) * 100;
  }

  //Combine results for the final IAQ index value (0-100% where 100% is good quality air)
  const float air_quality_score = hum_score + gas_score;

  //Serial.println("Air Quality = " + String(air_quality_score, 1) + "%");
  //Serial.println("Humidity element was : " + String(hum_score / 100) + " of 0.25");
  //Serial.println("     Gas element was : " + String(gas_score / 100) + " of 0.75");
  //Serial.println();
  //  if ((getgasreference_count++) % 10 == 0) GetGasReference();
  Serial.print("Air quality corr: ");
  Serial.println(gas_hum_corrected);
  Serial.println(CalculateIAQ(air_quality_score));
  Serial.println("------------------------------------------------");
  return air_quality_score;

}

void setup() {
  // Add a extra Timer0 interrupt for IRQ stuff.
  CustomInterruptSetup();
  SetupRelays();
  dallas::SetupTemperatureSensor();

  Serial.begin(9600);
  Wire.begin();

  //bmp085Calibration();

  if (!bme.begin()) {
    Serial.println("Could not find a valid BME680 sensor, check wiring!");
    while (1);
  }

  // Set up oversampling and filter initialization
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms

  sei();

  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);

  // create a new character
  lcd.createChar(0, degree);
}

// Current Temp 68.1°F with a target setpoint of 70.0°F, Fan is in On Mode, Heat is Running, time is 14:35 on Monday.
//
// Every 2 seconds, cycle to the next information message.
// |################|
// |68.1°/70.0°   FH| // Clips to 99.9° indoor.
// |Time: 14:34 M   |
// |################|
// |1234567890123456|

// |################|
// |68.1°/70.0°   FH|
// |Out:100.0° 23%RH| // Clips to 99% RH.
// |################|
// |1234567890123456|

// |################|
// |68.1°/70.0° !!FH| // !! = Means there is an action, such as replace filter to perform.
// |RH: I=26% O=23% |
// |################|
// |1234567890123456|

// |################|
// |68.1°/70.0°  !FH|
// |OffΔF +00.01°/hr|      // °/hr = [1 / double(s/°)](°/s) * [60*60] s/hr
// |################|
// |1234567890123456|

// |################|
// |68.1°/70.0°   FH|
// |On ΔF -00.01°/hr|
// |################|
// |1234567890123456|

// |################|
// |68.1°/70.0°   FH|
// |Run 1d: 10%     |
// |################|
// |1234567890123456|

// Every 2 months active use (fan running time), replace filter in the cycle, but hold for 5 seconds.
// |################|
// |68.1°/70.0°   FH|
// |Filter on  XXXXh|
// |################|
// |1234567890123456|

// 15 count window which is approx 15 seconds.
constexpr uint8_t TEMP_SIZE = 8;
int temp_window[TEMP_SIZE] = {0};
float temp_sum = 0;
// Index to fill in next. Starts at zero so the mean can be calculated accurately during initial filling.
uint8_t temp_index = 0;
// Flag gets updated to true once temp_index reaches TEMP_SIZE. This ensures the mean calculation is accurate.
bool temp_filled = false;

// Called to check and update the HVAC, humidifier and fan actions. This needs to be performed once every 1-5 seconds.
uint8_t hvac_run_counter = 0;
bool hvac_gas_measurement_on = false;

// Menu uses the user input buttons and user output lcd line 2 to manipulate the settings fields. The changed flag indicates when changes should take quicker effect.
Menus menu(&settings, &WaitForButtonPress, &rtc_date, &lcd);

uint32_t last_update = -10000;
void MaintainHvac() {
  // Run only once every 2.5 seconds.
  if (!settings.changed && millisSince(last_update) <= 2500) {
    return;
  }
  settings.hvac_debug = 'b';

  ++hvac_run_counter;

  settings.changed = 0;

  last_update = millis();

  if (settings.override_temp_x10 != 0 &&
      millisSince(settings.override_temp_started_ms) > HoursToMillis(2)) {
    settings.override_temp_x10 = 0;
  }

  // Stop or wait for the reading to complete.
  settings.hvac_debug = 'c';

  // Scale by 10 and clip the temperature to 99.9*.
  const int temperature = cmin( dallas::sensor.getTempF(dallas::temperature_address) * 10, 999);
  // Store the new value in the settings.
  settings.current_temp_x10 = temperature;

  settings.hvac_debug = 'd';
  if (! bme.endReading()) {
    menu.ShowStatus(Menus::Error::BME_SENSOR_FAIL);
    Serial.println("Failed to perform reading :(");
    settings.hvac_debug = 'e';
    return;
  }
  settings.hvac_debug = 'f';

  Serial.print("MaintainHvac Temperature = ");
  const int bme_temperature = cmin((bme.temperature * 1.8 + 32.0) * 10.0, 999);
  settings.hvac_debug = 'g';
  settings.current_bme_temp_x10 = bme_temperature;
  Serial.print(bme_temperature); Serial.println(" *F");
  Serial.print(temperature); Serial.println(" *F");
  Serial.print("Pressure = "); Serial.print(bme.pressure / 100.0); Serial.println(" hPa");
  Serial.print("Humidity = "); Serial.print(bme.humidity); Serial.println(" %");
  settings.hvac_debug = 'h';
  if (hvac_gas_measurement_on) {
    settings.hvac_debug = 'i';
    Serial.print("Gas = "); Serial.print(bme.gas_resistance / 1000.0); Serial.println(" KOhms");
    const float air_quality_score = GetIAQScore(bme);
    Serial.print("IAQ: "); Serial.print(air_quality_score); Serial.print ("% "); Serial.println(CalculateIAQ(air_quality_score));
  }
  settings.hvac_debug = 'j';

  // Turn off the gas heater/sensor if it was previously enabled.
  if (hvac_gas_measurement_on) {
    bme.setGasHeater(320, 0 /*Off*/);
    hvac_gas_measurement_on = false;
  }

  settings.hvac_debug = 'k';
  // Periodically (every 20s) check the gas sensor.hvac_run_counter
  if (hvac_run_counter % 8 == 7) {
    bme.setGasHeater(320, 150); // 320*C for 150 ms
    hvac_gas_measurement_on = true;
  }
  settings.hvac_debug = 'l';


  // Kick off the next asynchronous reading.
  bme.beginReading();

  settings.hvac_debug = 'm';
  dallas::sensor.requestTemperatures();
  settings.hvac_debug = 'n';

  // Only subtract past values that were previously added to the window.
  if (temp_filled) {
    temp_sum -= temp_window[temp_index];
  }
  temp_window[temp_index] = settings.current_temp_x10;
  temp_sum += temp_window[temp_index];
  if (temp_index == TEMP_SIZE - 1 ) {
    temp_filled = true;
  }
  temp_index = (temp_index + 1) % TEMP_SIZE;
  settings.hvac_debug = 'o';

  const uint8_t temp_counts = temp_filled ? TEMP_SIZE : temp_index;
  const int temp_mean = temp_sum / temp_counts;
  settings.current_mean_temp_x10 = temp_mean;

  // set the cursor to column 0, line 1
  // (note: line 1 is the second row, since counting begins with 0):
  settings.hvac_debug = 'p';
  lcd.setCursor(0, 0);

  lcd.print(static_cast<int>(temp_mean) / 10, DEC);
  lcd.print(".");
  lcd.print(static_cast<int>(temp_mean) % 10, DEC);
  lcd.write(byte(0));
  lcd.write(" ");
  settings.hvac_debug = 'q';

  settings.current_humidity = bme.humidity;
  lcd.print(static_cast<int>(bme.humidity)); // Clips to 99.9° indoor.
  lcd.print(".");
  lcd.print(static_cast<int>(bme.humidity * 10) % 10); // Clips to 99.9° indoor.
  lcd.write("%");
  lcd.print(" ");
  settings.hvac_debug = 'r';
  if (IsOverrideTempActive(settings)) {
    lcd.print("o");
  } else {
    lcd.write(' ');
  }

  settings.hvac_debug = 's';
  fan.UpdateSetting(&settings);

  settings.hvac_debug = 't';
  if (settings.fan_running) {
    lcd.print("F");
    SetFanRelay(ON);
  } else {
    lcd.print("_");
    SetFanRelay(OFF);
  }

  settings.hvac_debug = 'u';
  const int setpoint_temp_x10 = GetSetpointTemp(settings, rtc_date.Now());
  Serial.println("Temp: " + String(settings.current_temp_x10) + " Mean: " + String(temp_mean) + " heat enabled:" + String(settings.persisted.heat_enabled) + " Tol: " + String(settings.persisted.tolerance_x10));
  settings.hvac_debug = 'v';

  bool lockout_heat = false;
  bool lockout_cool = false;

  // If heating is on, heat to the setpoint. If off, drift to the setpoint minus the tolerance.
  // If cooling is on, cool to the setpoint. If off, drift to the setpoint plus the tolerance.
  // Should we enable/disable heating mode?
  if (settings.heat_running) {
    Serial.println("Heat running");

    if (temp_mean >= setpoint_temp_x10) {
      Serial.println("Reached setpoint");
      // Stop heating, we've reached our setpoint.
      settings.heat_running = false;
    }
  } else {
    Serial.println("Heat not running");
    if (settings.persisted.heat_enabled) {
      Serial.println("Settings heat true");
    }
    if (temp_mean > setpoint_temp_x10 - settings.persisted.tolerance_x10) {
      Serial.println("Temp not met " + String(temp_mean) + "<=" + String(setpoint_temp_x10) + "-" +  String(settings.persisted.tolerance_x10));
    }
    // Assuming Set point is 70 and current room temp is 68, we should enable heating if tolerance is 2.
    if (settings.persisted.heat_enabled &&
        temp_mean <= setpoint_temp_x10 - settings.persisted.tolerance_x10) {
      // Start heating, we've reached our lower tolerance limit.
      if (!IsInLockoutMode(Mode::HEAT, settings.events, 10)) {
        settings.heat_running = true;
      } else {
        lockout_heat = true;
      }
    }
  }
  settings.hvac_debug = 'w';

  // Prevent/Stop heating, we're disabled.
  if (!settings.persisted.heat_enabled) {
    settings.heat_running = false;
    lockout_heat = false;

  }

  const int cool_temp_x10 = GetCoolTemp(settings);
  Serial.println("Cool check: " + String(cool_temp_x10));
  // Should we enable/disable cooling mode?
  if (settings.cool_running) {
    if (temp_mean < settings.persisted.cool_temp_x10) {
      // Stop cooling, we've reached our setpoint.
      settings.cool_running = false;
    }
  } else {
    // Assuming a set point of 70, we should start cooling at 72 with a tolerance set of 2.
    if (temp_mean >= settings.persisted.cool_temp_x10 + settings.persisted.tolerance_x10) {
      if (!IsInLockoutMode(Mode::COOL, settings.events, 10)) {
        // Start heating, we've reached our upper tolerance limit.
        settings.cool_running = true;
      } else {
        lockout_cool = true;
      }
    }
  }

  settings.hvac_debug = 'x';
  // Stop/prevent cooling, we're disabled.
  if (!settings.persisted.cool_enabled) {
    settings.cool_running = false;
    lockout_cool = false;
  }

  // This should never happen, but disable both if both are enabled.
  if (settings.heat_running && settings.cool_running) {
    Serial.println("Error: Heat and Cooling were set to run");
    menu.ShowStatus(Menus::Error::HEAT_AND_COOL);
    settings.heat_running = false;
    settings.cool_running = false;
  }
  settings.hvac_debug = 'y';

  if (settings.heat_running && settings.current_humidity < settings.persisted.humidity) {
    // Allow humidifier only when heating. During hot A/C days, we want as much humidity removed as possible.
    SetHumidifierRelay(ON);
  } else {
    SetHumidifierRelay(OFF);
  }
  settings.hvac_debug = 'z';

  if (settings.heat_running) {
    SetHeatRelay(ON);
    SetCoolRelay(OFF);

    lcd.print("H");
  } else if (settings.cool_running) {
    SetHeatRelay(OFF);
    SetCoolRelay(ON);

    lcd.print('C');
  } else {
    SetHeatRelay(OFF);
    SetCoolRelay(OFF);

    if (lockout_heat) {
      lcd.print("h");
    } else if (lockout_cool) {
      lcd.print("c");
    } else {
      lcd.print("_");
    }
  }
  settings.hvac_debug = 'A';

  AddOrUpdateEvent(millis(), &settings);
  settings.hvac_debug = 'B';
  menu.ShowStatus(Menus::Error::STATUS_OK);
  settings.hvac_debug = '#';
  
  return;
}

// Waits for button press while maintaining the HVAC system, but not more than 10 seconds.
Button WaitForButtonPress(const uint32_t timeout) {
  const uint32_t start = millis();
  Button button = Button::NONE;

  while (button == Button::NONE) {
    MaintainHvac();
    button = GetButton();

    if (millisSince(start) >= timeout) {
      return Button::TIMEOUT;
    }
  }

  return button;
}

Button GetButton() {
  // Poll for single button presses.
  return Buttons::GetSinglePress(
           Buttons::StabilizedButtonPressed(
             Buttons::GetButton(analogRead(0))));

}


void loop() {
  if (1 == 1) {
  }

  MaintainHvac();

  // Nonblocking.
  menu.UpdateInformationalState();

  // Check the active buttons.
  const Button button = GetButton();

  // Informational exits if edit settings is selected.
  if (button == Button::SELECT) {
  }
  // Override the temperature.
  if (button == Button::UP) {
    Serial.println("UP");
    menu.EditOverrideTemp();
  }
  // Override the temperature.
  if (button == Button::DOWN) {
    Serial.println("DOWN");
    // Decrement the current temporary setpoint.
    menu.EditOverrideTemp();
  }
  if (button == Button::LEFT) {
    // Inform UpdateInformationalState to go back one.
    menu.ShowStatuses();
  }
  if (button == Button::RIGHT) {
    // Block in the menu while periodically updating MaintainHvac.
    menu.EditSettings();
  }
  // Once we're done editing settings, we return to the cycling of info.
}
