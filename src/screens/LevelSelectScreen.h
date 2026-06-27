// Paginated campaign level browser ("go for gold"). Shows every unlocked level with its best stars
// and fewest blocks vs par; tap one to (re)play it and trim your program for more stars. Levels
// beyond your furthest are locked (greyed). Backed by Profile::levelRec (see Profile.h).
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"
#include "game/Profile.h"

namespace screens {

class LevelSelectScreen : public app::IScreen {
 public:
  void begin(gb::Profile* profile);                 // opens on the page holding the current level
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;
  uint32_t pickedLevel() const { return _picked; }  // level to (re)play, set when a tile is tapped

 private:
  void draw();
  int pages() const;
  gb::Profile* _p = nullptr;
  int _page = 0;
  uint32_t _picked = 0;
  app::TapDetector _tap;
};

}  // namespace screens
