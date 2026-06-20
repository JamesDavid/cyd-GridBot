// NeuroLab lesson — transfer learning. Train a brain on maze A; on a NEW maze B it
// already does okay (it learned general skills, not the specific maze); a quick
// fine-tune masters B without starting from scratch. The egocentric brain is what makes
// skills transfer. (engines: Net / Distill)
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"
#include "game/Maze.h"
#include "game/Net.h"

namespace screens {

class TransferLessonScreen : public app::IScreen {
 public:
  void begin();
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;

 private:
  void draw();
  void mazeGeom(int& tile, int& ox, int& oy) const;
  void trace();

  gb::Maze _maze;
  gb::Net _brain;
  int _phase = 0;          // 0 untrained, 1 learned A, 2 on new B (transfer), 3 fine-tuned B
  uint8_t _path[64];
  int _pathLen = 0;
  bool _won = false;
  app::TapDetector _tap;
};

}  // namespace screens
