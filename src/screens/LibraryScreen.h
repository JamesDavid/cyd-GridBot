// "My Bots" — the library manager. Lists every saved bot with its SOURCE (code editor +
// level / neuro trainer / Brain Cam / arena / radio-traded friend) and TYPE (code / brain /
// rnn / pilot), and lets you delete (to free a slot -- the library is capped) or rename one.
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"
#include "game/Profile.h"

namespace screens {

class LibraryScreen : public app::IScreen {
 public:
  void begin(gb::Profile* profile);
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;
  int renameIdx() const { return _renameIdx; }  // entry the keyboard should rename (-1 = none)

 private:
  void draw();
  gb::Profile* _p = nullptr;
  int _sel = -1;       // selected row (shows Rename/Delete chips), -1 = none
  int _scroll = 0;     // first visible row
  int _renameIdx = -1; // set when "Rename" tapped -> App opens the keyboard for this entry
  app::TapDetector _tap;
};

}  // namespace screens
