// 2021-Feb
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

// Set this if we're using an Arduino Mega with the LCD Keypad Shield only.
// ========================================================================
// ========================================================================
#define DEV_BOARD 1
// ========================================================================
// ========================================================================

// Interfaces which include hardware abstraction layer.
#include "interfaces.h"

// AVR specific implementations.
#include "avr_impls.h"

// Workaround for AVR min/max logic bug.
#include "comparison.h"

// Calculate indoor air quality.
#include "calculate_iaq.h"

#include "buttons.h"
#include "events.h"
#include "menus.h"
#include "settings.h"
#include "thermostat_tasks.h"
#include "settings_storer.h"


// Interrupt Logic.
//
// This handles LCD backlight dimming.
volatile uint16_t g_backlight_count = 10000;

namespace {
void CustomInterruptSetup() {
  // Ensure the backlight is set low by default to prevent a VCC to GND short through the
  // transistor.
  digitalWrite(10, LOW);

  // Timer0 is already used for millis() - we'll just interrupt somewhere
  // in the middle and call the "Compare A" function below
  OCR0A = 0xAF;
  TIMSK0 |= _BV(OCIE0A);
}

// Enabling this flag will allow the backlight to go fully off after ~10 seconds of
// inactivity.
constexpr bool kBacklightShutoffEnabled = false;

// Utilize the millis() TIMER0 interrupt to dim the display backlight. We could use a
// different timer if we want it to operate dimmer.
SIGNAL(TIMER0_COMPA_vect) {
  static uint8_t isr_counter = 0;

  // Dim the backlight to 33% brightness.
  //
  // We change Input/Output rather than the output voltage level because many of the 1602
  // Serial Blue Backlight LCD Display Keypad (LCD Keypad Shield) that plug into the
  // Arduino have a transistor wiring bug which will short the pin otherwise.
  isr_counter = (isr_counter + 1) % 3;

  // Turn off backlight for two counts (2 ms)
  if (isr_counter == 0) {
    pinMode(10, OUTPUT);  // turn off backlight.
  }
  if (g_backlight_count > 0) {
    // Turn on backlight for one count. (1 ms)
    if (isr_counter == 2) {
      pinMode(10, INPUT);
      if (kBacklightShutoffEnabled) {
        --g_backlight_count;
      }
    }
  }
}

}  // namespace

// Ensure 'int' is 16 bits. If this assumption changes, it may cause code bugs.
static_assert(sizeof(int) == 2);
static_assert(sizeof(long) == 4);
static_assert(sizeof(float) == 4);
static_assert(sizeof(double) == 4);

using namespace thermostat;

Output g_print;

EepromSettingsStorer g_storer;

// Restore the settings to use for the thermostat.
Settings g_settings = GetEepromOrDefaultSettings(&g_storer);

// Create the relays controller.
SsdRelays g_relays = SsdRelays();


// Create the sensors
#ifdef DEV_BOARD
  CannedSensor g_secondary_temp_sensor = CannedSensor(&g_print);
  Dht22Sensor g_primary_sensor = Dht22Sensor(&g_print);
//  BmeSensor g_primary_sensor = BmeSensor(&g_print);
#else
  // Create the temperature sensor.
  DallasSensor g_secondary_temp_sensor = DallasSensor(&g_print);
  // Create the temperature/humidity sensor.
  Dht22Sensor g_primary_sensor = Dht22Sensor(&g_print);
#endif

// Create the clock with the real time device.
RealClock g_clock;

// Create the LCD display output.
Lcd g_lcd;

Status GetSystemStatus() {
  return g_status;
}

// Wire up the thermostat decorators. Each layer does one specific task which has significant advantages:
// Pros:
// + Easy to unit test each piece individually
// + Each class can clearly identify what it does in the class name.
// + Each class does one specific behavior.
// + Clear code organization.
// + Avoids spagetti code which mixes different logic.
// + Avoids the monolithic code which causes developer fatigue when things don't work or need to make changes/add features.
// + Allows for removing and replacing different parts easily.
// + Makes the code significantly more readable.
// + Uses a more Modern coding design.
// Cons:
// - Has a small upfront cost of some extra data elements and code forwarding.
// - Requires thinking about the order in which things are hooked together, it's not all in one place.
// - At first glance, this wiring looks scary, however having the wiring all in one place ensures software engineering practices keep the code well organized.
//   - An integration test (not unit tests) should verify this wiring.
//
// Normally shared_ptr/unique_ptr objects would be used, however for embedded AVR, avoiding malloc/new saves resources. Therefore global
// objects for static memory usage accounting is used.
WrapperThermostatTask wrapper_thermostat_task; // This is only for convenience, and could be removed by removing the wrapper from the last ThermostatTask.
SensorUpdatingThermostatTask g_sensor_updating_thermostat_task(&g_clock, &g_primary_sensor, &g_secondary_temp_sensor, &g_print, &wrapper_thermostat_task);
HvacControllerThermostatTask g_hvac_controller_thermostat_task(&g_clock, &g_print, &g_sensor_updating_thermostat_task);
LockoutControllingThermostatTask g_lockout_controlling_thermostat_task(&g_hvac_controller_thermostat_task);
HeatAdvancingThermostatTask g_heat_advancing_thermostat_task(&g_lockout_controlling_thermostat_task);
FanControllerThermostatTask g_fan_controller_thermostat_task(&g_clock, &g_print, &g_heat_advancing_thermostat_task);
RelaySettingThermostatTask g_relay_setting_thermostat_task(&g_relays, &g_print, &GetSystemStatus, &g_fan_controller_thermostat_task);
UpdateDisplayThermostatTask g_update_display_thermostat_task(&g_lcd, &g_print, &g_relay_setting_thermostat_task);
ErrorDisplayingThermostatTask g_error_displaying_thermostat_task(&g_lcd, &g_print, &g_update_display_thermostat_task);
HistoryUpdatingThermostatTask g_history_updating_thermostat_task(&g_error_displaying_thermostat_task);
LoggingThermostatTask g_logging_thermostat_task(&g_print, &g_history_updating_thermostat_task);
PacingThermostatTask g_pacing_thermostat_task(&g_clock, &g_logging_thermostat_task);

// Resulting CreateThermostatTask pointer.
ThermostatTask* const g_thermostat_task = &g_pacing_thermostat_task;

void setup() {
  // Add an extra Timer0 interrupt for the backlight dimming.
  CustomInterruptSetup();

  // Setup the relay ports.
  g_relays.SetUp();

  g_print.SetUp();
  Wire.begin();

  // We use bme for humidity and indoor air quality.
  g_primary_sensor.SetUp();
  // We use dallas for temperature
  g_secondary_temp_sensor.SetUp();

  // Start handling interrupts.
  sei();

  // Setup the LCD and custom characters.
  g_lcd.SetUp();
  g_print.print("Started...\n");
}

// Waits for a button press with a timeout while ensuring the HVAC task always gets called.
//
// All blocking calls should use this instead of using sleep.
//
// Menus use this to ensure blocking behaviors continue maintaining HVAC control.
Button WaitForButtonPress(const uint32_t timeout) {
  const uint32_t start = g_clock.Millis();
  Button button = Button::NONE;

  while (button == Button::NONE) {

    // Keep calling the layered thermostat decorators which make the HVAC system work. The thermostat task implements pacing to avoid being called to frequently.
    const Status status = g_thermostat_task->RunOnce(&g_settings);

    // Poll for single button presses.
    //
    // This uses a decorator pattern to add hysteresis and debouncing logic.
    button = Buttons::GetSinglePress(
               Buttons::StabilizedButtonPressed(Buttons::GetButton(analogRead(0))), g_clock.Millis());

    if (g_clock.millisSince(start) >= timeout) {
      return Button::TIMEOUT;
    }
  }

  // When a button is pressed, turn on the backlight for some length of time.
  g_backlight_count = 10000;

  return button;
}


// Menu uses the user input buttons and user output lcd line 2 to manipulate the settings
// fields.
//
// All blocking calls use WaitForButtonPress to ensure MaintainHvac is always periodically called.
Menus menu(&g_settings, &WaitForButtonPress, &g_clock, &g_lcd, &g_storer);


void loop() {
  // Show basic time information in the bottom row.
  //
  // This is blocking using the WaitForButtonPress function.
  const Button button = menu.InformationalState();

  // Up/Down on the main screen allows setting an override temperature which holds for 2 hours.
  if (button == Button::UP || button == Button::DOWN) {
    // This is blocking using the WaitForButtonPress function.
    menu.EditOverrideTemp();
  }

  if (button == Button::LEFT) {
    // Each left press takes the user through a status field.
    //
    // This is blocking using the WaitForButtonPress function.
    menu.ShowStatuses();
  }
  if (button == Button::RIGHT) {
    // Each left press takes the user through edit options. Using the up/down on an edit option allows the user to edit the field, and pressing select updates the field.
    //
    // This is blocking using the WaitForButtonPress function.
    menu.EditSettings();
  }
  // Once we're done editing settings, return back to showing the informational state.
}
