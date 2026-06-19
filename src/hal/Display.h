// GridBot — display HAL. Owns the single LGFX instance and the LEDC backlight.
#pragma once
#include "hal/LGFX_Config.h"

namespace hal {

class Display {
 public:
  void begin();
  LGFX& gfx() { return _lcd; }
  void setBacklight(uint8_t level);  // 0..255

 private:
  LGFX _lcd;
};

extern Display display;  // single global instance (defined in Display.cpp)

}  // namespace hal
