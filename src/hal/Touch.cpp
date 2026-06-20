#include "hal/Touch.h"
#include "hal/Display.h"
#include <Arduino.h>
#include <LittleFS.h>

namespace hal {

Touch touch;

static const char* CALIB_PATH = "/calib.dat";
// File format: [uint16 magic=0x6742][uint16 rotation][7 x uint16 calibration]
static constexpr uint16_t CALIB_MAGIC = 0x6742;  // 'gB'

void Touch::begin() {
  if (!loadCalibration()) {
    runCalibration();
  }
}

bool Touch::loadCalibration() {
  if (!LittleFS.exists(CALIB_PATH)) return false;
  File f = LittleFS.open(CALIB_PATH, "r");
  if (!f) return false;
  uint16_t hdr[2];
  uint16_t cal[8];
  bool ok = f.read((uint8_t*)hdr, sizeof(hdr)) == sizeof(hdr) &&
            f.read((uint8_t*)cal, sizeof(uint16_t) * 8) == (int)(sizeof(uint16_t) * 8);
  f.close();
  if (!ok || hdr[0] != CALIB_MAGIC) return false;
  if (hdr[1] != DISPLAY_DEFAULT_ROTATION) return false;  // rotation changed -> stale
  display.gfx().setTouchCalibrate(cal);
  return true;
}

void Touch::saveCalibration(const uint16_t* data) {
  File f = LittleFS.open(CALIB_PATH, "w");
  if (!f) return;
  uint16_t hdr[2] = {CALIB_MAGIC, (uint16_t)DISPLAY_DEFAULT_ROTATION};
  f.write((const uint8_t*)hdr, sizeof(hdr));
  f.write((const uint8_t*)data, sizeof(uint16_t) * 8);
  f.close();
}

void Touch::runCalibration() {
  auto& g = display.gfx();
  g.fillScreen(TFT_BLACK);
  g.setTextColor(TFT_WHITE);
  g.setTextDatum(textdatum_t::middle_center);
  g.drawString("Touch the arrows", g.width() / 2, g.height() / 2 - 20);
  g.drawString("to calibrate", g.width() / 2, g.height() / 2 + 4);
  uint16_t cal[8] = {0};
  g.calibrateTouch(cal, TFT_WHITE, TFT_BLACK, 20);
  saveCalibration(cal);
  g.fillScreen(TFT_BLACK);
}

void Touch::recalibrate() { runCalibration(); }

void Touch::clearCalibration() {
  if (LittleFS.exists(CALIB_PATH)) LittleFS.remove(CALIB_PATH);
}

void Touch::inject(int x, int y) {
  // Exactly ONE pressed read, then released — so a synthetic tap is consumed by a
  // single screen and never cascades through a transition (each new screen's tap
  // detector would otherwise see the still-held press as a fresh tap).
  _injX = x; _injY = y; _injCount = 1;
}

TouchPoint Touch::read() {
  TouchPoint p;
  if (_injCount > 0) {                 // synthetic tap takes priority
    _injCount--;
    p.x = (int16_t)_injX; p.y = (int16_t)_injY; p.pressed = true;
    return p;
  }
  int32_t x = 0, y = 0;
  if (display.gfx().getTouch(&x, &y)) {
#if TOUCH_INVERT_X
    x = display.gfx().width() - 1 - x;
#endif
#if TOUCH_INVERT_Y
    y = display.gfx().height() - 1 - y;
#endif
    p.x = (int16_t)x;
    p.y = (int16_t)y;
    p.pressed = true;
    if (_log && millis() - _lastLog > 120) {
      _lastLog = millis();
      Serial.printf("TOUCH %d %d\n", p.x, p.y);
    }
  }
  return p;
}

}  // namespace hal
