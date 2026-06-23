// NeuroLab lesson — TWO lessons share this screen (same brain+maze+distill mechanics):
//  mode 0 "Transfer": train on maze A; on a NEW maze B it already does okay (general skills),
//          a quick fine-tune masters B without starting over.
//  mode 1 "Data": YOU are the teacher — tag the path (draw it tile-by-tile, reusing the
//          campaign trainer's Draw mode), then train the brain to copy your tags. (Net / Distill)
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
  void drawData();         // mode 1: tag-a-path-and-train interaction
  void mazeGeom(int& tile, int& ox, int& oy) const;
  void trace();
  // mode-1 "Draw" (tag) reused from the campaign trainer:
  bool tileAtPixel(int x, int y, int& r, int& c) const;
  void seedDrawStart();
  void handleDrawTap(int r, int c);

  gb::Maze _maze;
  gb::Net _brain;
  int _mode = 0;           // 0 Transfer, 1 Data (same mechanics, different framing/labels)
  int _phase = 0;          // 0 untrained, 1 learned A, 2 on new B (transfer), 3 fine-tuned B
  int _examples = 0;       // (Data mode) running count of examples it has learned from
  uint8_t _path[64];
  int _pathLen = 0;
  bool _won = false;
  uint8_t _drawPath[64];   // (Data mode) the tiles the kid tagged
  int _drawLen = 0;
  bool _drawTrained = false;
  app::TapDetector _tap;
};

}  // namespace screens
