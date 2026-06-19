// GridBot — the GAME screen: Maze view + Code view (control pad + program list),
// the view toggle (hold-to-peek), and timer-stepped execution (SPEC §8, §10, §11).
#pragma once
#include <vector>
#include "app/Screen.h"
#include "hal/Display.h"
#include "game/Maze.h"
#include "game/Program.h"
#include "game/Interpreter.h"
#include "game/Profile.h"

namespace screens {

class GameScreen : public app::IScreen {
 public:
  void begin(gb::Profile* profile, uint32_t level);
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;

  int lastStars() const { return _stars; }
  int lastWrittenCount() const { return _writtenCount; }

 private:
  enum View : uint8_t { V_MAZE, V_CODE };
  enum Mode : uint8_t { M_EDIT, M_RUN, M_WIN, M_FAIL };

  // flattened program-list row (rebuilt each list draw)
  struct Row { gb::Node* node; gb::NodeList* list; int index; int depth; };

  // ---- drawing ----
  void drawChrome();
  void drawMazeView(bool peek);
  void drawCodeView();
  void drawControlPad();
  void drawProgramList();
  void drawBottomBar();
  void drawCell(int r, int c);
  void drawCharacterAt(const gb::Pose& p);
  void mazeGeometry(int& tile, int& ox, int& oy);
  void toast(const char* msg, uint16_t color);

  // ---- input ----
  void handleCodeTap(int x, int y);
  void handlePadTap(int x, int y);
  void handleListTap(int x, int y);
  void appendCommand(gb::Cmd c);
  void deleteSelected();
  void startRun();
  void resetRun();
  void stepOnce(uint32_t now);
  void flatten(gb::NodeList& list, int depth, std::vector<Row>& out);

  // ---- corner growth slots (SPEC §10) ----
  // 0=JUMP,1=REPEAT,2=CALL,3=SENSE. Returns whether unlocked for this profile.
  bool cornerUnlocked(int slot) const;

  gb::Profile* _profile = nullptr;
  uint32_t _level = 1;
  gb::Maze _maze;
  gb::Program _prog;
  gb::Interpreter _it;
  gb::NodeList* _editList = nullptr;  // which body the list edits (MAIN for now)

  View _view = V_CODE;
  Mode _mode = M_EDIT;
  bool _auto = false;
  uint32_t _lastStep = 0;
  uint16_t _stepMs = 400;

  int _par = 1;
  int _writtenCount = 0;
  int _stars = 0;
  int _selected = -1;   // selected program-list row
  int _scroll = 0;

  gb::Pose _drawnPose;          // last drawn character pose (for dirty-rect)
  const gb::Node* _failNode = nullptr;

  // deferred view-toggle / hold-to-peek state
  app::TapDetector _tap;
  bool _toggleActive = false;
  bool _peeking = false;
};

}  // namespace screens
