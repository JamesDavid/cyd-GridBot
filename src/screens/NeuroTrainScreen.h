// NeuroBot — the "neuro interface": train the brain inside a NEURO block. Opened from
// the editor (tap a brain -> "train >"). The brain evolves to solve THIS level's maze;
// "Use it" saves the best brain back into the program's block. (engine: game/Evolve)
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"
#include "game/Maze.h"
#include "game/Program.h"
#include "game/Evolve.h"

namespace screens {

class NeuroTrainScreen : public app::IScreen {
 public:
  void begin(gb::Program* prog, int brainIdx, gb::Maze* maze);
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;

 private:
  void draw();
  void mazeGeom(int& tile, int& ox, int& oy) const;
  void tracePath();

  gb::Program* _prog = nullptr;
  int _idx = 0;
  gb::Maze* _maze = nullptr;
  gb::Evolve _evo;
  gb::Net _brain;         // the working brain (set by Teach=distill or Evolve)
  bool _taught = false;
  uint8_t _path[64];
  int _pathLen = 0;
  bool _won = false, _saved = false;
  app::TapDetector _tap;
};

}  // namespace screens
