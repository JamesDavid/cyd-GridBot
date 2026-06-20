// GridBot — Puzzle Race (a multiplayer mode). Both players program the SAME maze under
// a countdown; whoever's robot ends up closest to the goal (or reaches it) wins. A
// code-authoring contest, not a live battle. Hotseat (one device, lock-in handoff).
// Uses the SHARED block editor (ProgramEditor) — you PROGRAM, you don't drive.
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"
#include "game/Maze.h"
#include "game/Program.h"
#include "game/Profile.h"
#include "screens/ProgramEditor.h"

namespace screens {

class PuzzleRaceScreen : public app::IScreen {
 public:
  void begin(gb::Profile* profile);
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;

 private:
  enum class Phase : uint8_t { AUTHOR, HANDOFF, RESULT };
  void attachEditor();   // bind the shared editor to the current player's program
  void drawAuthor();
  void drawBoard();      // peek: show the shared maze full-screen
  void drawHandoff();
  void drawResult();
  int scoreProgram(const gb::Program& p);  // final BFS distance to goal (0 = reached)
  void lockIn(uint32_t now);

  gb::Profile* _profile = nullptr;
  gb::Maze _maze;
  gb::Program _prog[2];   // [0]=P1, [1]=P2
  ProgramEditor _editor;  // the same control-pad + program-list editor used everywhere
  int _player = 0;        // 0 or 1 currently authoring
  int _dist[2] = {-1, -1};
  int _par = 1;
  bool _peeking = false;  // showing the maze instead of the editor
  Phase _phase = Phase::AUTHOR;
  uint32_t _deadline = 0;
  uint32_t _gameNo = 0;   // increments each game (incl. "Again") so the maze varies
  int _shownSec = -1;     // last drawn countdown value (avoid per-tick flicker)
  app::TapDetector _tap;
};

}  // namespace screens
