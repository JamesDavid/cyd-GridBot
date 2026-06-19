#include "hal/Audio.h"
#include <Arduino.h>

namespace hal {

Audio audio;

static constexpr int SPK_PIN = 26;
static constexpr int SPK_CH = 0;

void Audio::begin() {
  ledcSetup(SPK_CH, 2000, 8);
  ledcAttachPin(SPK_PIN, SPK_CH);
  ledcWriteTone(SPK_CH, 0);
}

void Audio::tone(uint16_t freq, uint16_t ms) {
  if (!_on) return;
  ledcWriteTone(SPK_CH, freq);
  delay(ms);
  ledcWriteTone(SPK_CH, 0);
}

void Audio::blip() { tone(880, 18); }
void Audio::tick() { tone(660, 12); }

void Audio::win() {
  if (!_on) return;
  tone(660, 90); tone(880, 90); tone(1320, 160);
}

void Audio::fail() {
  if (!_on) return;
  tone(300, 120); tone(180, 160);
}

}  // namespace hal
