// CodeLab — the coding-basics lessons hub (parallel to NeuroLab). Each teaches one core
// programming block with a runnable demo: 0 Move, 1 Repeat, 2 Sense, 3 Functions, 4 Jump.
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
  app::TapDetector _tap;
};

}  // namespace screens
