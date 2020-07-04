#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

#include "buttons.h"
#include "calculate_iaq_score.h"
#include "comparison.h"
#include "avr_impls.h"
#include "events.h"
#include "fan_controller.h"
#include "menus.h"
#include "avr_impls.h"
#include "settings.h"
#include "interfaces.h"
#include "maintain_hvac.h"
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

// Ensure 'int' is 16 bits (0 - 65535). If this assumption changes, it may cause code
// bugs.
static_assert(sizeof(int) == 2);

using namespace thermostat;

Output g_print;

EepromSettingsStorer g_storer;

// Restore the settings to use for the thermostat.
Settings g_settings = GetEepromOrDefaultSettings(&g_storer);

// Create the relays controller.
SsdRelays g_relays = SsdRelays();

// Create the temperature sensor
DallasSensor g_dallas_sensor = DallasSensor(&g_print);

// Create the temperature/humidity/gas quality sensor.
BmeSensor g_bme_sensor = BmeSensor(&g_print);

// Create the LCD display output.
Lcd g_lcd;

// Create the clock with the real time device.
RealClock g_clock;

// Create the fan controller, which is independent of the heat/cool control.
FanController g_fan;


void setup() {
  // Add an extra Timer0 interrupt for the backlight dimming.
  CustomInterruptSetup();

  // Setup the relay ports.
  g_relays.Setup();

  g_print.Setup();
  Wire.begin();

  // Setup the bme sensor hardware,
  g_bme_sensor.Setup();

  sei();

  // Setup the LCD and custom characters.
  g_lcd.Setup();
}

// Waits for a button press or specified timeout while maintaining calls to the HVAC system.
//
// Menus use this to ensure blocking behaviors continue maintaining HVAC control.
Button WaitForButtonPress(const uint32_t timeout) {
  const uint32_t start = g_clock.Millis();
  Button button = Button::NONE;

  while (button == Button::NONE) {
    Error error = MaintainHvac(&g_settings, &g_clock, &g_lcd, &g_relays, &g_fan, &g_bme_sensor, &g_dallas_sensor, &g_print);
    // Skip the status update when maintain hvac was skipped.
    if (error != Error::STATUS_NONE) {
      ShowStatus(&g_lcd, error);
    }

    // Poll for single button presses.
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
