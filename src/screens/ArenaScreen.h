// GridBot — Arena Race demo (SPEC §18). Pits the player's program (their latest
// library bot, or the wall-follower) against a built-in AI on a symmetric board and
// animates the deterministic match. A minimal capstone; authoring reuse, opponent
// picker, hotseat and Sumo/Zap are in BACKLOG.
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"
#include "game/Maze.h"
#include "game/Arena.h"
#include "game/Profile.h"

namespace screens {

class ArenaScreen : public app::IScreen {
 public:
  void begin(gb::Profile* profile);
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;

 private:
  void drawBoard();
  void mazeGeometry(int& tile, int& ox, int& oy);
  void drawCell(int r, int c);
  void drawBot(int i, const gb::Pose& p, int avatar);
  void finishOverlay();

  gb::Profile* _profile = nullptr;
  gb::Maze _maze;
  gb::Program _p0, _p1;
  gb::Pose _s0, _s1;
  gb::Arena _arena;
  gb::Pose _drawn[2];
  bool _running = false;
  bool _done = false;
  uint32_t _last = 0;
  app::TapDetector _tap;
};

}  // namespace screens
