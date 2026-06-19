// GridBot — on-board RGB LED (GPIO 4/16/17, active LOW) ambient feedback (SPEC §13).
#pragma once
#include <stdint.h>

namespace hal {

class Led {
 public:
  void begin();
  void off();
  void green();   // win
  void red();     // fail
  void set(bool r, bool g, bool b);

 private:
  bool _on = true;
};

extern Led led;

}  // namespace hal
