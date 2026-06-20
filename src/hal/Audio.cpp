#include "hal/Audio.h"
#include <Arduino.h>

namespace hal {

Audio audio;

static constexpr int SPK_PIN = 26;
static constexpr int SPK_CH = 0;

// A cheerful 8-bit theme that loops on the menu. (freq Hz, duration ms; 0 = rest)
const Note kTitleMusic[] = {
  {659, 170}, {784, 170}, {1047, 250}, {784, 170}, {880, 170}, {1047, 340}, {0, 110},
  {587, 170}, {698, 170}, {880, 250}, {698, 170}, {784, 170}, {659, 340}, {0, 220},
};
const int kTitleMusicLen = sizeof(kTitleMusic) / sizeof(kTitleMusic[0]);

void Audio::begin() {
  ledcSetup(SPK_CH, 2000, 8);
  ledcAttachPin(SPK_PIN, SPK_CH);
  ledcWriteTone(SPK_CH, 0);
}

void Audio::setEnabled(bool on) {
  _on = on;
  if (!on) { _playing = false; ledcWriteTone(SPK_CH, 0); }
}

void Audio::tone(uint16_t freq, uint16_t ms) {
  if (!_on) return;
  ledcWriteTone(SPK_CH, freq);
  delay(ms);
  ledcWriteTone(SPK_CH, 0);
  _noteStart = 0;  // make the melody re-assert its current note on the next update
}

void Audio::blip() { tone(880, 18); }
void Audio::tick() { tone(660, 12); }

void Audio::win() {
  if (!_on) return;
  tone(784, 110); tone(1047, 110); tone(1319, 110); tone(1047, 110); tone(1568, 320);
}

void Audio::fail() {
  if (!_on) return;
  tone(300, 120); tone(180, 160);
}

void Audio::badge() {
  if (!_on) return;
  tone(1047, 90); tone(1319, 90); tone(1568, 90); tone(2093, 240);
}

void Audio::startMusic(const Note* notes, int count, bool loop) {
  if (!_on || !notes || count <= 0) return;
  _mel = notes; _melLen = count; _melIdx = 0; _melLoop = loop;
  _noteStart = 0; _playing = true;
}

void Audio::stopMusic() {
  _playing = false;
  ledcWriteTone(SPK_CH, 0);
}

void Audio::update(uint32_t now) {
  if (!_playing || !_on || !_mel) return;
  if (_noteStart == 0) {                 // (re)start the current note
    _noteStart = now;
    ledcWriteTone(SPK_CH, _mel[_melIdx].freq);
    return;
  }
  if (now - _noteStart >= _mel[_melIdx].ms) {
    _melIdx++;
    if (_melIdx >= _melLen) {
      if (!_melLoop) { stopMusic(); return; }
      _melIdx = 0;
    }
    _noteStart = now;
    ledcWriteTone(SPK_CH, _mel[_melIdx].freq);
  }
}

}  // namespace hal
