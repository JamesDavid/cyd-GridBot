// GridBot — STATS: level reached, total stars, win rate, and the command-usage
// histogram as a little bar chart (SPEC §9).
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"
#include "game/Profile.h"

namespace screens {

class StatsScreen : public app::IScreen {
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
