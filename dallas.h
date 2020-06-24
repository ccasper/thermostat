#ifndef DALLAS_H_
#define DALLAS_H_

#include <DallasTemperature.h>

// This functionality is for the Dallas DS18B20
namespace dallas {

#define ONE_WIRE_BUS 33

DeviceAddress temperature_address;
OneWire temperature_one_wire(ONE_WIRE_BUS);

DallasTemperature sensor(&temperature_one_wire);

void SetupTemperatureSensor() {
  sensor.begin();

  // We only have one dallas temperature sensor on the bus. Store this to avoid needing to
  // scan the bus on each read.
  sensor.getAddress(temperature_address, static_cast<uint8_t>(0));

  // When requesting a conversion, don't wait for the data, we'll collect the metrics
  // later.
  sensor.setWaitForConversion(false);
}

}  // namespace dallas
#endif  // DALLAS_H_
