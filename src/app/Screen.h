// GridBot — screen interface for the App state machine (SPEC §3).
#pragma once
#include <stdint.h>
#include "hal/Touch.h"

namespace app {

enum class Signal : uint8_t {
  NONE,
  WON,           // game level cleared -> advance
  BACK,          // return to the previous screen
  PLAY,          // a profile was chosen -> start game
  NEW_PROFILE,   // "+ New Player" tapped -> profile create
  CREATED,       // profile-create finished -> create + play
  GOTO_STATS,    // open the stats screen
  GOTO_RADIO,    // open the radio (ESP-NOW) link screen
  EDIT_PROFILE,    // edit the current profile's name/avatar (keeps stats)
  GOTO_DRAW,       // open the pixel editor (custom character/goal)
  DELETE_PROFILE,  // delete the current profile (after multiple confirmations)
  GOTO_BADGES,     // open the badges gallery
  GOTO_SHOP,       // open the shop
  GOTO_PUZZLE,     // open Puzzle Race (shared-maze timed coding contest)
  GOTO_CHALLENGE,  // open the shared-seed Challenge picker
};

class IScreen {
 public:
  virtual ~IScreen() {}
  virtual void enter() = 0;  // draw initial frame
  virtual Signal tick(uint32_t now, const hal::TouchPoint& tp) = 0;
};

// Simple press-edge tap detector for resistive touch: fires once per press, after a
// short debounce. Also reports press duration so callers can do hold-to-peek.
class TapDetector {
 public:
  // Returns true on the rising edge (a new tap), filling x,y with the press point.
  bool tapped(const hal::TouchPoint& tp, uint32_t now, int& x, int& y) {
    bool fired = false;
    if (tp.pressed && !_down) {
      if (now - _lastUp >= 80) {  // debounce
        _down = true; _downAt = now; _x = tp.x; _y = tp.y;
        x = tp.x; y = tp.y; fired = true;
      }
    } else if (!tp.pressed && _down) {
      _down = false; _lastUp = now;
    } else if (tp.pressed) {
      _x = tp.x; _y = tp.y;  // track latest while held
    }
    return fired;
  }
  bool held() const { return _down; }
  uint32_t heldMs(uint32_t now) const { return _down ? now - _downAt : 0; }
  int x() const { return _x; }
  int y() const { return _y; }

 private:
  bool _down = false;
  uint32_t _downAt = 0, _lastUp = 0;
  int _x = 0, _y = 0;
};

}  // namespace app
