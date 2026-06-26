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
  void clearCalibration();      // delete stored calibration (forces re-cal on reboot)
  void inject(int x, int y, int frames = 1);  // synthetic touch held for `frames` reads (1 = a tap)
  void setLog(bool on) { _log = on; }

 private:
  bool loadCalibration();
  void saveCalibration(const uint16_t* data);
  void runCalibration();        // draws corners, stores result

  int _injX = 0, _injY = 0, _injCount = 0;  // synthetic tap state
  bool _log = false;
  uint32_t _lastLog = 0;
};

extern Touch touch;

}  // namespace hal
