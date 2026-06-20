// GridBot — shared-seed Challenge picker. Pick a 4-digit code; everyone who plays the
// same code gets the IDENTICAL maze (one fixed difficulty), so you can race a friend on
// the exact same board. Wins award no campaign progress (see GameScreen::beginChallenge).
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"
#include "game/Profile.h"

namespace screens {

class ChallengeScreen : public app::IScreen {
 public:
  void begin(gb::Profile* profile);
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;

  uint32_t code() const { return _code; }

 private:
  void draw();

  gb::Profile* _profile = nullptr;
  uint32_t _code = 1;
  app::TapDetector _tap;
};

}  // namespace screens
