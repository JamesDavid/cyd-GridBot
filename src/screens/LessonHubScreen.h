// NeuroLab — the lessons hub. Each lesson teaches one way machines learn:
//   0  one neuron, backprop  (learn from a teacher)
//   1  Q-learning            (learn from reward)
//   2  evolution             (breed the best, no teacher)
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"

namespace screens {

class LessonHubScreen : public app::IScreen {
 public:
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;
  int pick() const { return _pick; }  // which lesson was chosen (read after PLAY)

 private:
  void draw();
  int _pick = -1;
  int _page = 0;            // 0 or 1 — lessons are paginated 5 per page (10 total)
  app::TapDetector _tap;
};

}  // namespace screens
