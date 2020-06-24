#ifndef RELAYS_H_
#define RELAYS_H_

// SCR relays
namespace relays {

constexpr int ON = LOW;
constexpr int OFF = HIGH;

// Customize these pins for your own board.
constexpr int SSR1_PIN = 34;
constexpr int SSR2_PIN = 35;
constexpr int SSR4_PIN = 36;
constexpr int SSR3_PIN = 37;

void SetupRelays() {
  digitalWrite(SSR1_PIN, OFF);  // Relay (1)
  digitalWrite(SSR2_PIN, OFF);  // Relay (2)
  digitalWrite(SSR4_PIN, OFF);  // Relay (4)
  digitalWrite(SSR3_PIN, OFF);  // Relay (3)
  pinMode(SSR1_PIN, OUTPUT);
  pinMode(SSR2_PIN, OUTPUT);
  pinMode(SSR4_PIN, OUTPUT);
  pinMode(SSR3_PIN, OUTPUT);
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

}  // namespace relays

#endif  // RELAYS_H_
