// NeuroLab lesson — Perception (the senses we hand the brain). Every other lesson is HANDED
// clean senses like "wall ahead = 1". This shows where they come from: the robot's immediate
// surroundings turned into the numbers that ARE the brain's input layer. The honest caveats:
// in this game we just hand the values; a real robot would need a camera + sensors to work them
// out (that's perception, the hard part); choosing WHICH values to sense is an engineering call;
// and these are the robot's LOCAL surroundings, not a top-down map of the whole world.
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
  int _phase = 0;   // 0 the senses, 1 we-hand-them/real-robots-perceive, 2 local-not-global
  int _scene = 0;   // which surroundings example is shown
  app::TapDetector _tap;
};

}  // namespace screens
