// Implements an indoor air quality percentage for the BME gas sensor.
#ifndef CALCULATE_IAQ_SCORE_H_
#define CALCULATE_IAQ_SCORE_H_

#include "comparison.h"

namespace thermostat {

// I hand rolled this implementation, but I am aware of another IAQ implementation at
// https://github.com/G6EJD/BME680-Example that this could be compared against.
float CalculateIaqScore(const float bme680_humidity, const uint32_t bme680_resistance) {
  // The ideal relative humidity for health and comfort is about 40–50%
  //
  // Shift the gas humidity of 45% to be the zero point, and calculate how far away from
  // that we are for the quality.
  float shifted_gas_humidity = abs(bme680_humidity - 45);

  // Next, subtract 5 for the ideal range being 45-5 45+5. Therefore 0 = ideal, and +40 to
  // +50 is the worst depending on low humidity or high humidity.
  shifted_gas_humidity = shifted_gas_humidity - 5;

  // Invert and scale so 100 = ideal air quality, and 0 = worst air quality.
  const float humidity_quality = 100 - (shifted_gas_humidity * 2);

  // Get a bounded gas resisance from the sensor.
  float gas_resistance = cmin(cmax(bme680_resistance, 5000UL /*bad air quality*/),
                              50000UL /*excellent air quality*/);

  // The gas resistance is now in the range 3.69 - 4.69 and by subtracting 3.69, we're now
  // in the range of 0-1 where 0 is bad, and 1 is excellent air quality.
  gas_resistance = log10(gas_resistance) - log10(5000);

  // Scale by 100, so 0 = bad and 100 = excellent.
  gas_resistance = gas_resistance * 100.0;

  // Returns the Indoor Air Quality partially based on resistance (volatiles) and humidity
  // (RH%). Returns a value of 100 = excellent, and 0 = bad.
  constexpr float resistance_weight = 0.75;
  constexpr float humidity_weight = (1.0 - resistance_weight);
  return (resistance_weight * gas_resistance) + humidity_weight * humidity_quality;
}

}
#endif  // CALCULATE_IAQ_SCORE_H_
