// GridBot — the GAME screen: Maze view + Code view (control pad + program list),
// the view toggle (hold-to-peek), and timer-stepped execution (SPEC §8, §10, §11).
#pragma once
#include <vector>
#include "app/Screen.h"
#include "hal/Display.h"
#include "ui/Theme.h"
#include "game/Maze.h"
#include "game/MazeGen.h"
#include "game/Program.h"
#include "game/Interpreter.h"
#include "game/Profile.h"

namespace screens {

class GameScreen : public app::IScreen {
 public:
  void begin(gb::Profile* profile, uint32_t level);
  // Shared-seed Challenge: a one-off board keyed purely by `seedCode` (so friends with
  // the same code get the identical maze), at a fixed difficulty. Wins award no campaign
  // progress/coins/stats (not farmable) — just the star rating on the overlay.
  void beginChallenge(gb::Profile* profile, uint32_t seedCode);
  bool isChallenge() const { return _challenge; }
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;

  int lastStars() const { return _stars; }
  int lastWrittenCount() const { return _writtenCount; }
  // NeuroBot: which brain index to train (the editor asked to open the neuro interface)
  int pendingNeuro() const { return _pendingNeuro; }
  void clearPendingNeuro() { _pendingNeuro = -1; }
  gb::Program& program() { return _prog; }
  gb::Maze& maze() { return _maze; }

  // capture aids: solve the level and run it paused; advance one primitive
  void beginAutoRun();
  void debugStep();

 private:
  enum View : uint8_t { V_MAZE, V_CODE };
  enum Mode : uint8_t { M_EDIT, M_RUN, M_WIN, M_FAIL };

  // flattened program-list row (rebuilt each list draw)
  // addSlot rows are synthetic "+ add inside" affordances under a block; trainSlot rows
  // are the "train this brain" line under a NEURO node (node = the brain). Neither is a
  // real program node (index = -1).
  struct Row { gb::Node* node; gb::NodeList* list; int index; int depth; uint16_t bracket; bool addSlot; bool trainSlot = false; };

  // ---- drawing ----
  void drawChrome();
  void drawMazeView(bool peek);
  void drawCodeView();
  void drawControlPad();
  void drawProgramList();
  void drawBottomBar();
  void drawCell(int r, int c);
  void drawCharacterAt(const gb::Pose& p);
  void drawCharacterPx(int cx, int cy, gb::Facing facing, int emote);  // emote: 0 none,1 happy,2 dizzy
  void redrawCellsBetween(const gb::Pose& a, const gb::Pose& b);
  void winCelebration();
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
  void settleOutcome(gb::Outcome o);
  void flatten(gb::NodeList& list, int depth, std::vector<Row>& out, uint16_t bracket = 0);
  gb::NodeList* appendTarget();    // where new blocks/commands go (selected body or edit list)
  void appendNodeToTarget(const gb::Node& n);
  ui::Rect funcTabRect(int i) const;
  int listTop() const;

  // ---- corner growth slots (SPEC §10) ----
  // 0=JUMP,1=REPEAT,2=CALL,3=SENSE. Returns whether unlocked for this profile.
  bool cornerUnlocked(int slot) const;

  void advanceBoard();        // multi-maze: load the next board, keep the program
  void saveToLibrary();
  void loadFromLibrary();

  gb::Profile* _profile = nullptr;
  uint32_t _level = 1;
  bool _challenge = false;    // shared-seed Challenge run (no campaign progress on win)
  uint32_t _challengeCode = 0;  // the seed code (shown in the chrome instead of "Lv N")
  gb::Maze _boards[gb::MAX_BOARDS];
  int _boardCount = 1;
  int _boardIdx = 0;
  gb::Maze _maze;             // active board (copy of _boards[_boardIdx])
  ui::Biome _biome;           // palette for this level band
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
  int _pendingNeuro = -1;  // brainIdx the user asked to train (-> Signal::GOTO_NEURO_TRAIN)
  int _scroll = 0;
  bool _followTail = true;  // keep the newest command in view as you add

  gb::Pose _drawnPose;          // last drawn character pose (for dirty-rect)
  bool _visited[gb::MAZE_MAX_CELLS] = {false};  // breadcrumb trail
  bool _coinTaken[gb::MAZE_MAX_CELLS] = {false};
  int _coinsThisRun = 0;
  // STAR gems: a rarer bonus collectible placed on a detour OFF the path, so you must
  // route to grab them. Grabbing every gem on a board pays a bonus on the win.
  bool _gemTaken[gb::MAZE_MAX_CELLS] = {false};
  int _gemsThisRun = 0;
  int _gemTotal = 0;          // gems on the active board
  void recountGems();         // count STAR tiles on _maze into _gemTotal
  const gb::Node* _failNode = nullptr;

  // smooth movement tween between tiles
  bool _tween = false;
  gb::Pose _tweenFrom, _tweenTo;
  uint32_t _tweenT0 = 0;
  gb::Outcome _pendingOutcome = gb::OUT_OK;

  // level-start maze preview (show the board, then auto-switch to code)
  bool _previewing = false;
  uint32_t _previewStart = 0;

  // deferred view-toggle / hold-to-peek state
  app::TapDetector _tap;
  bool _toggleActive = false;
  bool _peeking = false;
};

}  // namespace screens
