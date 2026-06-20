// NeuroBot lesson — neuroevolution. A population of random brains tries the maze; the
// best one's path is traced, and a fitness curve climbs as generations breed the winners.
// "No teacher, just a score." (engine: game/Evolve)
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"
#include "game/Maze.h"
#include "game/Evolve.h"

namespace screens {

class EvoLessonScreen : public app::IScreen {
 public:
  void begin();
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;

 private:
  void draw();
  void mazeGeom(int& tile, int& ox, int& oy) const;
  void tracePath();

  gb::Maze _maze;
  gb::Evolve _evo;
  uint8_t _path[64];      // cells the best brain visits (r*cols+c), _pathLen long
  int _pathLen = 0;
  bool _won = false;
  static constexpr int HIST = 48;
  float _hist[HIST] = {0};   // best fitness per recent generation (ring of points)
  int _histN = 0;
  app::TapDetector _tap;
};

}  // namespace screens
