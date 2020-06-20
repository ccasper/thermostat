#include <Arduino.h>

#include <LiquidCrystal.h>
#include <SPI.h>
#include <Wire.h>
#include "Adafruit_BME680.h"

#include "comparison.h"
#include "rtc_date.h"
#include "buttons.h"
#include "relays.h"
#include "menus.h"
#include "settings.h"
#include "dallas.h"
#include "millis_since.h"
#include "events_processing.h"
#include "fan_controller.h"
#include "calculate_iaq_score.h"

// Interrupt Logic.
//
// This handles LCD backlight dimming.
volatile uint16_t backlight_count = 10000;
namespace {
void CustomInterruptSetup() {
  // Ensure the backlight is set low by default to prevent a VCC to GND short through the transistor.
  digitalWrite(10, LOW);

  // Timer0 is already used for millis() - we'll just interrupt somewhere
  // in the middle and call the "Compare A" function below
  OCR0A = 0xAF;
  TIMSK0 |= _BV(OCIE0A);
}

// Enabling this flag will allow the backlight to go fully off after ~10 seconds of inactivity.
constexpr bool kBacklightShutoffEnabled = false;

// Utilize the millis() TIMER0 interrupt to dim the display backlight. We could use a different timer if we want it to operate dimmer.
SIGNAL(TIMER0_COMPA_vect) {
  static uint8_t isr_counter = 0;

  // Dim the backlight to 33% brightness.
  //
  // We change Input/Output rather than the output voltage level because many of the 1602 Serial Blue Backlight LCD Display Keypad (LCD Keypad Shield) that plug into the Arduino have a transistor wiring bug which will short the pin otherwise.
  isr_counter = (isr_counter + 1) % 3;

  // Turn off backlight for two counts (2 ms)
  if (isr_counter == 0) {
    pinMode(10, OUTPUT); // turn off backlight.
  }
  if (backlight_count > 0) {
    // Turn on backlight for one count. (1 ms)
    if (isr_counter == 2) {
      pinMode(10, INPUT);
      if (kBacklightShutoffEnabled) {
        --backlight_count;
      }
    }
  }
}

} // namespace

// Create a custom degree '°' symbol for the 1602 Serial LCD display.
//
// This isn't const/constexpr because lcd.createChar wants an uint8_t* type.
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

// Setup the LCD.
constexpr int rs = 8, en = 9, d4 = 4, d5 = 5, d6 = 6, d7 = 7;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// Ensure 'int' is 16 bits (0 - 65535). If this assumption changes, it may cause code bugs.
static_assert(sizeof(int) == 2);

Settings settings = GetEepromOrDefaultSettings();

FanController fan = FanController();

#define BME_SCK 13
#define BME_MISO 12
#define BME_MOSI 11
#define BME_CS 10
Adafruit_BME680 bme;


void setup() {
  // Add a extra Timer0 interrupt for IRQ stuff.
  CustomInterruptSetup();
  relays::SetupRelays();
  dallas::SetupTemperatureSensor();

  Serial.begin(9600);
  Wire.begin();

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

// Average the temperature using a window of values.
constexpr uint8_t TEMP_SIZE = 8;
int temp_window[TEMP_SIZE] = {0};
// Maintain a sum of the window values to make calculating the average fast.
int temp_sum = 0;

// Index to fill in next. Starts at zero so the mean can be calculated accurately during initial filling.
uint8_t temp_index = 0;

// Flag gets updated to true once temp_index reaches TEMP_SIZE. This ensures the mean calculation is accurate.
bool temp_filled = false;

// Called to check and update the HVAC, humidifier and fan actions. This needs to be performed once every 1-5 seconds.
uint8_t hvac_run_counter = 0;
bool hvac_gas_measurement_on = false;

// Menu uses the user input buttons and user output lcd line 2 to manipulate the settings fields. The changed flag indicates when changes should take quicker effect.
Menus menu(&settings, &WaitForButtonPress, &rtc_date, &lcd);

// This routine is always run periodically, even when blocking in menus. This ensures the HVAC controls are continuously maintained.
//
// For example, when we call into a menu, the menu blocks, and since the menu always blocks using WaitForButtonPress, we regularily update this function.
uint32_t last_update = -10000;
void MaintainHvac() {
  // Run only once every 2.5 seconds.
  if (!settings.changed && millisSince(last_update) <= 2500) {
    return;
  }
  settings.hvac_debug = 'b';

  ++hvac_run_counter;

  // Reset the settings.
  settings.changed = 0;

  last_update = millis();

  if (settings.override_temp_x10 != 0 &&
      millisSince(settings.override_temp_started_ms) > HoursToMillis(2)) {
    settings.override_temp_x10 = 0;
  }

  // Stop or wait for the reading to complete.
  settings.hvac_debug = 'c';

  // Scale by 10 and clip the temperature to 99.9*.
  const int temperature = cmin(dallas::sensor.getTempF(dallas::temperature_address) * 10, 999);

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
    const float air_quality_score = CalculateIAQScore(bme);
    Serial.print("IAQ: "); Serial.print(air_quality_score); Serial.print ("% ");
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

  // Kick off the next asynchronous readings.
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
    relays::SetFanRelay(relays::ON);
  } else {
    lcd.print("_");
    relays::SetFanRelay(relays::OFF);
  }

  settings.hvac_debug = 'u';
  const int setpoint_temp_x10 = GetSetpointTemp(settings, rtc_date.Now(), Mode::HEAT);
  Serial.println("Temp: " + String(settings.current_temp_x10) + " Mean: " + String(temp_mean) + " heat enabled:" + String(settings.persisted.heat_enabled) + " Tol: " + String(settings.persisted.tolerance_x10));
  settings.hvac_debug = 'v';

  bool lockout_heat = false;
  bool lockout_cool = false;

  // If heating is on, heat to the setpoint. If off, drift to the setpoint minus the tolerance.
  // If cooling is on, cool to the setpoint. If off, drift to the setpoint plus the tolerance.
  // Should we enable/disable heating mode?
  if (settings.heat_running) {
    Serial.println("Heat running");

    if (temp_mean >= setpoint_temp_x10 + settings.persisted.tolerance_x10) {
      Serial.println("Reached setpoint");
      // Stop heating, we've reached our setpoint.
      settings.heat_running = false;
    }
  } else {
    Serial.println("Heat not running");
    if (settings.persisted.heat_enabled) {
      Serial.println("Settings heat true");
    }
    if (temp_mean > setpoint_temp_x10) {
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

  const int cool_temp_x10 = GetSetpointTemp(settings, rtc_date.Now(), Mode::COOL);
  Serial.println("Cool check: " + String(cool_temp_x10));
  // Should we enable/disable cooling mode?
  if (settings.cool_running) {
    if (temp_mean <= cool_temp_x10 - settings.persisted.tolerance_x10) {
      // Stop cooling, we've reached our setpoint.
      settings.cool_running = false;
    }
  } else {
    // Assuming a set point of 70, we should start cooling at 72 with a tolerance set of 2.
    if (temp_mean > cool_temp_x10) {
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
    relays::SetHumidifierRelay(relays::ON);
  } else {
    relays::SetHumidifierRelay(relays::OFF);
  }
  settings.hvac_debug = 'z';

  if (settings.heat_running) {
    relays::SetHeatRelay(relays::ON);
    relays::SetCoolRelay(relays::OFF);

    lcd.print("H");
  } else if (settings.cool_running) {
    relays::SetHeatRelay(relays::OFF);
    relays::SetCoolRelay(relays::ON);

    lcd.print('C');
  } else {
    relays::SetHeatRelay(relays::OFF);
    relays::SetCoolRelay(relays::OFF);

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

// Waits for a button press while maintaining the HVAC system.
//
// Menus use this to ensure sleeping always updates the HVAC routine.
Button WaitForButtonPress(const uint32_t timeout) {
  const uint32_t start = millis();
  Button button = Button::NONE;

  while (button == Button::NONE) {
    MaintainHvac();

    // Poll for single button presses.
    button = Buttons::GetSinglePress(
               Buttons::StabilizedButtonPressed(
                 Buttons::GetButton(analogRead(0))));


    if (millisSince(start) >= timeout) {
      return Button::TIMEOUT;
    }
  }

  // If a button was pressed, turn on the backlight for some length of time.
  backlight_count = 10000;

  return button;
}

void loop() {
  // Show basic time information in the bottom row.
  //
  // This is blocking.
  const Button button = menu.InformationalState();

  // Override the temperature.
  if (button == Button::UP || button == Button::DOWN) {
    // This is blocking.
    menu.EditOverrideTemp();
  }

  if (button == Button::LEFT) {
    // Go through the statuses.
    //
    // This is blocking.
    menu.ShowStatuses();
  }
  if (button == Button::RIGHT) {
    // Go through the edit options.
    //
    // This is blocking.
    menu.EditSettings();
  }
  // Once we're done editing settings, return back to showing the informational state.
}
