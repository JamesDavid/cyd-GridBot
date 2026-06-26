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

// Draw / erase a crosshair target.
static void drawCross(LGFX& g, int x, int y, uint16_t col) {
  g.fillRect(x - 1, y - 13, 3, 27, col);
  g.fillRect(x - 13, y - 1, 27, 3, col);
}

// Wait for a firm touch and return an averaged raw ADC reading (mirrors LovyanGFX's own
// sampler: 8 pairs, each pair must agree within RAWERR so a wobble doesn't poison the mean).
static void readStableRaw(LGFX& g, int32_t& ax, int32_t& ay) {
  static constexpr int RAWERR = 20;
  int32_t sx = 0, sy = 0;
  for (int j = 0; j < 8; ++j) {
    int32_t x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    for (;;) {
      while (!g.getTouchRaw(&x1, &y1)) delay(2);
      delay(10);
      int32_t dx = x1 - x2, dy = y1 - y2;
      if (g.getTouchRaw(&x2, &y2)) {
        dx = x1 - x2; dy = y1 - y2;
        if ((dx < 0 ? -dx : dx) <= RAWERR && (dy < 0 ? -dy : dy) <= RAWERR) break;
      }
    }
    sx += x1 + x2; sy += y1 + y2;
  }
  ax = sx >> 4; ay = sy >> 4;
}

// Four-target touch calibration. Unlike LovyanGFX's built-in (which puts the targets at the
// very corners -- awkward to hit, often under the bezel), we draw them INSET a little, sample
// the raw ADC there, fit the (linear) raw<->screen relationship, then extrapolate the raw values
// back out to the true corners so the stock fitter still maps the whole screen accurately.
// Both panel & touch offset_rotation are 0, so the native orientation is rotation 0 -- we sample
// there (matching what setTouchCalibrate's corner mapping assumes), then restore the app rotation.
void Touch::runCalibration() {
  auto& g = display.gfx();
  uint_fast8_t rot = g.getRotation();
  g.fillScreen(TFT_BLACK);
  g.setTextColor(TFT_WHITE, TFT_BLACK);
  g.setTextDatum(textdatum_t::middle_center);
  g.drawString("Screen calibration", g.width() / 2, g.height() / 2 - 16);
  g.drawString("tap each target", g.width() / 2, g.height() / 2 + 6);

  g.setRotation(0);
  const int w = g.width(), h = g.height();
  const int d = (w < h ? w : h) / 10;   // inset ~10% from each edge ("slightly in", easy to hit)
  // i-order matches setTouchCalibrate's corner map: 0=(0,0) 1=(0,h-1) 2=(w-1,0) 3=(w-1,h-1).
  const int tx[4] = {d, d, w - 1 - d, w - 1 - d};
  const int ty[4] = {d, h - 1 - d, d, h - 1 - d};
  int32_t rx[4], ry[4];
  for (int i = 0; i < 4; ++i) {
    drawCross(g, tx[i], ty[i], TFT_WHITE);
    delay(300);
    readStableRaw(g, rx[i], ry[i]);
    drawCross(g, tx[i], ty[i], TFT_BLACK);   // erase
    int32_t jx, jy; do { delay(2); } while (g.getTouchRaw(&jx, &jy));  // wait for release
  }

  // Fit raw = P*sx + Q*sy + R per axis (average two estimates of each slope for noise), then
  // evaluate at the true corners -> the 8 values setTouchCalibrate expects.
  const float spanX = (float)(w - 1 - 2 * d), spanY = (float)(h - 1 - 2 * d);
  const float Px = 0.5f * (((rx[2] - rx[0]) + (rx[3] - rx[1])) / spanX);
  const float Py = 0.5f * (((ry[2] - ry[0]) + (ry[3] - ry[1])) / spanX);
  const float Qx = 0.5f * (((rx[1] - rx[0]) + (rx[3] - rx[2])) / spanY);
  const float Qy = 0.5f * (((ry[1] - ry[0]) + (ry[3] - ry[2])) / spanY);
  float Rx = 0, Ry = 0;
  for (int i = 0; i < 4; ++i) { Rx += rx[i] - Px * tx[i] - Qx * ty[i]; Ry += ry[i] - Py * tx[i] - Qy * ty[i]; }
  Rx *= 0.25f; Ry *= 0.25f;
  const int cx[4] = {0, 0, w - 1, w - 1}, cy[4] = {0, h - 1, 0, h - 1};
  uint16_t cal[8];
  for (int i = 0; i < 4; ++i) {
    float vx = Px * cx[i] + Qx * cy[i] + Rx;
    float vy = Py * cx[i] + Qy * cy[i] + Ry;
    cal[i * 2]     = (uint16_t)(vx < 0 ? 0 : vx > 65535 ? 65535 : vx);
    cal[i * 2 + 1] = (uint16_t)(vy < 0 ? 0 : vy > 65535 ? 65535 : vy);
  }

  g.setRotation(rot);            // back to the app's rotation
  g.setTouchCalibrate(cal);      // re-fit the affine from the (extrapolated) corner values
  saveCalibration(cal);
  g.fillScreen(TFT_BLACK);
}

void Touch::recalibrate() { runCalibration(); }

void Touch::clearCalibration() {
  if (LittleFS.exists(CALIB_PATH)) LittleFS.remove(CALIB_PATH);
}

void Touch::inject(int x, int y, int frames) {
  // Normally ONE pressed read, then released — so a synthetic tap is consumed by a single screen
  // and never cascades through a transition (each new screen's tap detector would otherwise see the
  // still-held press as a fresh tap). `frames` > 1 holds the touch (for testing the long-press menu).
  _injX = x; _injY = y; _injCount = frames < 1 ? 1 : frames;
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
