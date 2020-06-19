# thermostat
Arduino Home thermostat control with heat, cool, fan, and humidity controls

This is an Arduino sketch for a Home Thermostat using the following parts:
- Arduino Mega 2560 board or similar.
- 1602 LCD Keypad Shield (6 buttons + 16x2 LCD)
- Arduino compatible BME680 Temperature/Humidity sensor (primarily for volatile gas sensing)
  - Other Temp/Humidity sensors can be used, but the code will need to be adapted for them, such as BMP085 which I used previously.
- DS1307 clock chip+AT24C128 storage+DS18B20 temp sensor triple module +CR1220 SMD
- 5v 4 Channel OMRON SSR G3MB-202P Solid State Relay Module

Parts total cost: ~$34 + Arduino Mega. 

A small amount of soldering is necessary to wire up the accessory boards.

The Arduino sketch only consists of header files and not cpp files because of the nature of the Arduino IDE. It automatically pulls in header files in the list of files, and therefore it's much easier to develop rather than requiring a library. Simply open the thermostat.ino and the headers will automatically be visible in separate tabs!

![Thermostat Arduino IDE](https://github.com/ccasper/thermostat/blob/master/screenshots/arduino-tabs.png?raw=true)

