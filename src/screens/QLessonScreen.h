// NeuroBot lesson — reinforcement learning (tabular Q). The robot tries the maze over
// and over; each cell is shaded by its learned VALUE and shows an arrow for the best
// move. Tap "Learn" and watch value spread back from the battery until a policy emerges
// — no code, no teacher, just reward. (engine: game/QLearn)
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"
#include "game/Maze.h"
#include "game/QLearn.h"

namespace screens {

class QLessonScreen : public app::IScreen {
 public:
  void begin();
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;

 private:
  void draw();
  void mazeGeom(int& tile, int& ox, int& oy) const;

  gb::Maze _maze;
  gb::QLearn _q;
  bool _solved = false;
  app::TapDetector _tap;
};

}  // namespace screens
