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

thermostat::Settings g_settings = thermostat::GetEepromOrDefaultSettings();

thermostat::SsdRelays g_relays = thermostat::SsdRelays();

thermostat::DallasSensor g_dallas_sensor;

thermostat::BmeSensor g_bme_sensor;

thermostat::Lcd g_lcd;

thermostat::RealClock g_clock;

thermostat::FanController g_fan;


void setup() {
  // Add a extra Timer0 interrupt for IRQ stuff.
  CustomInterruptSetup();
  g_relays.Setup();

  Serial.begin(9600);
  Wire.begin();

  g_bme_sensor.Setup();

  sei();

  g_lcd.Setup();
}

// Waits for a button press while maintaining the HVAC system.
//
// Menus use this to ensure sleeping always updates the HVAC routine.
thermostat::Button WaitForButtonPress(const uint32_t timeout) {
  const uint32_t start = g_clock.Millis();
  thermostat::Button button = thermostat::Button::NONE;

  while (button == thermostat::Button::NONE) {
    thermostat::Error error = thermostat::MaintainHvac(&g_settings, &g_clock, &g_lcd, &g_relays, &g_fan, &g_bme_sensor, &g_dallas_sensor);
    // Skip the status update when maintain hvac was skipped.
    if (error != thermostat::Error::STATUS_NONE) {
      ShowStatus(&g_lcd, error);
    }

    // Poll for single button presses.
    button = thermostat::Buttons::GetSinglePress(
               thermostat::Buttons::StabilizedButtonPressed(thermostat::Buttons::GetButton(analogRead(0))), g_clock.Millis());

    if (g_clock.millisSince(start) >= timeout) {
      return thermostat::Button::TIMEOUT;
    }
  }

  // If a button was pressed, turn on the backlight for some length of time.
  g_backlight_count = 10000;

  return button;
}


// Menu uses the user input buttons and user output lcd line 2 to manipulate the settings
// fields.
//
// All blocking calls use WaitForButtonPress to ensure MaintainHvac is always periodically called.
thermostat::Menus menu(&g_settings, &WaitForButtonPress, &g_clock, &g_lcd);


void loop() {
  // Show basic time information in the bottom row.
  //
  // This is blocking.
  const thermostat::Button button = menu.InformationalState();

  // Override the temperature.
  if (button == thermostat::Button::UP || button == thermostat::Button::DOWN) {
    // This is blocking.
    menu.EditOverrideTemp();
  }

  if (button == thermostat::Button::LEFT) {
    // Go through the statuses.
    //
    // This is blocking.
    menu.ShowStatuses();
  }
  if (button == thermostat::Button::RIGHT) {
    // Go through the edit options.
    //
    // This is blocking.
    menu.EditSettings();
  }
  // Once we're done editing settings, return back to showing the informational state.
}
