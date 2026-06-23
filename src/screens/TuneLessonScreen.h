// NeuroBot lesson 10 — TUNING the knobs. Same reward maze as Q-learning, but now the kid turns
// the dials: EXPLORE (epsilon) and STEP (learning rate). With explore=0 the robot never tries new
// moves and stays stuck; turn it up and value spreads and it solves. The exercise: get it to solve
// the maze, then beat the par number of tries. Teaches hyperparameters by feel. (engine: QLearn)
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"
#include "game/Maze.h"
#include "game/QLearn.h"

namespace screens {

class TuneLessonScreen : public app::IScreen {
 public:
  void begin();
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;

 private:
  void draw();
  void mazeGeom(int& tile, int& ox, int& oy) const;
  void reset();   // re-seed Q with the current knobs (fresh learning to compare fairly)

  gb::Maze _maze;
  gb::QLearn _q;
  float _eps = 0.0f;     // explore knob (starts at 0 so the kid SEES it stuck, then turns it up)
  float _alpha = 0.5f;   // step (learning-rate) knob
  bool _solved = false;
  int  _par = 6;         // batches of 20 to beat (the exercise)
  app::TapDetector _tap;
};

}  // namespace screens
