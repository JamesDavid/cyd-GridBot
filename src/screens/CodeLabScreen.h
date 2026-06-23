// CodeLab — the coding-basics lessons hub (parallel to NeuroLab). Each teaches one core
// programming block with a runnable demo: Move, Jump, Repeat, Sense, Functions, then the
// neurosymbolic Brain block and a Debug-it lesson. Paginated (5 per page).
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"

namespace screens {

class CodeLabScreen : public app::IScreen {
 public:
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;
  int pick() const { return _pick; }

 private:
  void draw();
  int _pick = -1;
  int _page = 0;
  app::TapDetector _tap;
};

}  // namespace screens
