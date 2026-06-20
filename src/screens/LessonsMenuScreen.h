// The top-level Lessons menu: two tracks — CodeLab (programming basics) and NeuroLab
// (machine learning). pick() == 0 -> CodeLab, 1 -> NeuroLab.
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"

namespace screens {

class LessonsMenuScreen : public app::IScreen {
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
