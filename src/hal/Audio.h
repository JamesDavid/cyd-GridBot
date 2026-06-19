// GridBot — tiny tone helper on the speaker / DAC pin (GPIO26) via LEDC tone
// (SPEC §13). All calls are no-ops when sound is disabled.
#pragma once
#include <stdint.h>

namespace hal {

class Audio {
 public:
  void begin();
  void setEnabled(bool on) { _on = on; }
  void blip();   // tap feedback
  void tick();   // step tick
  void win();    // happy jingle
  void fail();   // sad buzz

 private:
  void tone(uint16_t freq, uint16_t ms);
  bool _on = true;
};

extern Audio audio;

}  // namespace hal
