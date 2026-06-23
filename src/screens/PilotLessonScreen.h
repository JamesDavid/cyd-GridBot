// NeuroLab lesson — Pilot mode (planner + follower, the Tesla-FSD idea). A reactive brain
// only senses what's nearby, so on a twisty maze it gets stuck. Add a route PLANNER that
// lays down waypoints and the same brain clears it — the planner decides WHERE, the brain
// decides HOW. (engines: Net / Pilot)
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"
#include "game/Maze.h"
#include "game/Net.h"

namespace screens {

class PilotLessonScreen : public app::IScreen {
 public:
  void begin();
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;

 private:
  void draw();
  void mazeGeom(int& tile, int& ox, int& oy) const;
  void trace(bool pilot);   // run the brain (with/without the planner) and record its path

  gb::Maze _maze;
  gb::Net _brain;
  int _phase = 0;           // 0 brain-alone (stuck), 1 with planner (solved), 2 the analogy
  uint8_t _path[80];
  int _pathLen = 0;
  uint8_t _wp[40];
  int _wpN = 0;
  bool _won = false;
  app::TapDetector _tap;
};

}  // namespace screens
