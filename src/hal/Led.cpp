#include "hal/Led.h"
#include <Arduino.h>

namespace hal {

Led led;

static constexpr int R_PIN = 4, G_PIN = 16, B_PIN = 17;  // active LOW

void Led::begin() {
  pinMode(R_PIN, OUTPUT);
  pinMode(G_PIN, OUTPUT);
  pinMode(B_PIN, OUTPUT);
  off();
}

void Led::set(bool r, bool g, bool b) {
  digitalWrite(R_PIN, r ? LOW : HIGH);  // active LOW
  digitalWrite(G_PIN, g ? LOW : HIGH);
  digitalWrite(B_PIN, b ? LOW : HIGH);
}

void Led::off() { set(false, false, false); }
void Led::green() { set(false, true, false); }
void Led::red() { set(true, false, false); }

}  // namespace hal
