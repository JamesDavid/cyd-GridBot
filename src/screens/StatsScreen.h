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
  void drawConfirm();
  const gb::Profile* _p = nullptr;
  uint8_t _confirm = 0;   // 0 = none, 1 = first confirm, 2 = second confirm
  app::TapDetector _tap;
};

}  // namespace screens
