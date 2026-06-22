// NeuroLab lesson — TWO lessons share this screen (same brain+maze+distill mechanics):
//  mode 0 "Transfer": train on maze A; on a NEW maze B it already does okay (general skills),
//          a quick fine-tune masters B without starting over.
//  mode 1 "Data": the same steps reframed around DATA — Teach copies an expert (examples =
//          what it saw + the right move); on a new maze it hits unseen spots; add examples
//          there and retrain (the data loop). (engines: Net / Distill)
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"
#include "game/Maze.h"
#include "game/Net.h"

namespace screens {

class TransferLessonScreen : public app::IScreen {
 public:
  void begin(int mode = 0);   // 0 = Transfer, 1 = Data & labels
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;

 private:
  void draw();
  void mazeGeom(int& tile, int& ox, int& oy) const;
  void trace();

  gb::Maze _maze;
  gb::Net _brain;
  int _mode = 0;           // 0 Transfer, 1 Data (same mechanics, different framing/labels)
  int _phase = 0;          // 0 untrained, 1 learned A, 2 on new B (transfer), 3 fine-tuned B
  int _examples = 0;       // (Data mode) running count of examples it has learned from
  uint8_t _path[64];
  int _pathLen = 0;
  bool _won = false;
  app::TapDetector _tap;
};

}  // namespace screens
