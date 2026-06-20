// GridBot — Arena (SPEC §18). Pre-match menu (Race/Sumo + opponent), then an
// animated deterministic match. Opponents: House AI, Hotseat 2P (lock-in/handoff),
// and Radio (ESP-NOW, hardware-pending). Authoring-in-code-view -> BACKLOG; bots are
// picked from built-ins + the player's library.
#pragma once
#include <vector>
#include <string>
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
  void debugStep();   // capture aid: advance the match one tick, paused

 private:
  enum class Phase : uint8_t { MENU, PICK1, HANDOFF, PICK2, BOARD, DONE };
  struct Candidate { std::string name; gb::Program prog; uint8_t avatar; std::string style; bool house; bool smart; bool neuro = false; };

  void buildCandidates();
  void drawMenu();
  void drawPick(int player);
  ui::Rect pickRowRect(int i) const;
  int pickVisible() const;
  bool pickScrollTap(int x, int y);  // handle a tap in the scrollbar zone
  int houseBotIndex(const char* name) const;
  void drawHandoff();
  void startMatch();
  void drawBoard();
  void mazeGeometry(int& tile, int& ox, int& oy);
  void drawCell(int r, int c);
  void drawBot(int i, const gb::Pose& p, int avatar);
  void finishOverlay();

  gb::Profile* _profile = nullptr;
  std::vector<Candidate> _cands;
  gb::MatchType _type = gb::MatchType::RACE;
  bool _hotseat = false;
  int _pick0 = -1, _pick1 = -1;
  int _pickScroll = 0;  // first visible candidate row in the (scrollable) pick list

  gb::Maze _maze;
  gb::Pose _s0, _s1;
  gb::Arena _arena;
  Phase _phase = Phase::MENU;
  bool _running = false;
  uint32_t _last = 0;
  app::TapDetector _tap;
};

}  // namespace screens
