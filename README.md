# Arduino DIY Thermostat
Arduino Home thermostat control with heat, cool, fan, and humidity controls

![Thermostat LCD](https://github.com/ccasper/thermostat/blob/master/screenshots/display_lcd.jpg?raw=true)

# Configurable Settings:
 - Modes selection for Cool only, Heat only, Off, Both on.
 - 2 hour temperature override
 - Ability to extend the Fan On time after heating/cooling cycle stops for better temperature balancing.
 - Fan Always On setting
 - Humidifier humidity setting from 30%-90%
 - Two 7-day configurable time cooling setpoints.
 - Two 7-day configurable time heating setpoints.
 - Adjustable RTC time.
 - Configurable tolerance for how much temperature the can fluctuate before HVAC turns on.
 - Displays HVAC on time percentage over the last 10 on/off cycles.
 - Displays Indoor Air Quality.

# Implementation

## Code layout
The Arduino sketch only consists of header files and not cpp files because of the nature of the Arduino IDE. The Arduino IDE automatically pulls in header files in the list of files, and therefore it's much easier to develop rather than requiring a library. Simply open the thermostat.ino and the headers will automatically be visible in separate tabs!

### interface.h
This file consists of the abstract interfaces that allow dependency injecting new implementations or easier unit testing of the thermostat. If you are using different hardware than described below, simply implement the interface with your hardware device specifications and use this implementation instead!

### avr_impls.h
This file consists of all the avr/chip implementations that are described in the hardware section. These implement the abstract interfaces in interface.h.

### buttons.h
This file makes it easy to decode, debounce, and button long hold fast iterating.

### calculate_iaq_score.h
This file calculates the indoor air quality based on humidity and VOC (volatile organic compounds) gas resistance using the BME680 sensor.

### comparison.h
Functional min/max functions for Arduino since the existing implementations do not work well.

### events.h
Helpers methods for gaining information from the past heating and cooling cycles.

### fan_controller.h
Since the fan on/off logic is quite complex, the hvac uses this class to separate fan enabling behavior.

This allows for:

0. Extended fan on time after heating/cooling has turned off
0. Cycling the fan on for 30 minutes when the fan hasn't run in 3 hours. 

### maintain_hvac.h
This is the core HVAC control logic for making the system relays turn on and off at the appropriate times.

This logic runs every 2.5 seconds even when operating in the menus.

### menus.h
This file contains all the menu and status related logic for giving the user options to adjust and change settings.

### settings.h
This file contains the settings data object and some helpers. The helpers are able to read/write to EEPROM to persist the settings. 

This settings object consists of persistent data (stored in EEPROM) such as setpoints, thresholds, and modes that should be maintained through power cycles.

This settings object also consists of current settings, such as current temperature, recent events history, and any manual temperature override.

## LCD Display
The first row is controlled entirely by the MaintainHvac() function and shows the current mean indoor temperature, humidity, Heating/Cooling/Fan state, and a number that updates every 2 seconds showing that MaintainHvac() is still working.

The second row is managed by the Menus to show statuses and edit settings. The default status is to show the current RTC time.

## Buttons
Pressing the Left button guides you through other informational statuses, and pressing the Right button guides you through editable settings.

Up and Down arrows on the main status screen will allow you to set an override temperature for 2 hours. On the edit menus, it allows changing settings.

## Hardware

This Arduino sketch uses the following parts:

	- Arduino Mega 2560 board or similar.
	- 1602 LCD Keypad Shield (6 buttons + 16x2 LCD)
    - Arduino compatible BME680 Temperature/Humidity sensor (primarily for volatile gas sensing)
	- Other Temp/Humidity sensors can be used, but the code will need to be adapted for them, such as BMP085 which I used previously.
	- DS1307 clock chip+AT24C128 storage+DS18B20 temp sensor triple module +CR1220 SMD
	- 5v 4 Channel OMRON SSR G3MB-202P Solid State Relay Module

	Parts total cost: ~$34 + Arduino Mega. 

A small amount of soldering is necessary to wire up the accessory boards.

However, if you have different components is very easy to add. Simply implement the Sensor, Display, or Clock interface (see interface.h) and use this when calling MaintainHVAC().



	![Thermostat Arduino IDE](https://github.com/ccasper/thermostat/blob/master/screenshots/arduino-tabs.png?raw=true)

