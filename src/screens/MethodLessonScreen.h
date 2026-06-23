// NeuroBot lesson — THE RIGHT TOOL. The big practical-ML lesson: no method is best at everything;
// match the method (and the brain type) to the problem. A few quick "which fits?" challenges drawn
// straight from this game's own findings -- memory helps a dead-end maze but is noise in a reflex
// battle; if you already KNOW the answer, just Teach it. Exercise: pick right on all of them.
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"

namespace screens {

class MethodLessonScreen : public app::IScreen {
 public:
  void begin();
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;

 private:
  void draw();
  int _q = 0;          // current question
  int _score = 0;      // correct so far
  int _picked = -1;    // chosen option (-1 = not yet answered this question)
  app::TapDetector _tap;
};

}  // namespace screens
