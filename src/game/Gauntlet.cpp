#include "game/Gauntlet.h"
#include "game/Maze.h"
#include "game/Program.h"
#include "game/Interpreter.h"
#include "game/Sensors.h"

namespace gb {
namespace {

// --- tiny deterministic RNG ---------------------------------------------------
struct Rng {
  uint32_t s;
  explicit Rng(uint32_t seed) : s(seed ? seed : 0x9E3779B9u) {}
  uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
  int range(int n) { return (int)(next() % (uint32_t)n); }
};

// --- apply one primitive to a pose (mirrors Interpreter, used by the reactive teacher) -
Outcome applyCmd(const Maze& m, Pose& p, Cmd cmd) {
  int dr, dc; facingDelta(p.facing, dr, dc);
  switch (cmd) {
    case CMD_TURN_L: p.facing = turnLeft(p.facing);  return OUT_OK;
    case CMD_TURN_R: p.facing = turnRight(p.facing); return OUT_OK;
    case CMD_FWD: {
      int nr = p.row + dr, nc = p.col + dc;
      if (!m.inBounds(nr, nc) || m.at(nr, nc) == WALL) return OUT_BONK;
      if (m.at(nr, nc) == PIT) return OUT_FELL;
      p.row = (int8_t)nr; p.col = (int8_t)nc;
      return m.isGoal(nr, nc) ? OUT_WIN : OUT_OK;
    }
    case CMD_JUMP: {
      int mr = p.row + dr, mc = p.col + dc, lr = p.row + 2 * dr, lc = p.col + 2 * dc;
      if (!m.inBounds(lr, lc) || m.at(mr, mc) == WALL || m.at(lr, lc) == WALL) return OUT_BONK;
      if (m.at(lr, lc) == PIT) return OUT_FELL;
      p.row = (int8_t)lr; p.col = (int8_t)lc;
      return m.isGoal(lr, lc) ? OUT_WIN : OUT_OK;
    }
    default: return OUT_OK;
  }
}

// --- the reactive teacher: a memoryless navigator that uses ONLY sensor-equivalent info
// (straight-line goal bearing + the wall/pit immediately around it), so its decisions ARE
// a function of the brain's inputs and can be learned. Solves open/sparse mazes; concave
// traps it can't escape are simply filtered out by gauntletMaze()'s solvability check.
Cmd reactiveAction(const Maze& m, const Pose& p) {
  int fdr, fdc; facingDelta(p.facing, fdr, fdc);
  int rdr, rdc; facingDelta(turnRight(p.facing), rdr, rdc);
  int gr = m.goalRow() - p.row, gc = m.goalCol() - p.col;
  int fwd = gr * fdr + gc * fdc;       // how much the goal is straight ahead
  int rgt = gr * rdr + gc * rdc;       // how much the goal is to the right
  int ar = p.row + fdr, ac = p.col + fdc;
  bool wallAhead = !m.inBounds(ar, ac) || m.at(ar, ac) == WALL;
  bool pitAhead  = m.inBounds(ar, ac) && m.at(ar, ac) == PIT;

  if (pitAhead) {                      // jump a pit if the landing is safe and we want forward
    int lr = p.row + 2 * fdr, lc = p.col + 2 * fdc;
    if (fwd > 0 && m.inBounds(lr, lc) && m.at(lr, lc) != WALL && m.at(lr, lc) != PIT)
      return CMD_JUMP;
    return rgt >= 0 ? CMD_TURN_R : CMD_TURN_L;   // can't pass -> turn away
  }
  if (wallAhead) return rgt >= 0 ? CMD_TURN_R : CMD_TURN_L;   // steer around the wall
  if (fwd > 0) {                       // goal is ahead-ish
    if (rgt > fwd)  return CMD_TURN_R; // but strongly to a side -> face it first
    if (-rgt > fwd) return CMD_TURN_L;
    return CMD_FWD;
  }
  if (rgt != 0) return rgt > 0 ? CMD_TURN_R : CMD_TURN_L;     // goal beside/behind -> rotate
  return CMD_TURN_R;                                          // goal directly behind
}

int cmdToAction(Cmd c) {
  switch (c) { case CMD_FWD: return 0; case CMD_TURN_L: return 1;
               case CMD_TURN_R: return 2; case CMD_JUMP: return 3; default: return 0; }
}

bool reactiveSolves(const Maze& m) {
  Pose p = m.startPose();
  if (m.isGoal(p.row, p.col)) return true;
  for (int s = 0; s < 200; s++) {
    Outcome o = applyCmd(m, p, reactiveAction(m, p));
    if (o == OUT_WIN) return true;
    if (o == OUT_BONK || o == OUT_FELL) return false;
  }
  return false;
}

// A sparse open maze: opposite-corner start/goal with a few scattered walls/pits.
// `obstDiv` controls density (cells/obstDiv obstacles) — higher = easier.
void genCandidate(Maze& out, uint32_t seed, int obstDiv) {
  Rng rng(seed);
  int rows = 5 + rng.range(2);   // 5..6
  int cols = 6 + rng.range(3);   // 6..8
  out.reset(rows, cols);
  out.fill(FLOOR);
  int sr = rows - 1, sc = 0, gr = 0, gc = cols - 1;
  Pose s; s.row = (int8_t)sr; s.col = (int8_t)sc; s.facing = (Facing)rng.range(4);
  out.setStart(s); out.set(sr, sc, START);
  out.setGoal(gr, gc);
  int nObs = (rows * cols) / obstDiv;
  for (int k = 0; k < nObs; k++) {
    int r = rng.range(rows), c = rng.range(cols);
    if ((r == sr && c == sc) || (r == gr && c == gc)) continue;
    out.set(r, c, rng.range(4) == 0 ? PIT : WALL);
  }
  out.set(sr, sc, START); out.setGoal(gr, gc);   // keep endpoints clear
}

uint32_t mix(uint32_t a, int idx, int t) {
  return (a ^ ((uint32_t)idx * 2654435761u) ^ ((uint32_t)t * 40503u)) + 0x9E3779B9u;
}

}  // namespace

bool gauntletMaze(Maze& out, int idx, bool train) {
  uint32_t base = train ? 0x6A7F1234u : 0x1B3D5779u;   // disjoint train/test maze spaces
  int obstDiv = train ? 5 : 9;                          // test mazes are sparser (winnable bar)
  for (int t = 0; t < 400; t++) {
    genCandidate(out, mix(base, idx, t), obstDiv);
    if (reactiveSolves(out)) return true;
  }
  // Fallback (essentially never): an empty open room is always greedy-solvable.
  out.reset(5, 6); out.fill(FLOOR);
  Pose s; s.row = 4; s.col = 0; s.facing = EAST;
  out.setStart(s); out.set(4, 0, START); out.setGoal(0, 5);
  return true;
}

void gauntletTrain(Net& brain, int epochs) {
  const int kTrain = 24;
  for (int e = 0; e < epochs; e++)
    for (int i = 0; i < kTrain; i++) {
      Maze m; gauntletMaze(m, i, true);
      Pose p = m.startPose();
      for (int s = 0; s < 200; s++) {
        if (m.isGoal(p.row, p.col)) break;
        float sv[SENSOR_COUNT]; senseEgo(m, p, nullptr, sv);
        Cmd c = reactiveAction(m, p);
        float t[NET_MAX_OUT] = {0}; t[cmdToAction(c)] = 1.0f;
        brain.trainStep(sv, t);             // backprop one teacher example
        if (applyCmd(m, p, c) != OUT_OK) break;
      }
    }
}

int gauntletRun(const Net& brain) {
  int cleared = 0;
  for (int i = 0; i < GAUNTLET_MAZES; i++) {
    Maze m; gauntletMaze(m, i, false);      // held-out test maze
    Program prog; prog.brains.push_back(brain);
    Node loop = Node::repeatUntil(AT_GOAL);
    loop.body.push_back(Node::neuro(0));
    prog.main.push_back(loop);
    Interpreter it; it.load(&prog, &m, m.startPose(), 300);
    if (it.runToEnd() == OUT_WIN) cleared++;
  }
  return cleared;
}

}  // namespace gb
