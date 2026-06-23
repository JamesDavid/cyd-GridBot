// GridBot — the execution engine (SPEC §8.3). An explicit interpreter with its own
// call stack (NOT C++ recursion), so it's steppable and bounded. step() advances
// exactly one primitive command and returns an outcome; the UI calls it on a timer
// to animate. The same engine drives one campaign character or N arena bots.
#pragma once
#include <vector>
#include "game/Maze.h"
#include "game/Program.h"

namespace gb {

constexpr int DEFAULT_STEP_CAP = 500;  // SPEC §8.3: bounds REPEAT_UNTIL spins
constexpr int MAX_FRAME_DEPTH  = 64;   // bounds call-stack growth (recursive F1/F2) — guards heap

// Optional view of an opponent, for arena ENEMY_* conditions (SPEC §18.3). In the
// campaign this is null and ENEMY_* always read false.
struct EnemyView {
  const Pose* pose = nullptr;
  int nearDist = 3;
};

class Interpreter {
 public:
  void load(const Program* prog, const Maze* maze, const Pose& start,
            int stepCap = DEFAULT_STEP_CAP);
  void setEnemy(const EnemyView* e) { _enemy = e; }

  // Advance one primitive command. Returns OUT_OK while running; a terminal outcome
  // (WIN/BONK/FELL/DONE_NO_WIN) sets finished().
  Outcome step();

  // Run to completion (used by tests, scoring dry-runs, and the self-test harness).
  Outcome runToEnd();

  bool finished() const { return _finished; }
  Outcome lastOutcome() const { return _last; }
  const Pose& pose() const { return _pose; }
  void setPose(const Pose& p) { _pose = p; }  // arena collision revert (SPEC §18.1)
  int primCount() const { return _prim; }
  // The node whose command was last executed — for highlighting a failing step.
  const Node* currentNode() const { return _current; }
  // The actual primitive run by the last step() — incl. the action a NEURO brain
  // chose. The arena reads this to resolve a FIRE/"zap" shove (a brain's node is N_NEURO, so
  // inspecting currentNode() can't see the move it picked). CMD_FWD when none ran.
  Cmd lastCmd() const { return _lastCmd; }

  bool evalCond(Cond c) const;  // exposed for tests/arena resolution

 private:
  enum FrameKind : uint8_t { F_SEQ, F_REPEAT, F_REPEAT_UNTIL };
  struct Frame {
    const NodeList* body = nullptr;
    size_t ip = 0;
    FrameKind kind = F_SEQ;
    uint8_t repsLeft = 0;
    Cond cond = WALL_AHEAD;
  };

  void push(FrameKind k, const NodeList* body, uint8_t reps = 0, Cond cond = WALL_AHEAD);
  Outcome execCmd(Cmd cmd);
  Outcome move(int dr, int dc);  // facing unchanged
  Outcome jump();
  void finish(Outcome o) { _finished = true; _last = o; }

  const Program* _prog = nullptr;
  const Maze* _maze = nullptr;
  const EnemyView* _enemy = nullptr;
  std::vector<Frame> _stack;
  Pose _pose;
  int _stepCap = DEFAULT_STEP_CAP;
  int _prim = 0;
  bool _finished = false;
  Outcome _last = OUT_OK;
  const Node* _current = nullptr;
  Cmd _lastCmd = CMD_FWD;  // effective primitive of the last step() (see lastCmd())

  // Pilot mode (N_NEURO with pilot=true): a one-time planned route the brain follows.
  uint8_t _wp[40];
  int _wpN = 0, _wpIdx = 0;
  bool _wpPlanned = false;
};

}  // namespace gb
