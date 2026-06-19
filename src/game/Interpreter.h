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
  int primCount() const { return _prim; }
  // The node whose command was last executed — for highlighting a failing step.
  const Node* currentNode() const { return _current; }

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
};

}  // namespace gb
