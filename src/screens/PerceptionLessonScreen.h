// NeuroLab lesson — Perception (raw -> meaning). Every other lesson HANDS the brain clean
// senses ("wall ahead = 1"). But where do those come from? A little perception net looks at
// the raw squares the robot can see and decides "wall ahead? yes/no". Turning raw input into
// meaning is the hard part real robots spend the most on. (read-only, example-cycling demo)
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"

namespace screens {

class PerceptionLessonScreen : public app::IScreen {
 public:
  void begin();
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;

 private:
  void draw();
  int _phase = 0;   // 0 the puzzle, 1 the perception net classifying, 2 the takeaway
  int _view = 0;    // which example "robot's-eye view" is shown
  app::TapDetector _tap;
};

}  // namespace screens
