#include "game/Gauntlet.h"
#include "game/Maze.h"
#include "game/Program.h"
#include "game/Interpreter.h"
#include "game/Sensors.h"
#include "game/Reactive.h"

namespace gb {
namespace {

// --- tiny deterministic RNG ---------------------------------------------------
struct Rng {
  uint32_t s;
  explicit Rng(uint32_t seed) : s(seed ? seed : 0x9E3779B9u) {}
  uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
  int range(int n) { return (int)(next() % (uint32_t)n); }
};

bool reactiveSolves(const Maze& m) {
  Pose p = m.startPose();
  if (m.isGoal(p.row, p.col)) return true;
  for (int s = 0; s < 200; s++) {
    Outcome o = reactiveApply(m, p, reactiveActionTo(m, p, m.goalRow(), m.goalCol()));
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
        Cmd c = reactiveActionTo(m, p, m.goalRow(), m.goalCol());
        float t[NET_MAX_OUT] = {0}; t[reactiveCmdToAction(c)] = 1.0f;
        brain.trainStep(sv, t);             // backprop one teacher example
        if (reactiveApply(m, p, c) != OUT_OK) break;
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
