// GridBot — badges gallery: all 13 achievements with earned/locked state.
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"
#include "game/Profile.h"

namespace screens {

class BadgesScreen : public app::IScreen {
 public:
  void begin(const gb::Profile* p) { _p = p; }
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;

 private:
  void draw();
  const gb::Profile* _p = nullptr;
  app::TapDetector _tap;
};

}  // namespace screens
