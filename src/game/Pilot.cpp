#include "game/Pilot.h"
#include "game/Maze.h"
#include "game/MazeGen.h"
#include "game/Program.h"
#include "game/Interpreter.h"
#include "game/Sensors.h"
#include "game/Reactive.h"
#include "game/Score.h"

namespace gb {

int planWaypoints(const Maze& m, const Pose& start, uint8_t* wp, int maxN) {
  Program sol;
  if (!solveMazeFrom(m, start, true, sol)) return 0;   // no route -> no plan

  // Walk the optimal route and record the sequence of tiles it visits (dedup turns,
  // which don't change the tile).
  uint8_t path[MAX_WAYPOINTS * 4];
  int pn = 0;
  const int cols = m.cols();
  auto tileOf = [&](const Pose& p) { return (uint8_t)(p.row * cols + p.col); };
  Interpreter it; it.load(&sol, &m, start, 500);
  path[pn++] = tileOf(start);
  for (int s = 0; s < MAX_WAYPOINTS * 4 - 1 && !it.finished(); s++) {
    it.step();
    uint8_t t = tileOf(it.pose());
    if (pn == 0 || path[pn - 1] != t) path[pn++] = t;
    if (pn >= MAX_WAYPOINTS * 4) break;
  }
  if (pn <= 1) return 0;   // already at the goal

  // Keep only corners: a tile where the travel direction changes. The goal (last tile)
  // is always a waypoint. (A jump keeps the same direction, so pits don't make corners.)
  int n = 0;
  for (int i = 1; i < pn - 1 && n < maxN; i++) {
    int pr = path[i - 1] / cols, pc = path[i - 1] % cols;
    int cr = path[i] / cols,     cc = path[i] % cols;
    int nr = path[i + 1] / cols, nc = path[i + 1] % cols;
    int din = (cr - pr != 0) ? 0 : 1;   // 0 = moving vertically, 1 = horizontally
    int dout = (nr - cr != 0) ? 0 : 1;
    bool dirChange = (din != dout) ||
                     ((cr - pr) * (nr - cr) < 0) || ((cc - pc) * (nc - cc) < 0);  // reversal
    if (dirChange) wp[n++] = path[i];
  }
  if (n < maxN) wp[n++] = path[pn - 1];   // the goal is the final waypoint
  return n;
}

void pilotTrain(Net& brain, uint32_t seedBase, int trainLevels, int epochs) {
  for (int e = 0; e < epochs; e++)
    for (int lvl = 1; lvl <= trainLevels; lvl++) {
      Maze boards[MAX_BOARDS];
      int nb = MazeGen::generateBoards(boards, MAX_BOARDS, seedBase, lvl);
      for (int b = 0; b < nb; b++) {
        const Maze& m = boards[b];
        uint8_t wp[MAX_WAYPOINTS]; int wn = planWaypoints(m, m.startPose(), wp, MAX_WAYPOINTS);
        if (wn == 0) continue;
        const int cols = m.cols();
        Pose p = m.startPose(); int wi = 0;
        for (int s = 0; s < 200; s++) {
          if (m.isGoal(p.row, p.col)) break;
          int cur = p.row * cols + p.col;
          while (wi < wn && wp[wi] == cur) wi++;
          int tr = m.goalRow(), tc = m.goalCol();
          if (wi < wn) { tr = wp[wi] / cols; tc = wp[wi] % cols; }
          float sv[SENSOR_COUNT]; senseEgoTo(m, p, nullptr, tr, tc, sv);  // sense toward waypoint
          Cmd c = reactiveActionTo(m, p, tr, tc);                         // teacher steers there
          float t[NET_MAX_OUT] = {0}; t[reactiveCmdToAction(c)] = 1.0f;
          brain.trainStep(sv, t);
          if (reactiveApply(m, p, c) != OUT_OK) break;
        }
      }
    }
}

int pilotRun(const Net& brain, uint32_t seedBase, int upToLevel) {
  for (int lvl = 1; lvl <= upToLevel; lvl++) {
    Maze boards[MAX_BOARDS];
    int nb = MazeGen::generateBoards(boards, MAX_BOARDS, seedBase, lvl);
    for (int b = 0; b < nb; b++) {
      Program prog; prog.brains.push_back(brain);
      Node loop = Node::repeatUntil(AT_GOAL);
      loop.body.push_back(Node::pilotBrain(0));     // planner waypoints + the brain steers
      prog.main.push_back(loop);
      Interpreter it; it.load(&prog, &boards[b], boards[b].startPose(), 500);
      if (it.runToEnd() != OUT_WIN) return lvl - 1;
    }
  }
  return upToLevel;
}

}  // namespace gb
