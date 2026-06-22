// NeuroLab lesson — Backprop, one step at a time. Like the Debug lesson's "do one thing,
// look, repeat": walks a SINGLE training example through the four moves of learning —
// LOOK (guess), SCORE (how wrong), BLAME (which weights to nudge, and why OFF inputs aren't),
// NUDGE (apply it; the guess gets closer) — with a "Next step >" button. Trains a real
// perceptron one nudge per tap, so the kid watches the guesses improve by hand.
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"
#include "game/Perceptron.h"

namespace screens {

class BackpropLessonScreen : public app::IScreen {
 public:
  void begin();
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;

 private:
  void draw();
  gb::Perceptron _p;
  int _ex = 0;     // which example (0..3)
  int _step = 0;   // 0 LOOK, 1 SCORE, 2 BLAME, 3 NUDGE
  app::TapDetector _tap;
};

}  // namespace screens
