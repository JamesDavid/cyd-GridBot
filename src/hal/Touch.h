// GridBot — touch HAL (XPT2046 via LovyanGFX). Calibration is stored in LittleFS
// (/calib.dat) and tagged with the active rotation, so changing the display
// rotation auto-invalidates it and re-runs the 4-corner routine.
#pragma once
#include <stdint.h>

namespace hal {

struct TouchPoint {
  int16_t x = 0, y = 0;
  bool pressed = false;
};

class Touch {
 public:
  void begin();                 // load calibration or run it if missing/stale
  TouchPoint read();            // debounced single read (invert flags applied)
  void recalibrate();           // force the 4-corner routine + persist

 private:
  bool loadCalibration();
  void saveCalibration(const uint16_t* data);
  void runCalibration();        // draws corners, stores result
};

extern Touch touch;

}  // namespace hal
