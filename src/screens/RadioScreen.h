// GridBot — Radio link screen (SPEC §18.4). Choose Battle or Trade, pair with a
// friend's CYD over ESP-NOW, then either run the SAME deterministic match on both
// screens (Battle) or drop the friend's bot into your library (Trade, Pokemon-style).
// HARDWARE-PENDING: needs two boards to verify; compiles clean.
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"
#include "game/Maze.h"
#include "game/Arena.h"
#include "game/Profile.h"

namespace screens {

class RadioScreen : public app::IScreen {
 public:
  void begin(gb::Profile* profile);
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;

 private:
  enum class Phase : uint8_t { CHOICE, LINKING, TRADED, BATTLE, DONE };
  void drawChoice();
  void drawLinking();
  void startLink(bool trade);
  void onReady();
  void drawBoard();
  void mazeGeometry(int& tile, int& ox, int& oy);
  void drawCell(int r, int c);
  void drawBot(int i, const gb::Pose& p, int avatar);

  gb::Profile* _profile = nullptr;
  bool _trade = false;
  Phase _phase = Phase::CHOICE;

  gb::Program _mine, _theirs;
  gb::Maze _maze;
  gb::Pose _s0, _s1;
  gb::Arena _arena;
  int _myAvatar = 0, _theirAvatar = 5;
  uint32_t _last = 0;
  app::TapDetector _tap;
};

}  // namespace screens
