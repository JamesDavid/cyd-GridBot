#include "game/Distill.h"
#include "game/Program.h"
#include "game/Interpreter.h"
#include "game/Sensors.h"
#include "game/Reactive.h"
#include "game/MazeGen.h"
#include "game/Score.h"

namespace gb {

// One shared episode scratch buffer, reused by rnnTrainGeneral and Brain Cam (saves DRAM).
static float gEpiX[RNET_MAX_T * SENSOR_COUNT];
static int   gEpiAct[RNET_MAX_T];
static int   gEpiT = 0;

int captureSolverShared(const Maze& m) {
  gEpiT = 0;
  Program sol;
  if (!solveMaze(m, true, sol)) return 0;
  Interpreter it; it.load(&sol, &m, m.startPose(), 500);
  int guard = 0;
  while (!it.finished() && guard++ < 400 && gEpiT < RNET_MAX_T) {
    Pose before = it.pose();
    float s[SENSOR_COUNT]; senseEgo(m, before, nullptr, s);
    it.step();
    const Node* n = it.currentNode();
    if (!n || n->type != N_CMD) continue;
    int a = -1;
    switch (n->cmd) { case CMD_FWD: a = 0; break; case CMD_TURN_L: a = 1; break;
                      case CMD_TURN_R: a = 2; break; case CMD_JUMP: a = 3; break; default: break; }
    if (a < 0) continue;
    for (int i = 0; i < SENSOR_COUNT; i++) gEpiX[gEpiT * SENSOR_COUNT + i] = s[i];
    gEpiAct[gEpiT] = a; gEpiT++;
  }
  return gEpiT;
}

float ffTrainShared(Net& brain, int passes) {
  if (gEpiT <= 0) return 0.0f;
  float L = 0;
  for (int e = 0; e < passes; e++) {
    L = 0;
    for (int t = 0; t < gEpiT; t++) {
      float tg[NET_MAX_OUT] = {0}; tg[gEpiAct[t]] = 1.0f;
      L += brain.trainStep(&gEpiX[t * SENSOR_COUNT], tg);
    }
    L /= gEpiT;
  }
  return L;
}

float rnnTrainShared(RNet& brain, int passes) {
  if (gEpiT <= 0) return 0.0f;
  float L = 0;
  for (int e = 0; e < passes; e++) L = brain.trainEpisode(gEpiX, gEpiAct, gEpiT);
  return L;
}

void rnnTrainGeneral(RNet& brain, uint32_t seedBase, int levels, int epochs) {
  for (int e = 0; e < epochs; e++)
    for (int l = 1; l <= levels; l++) {
      Maze m; MazeGen::generate(m, seedBase, l);
      // roll out the memory-using explorer, capturing (senses, action) each step
      uint8_t vis[MAZE_MAX_CELLS] = {0};
      Pose p = m.startPose(); vis[p.row * m.cols() + p.col] = 1;
      int T = 0;
      for (int s = 0; s < RNET_MAX_T; s++) {
        if (m.isGoal(p.row, p.col)) break;
        senseEgo(m, p, nullptr, gEpiX + T * SENSOR_COUNT);
        Cmd c = exploreActionTo(m, p, vis, m.goalRow(), m.goalCol());
        gEpiAct[T] = reactiveCmdToAction(c); T++;
        if (reactiveApply(m, p, c) != OUT_OK) break;
        vis[p.row * m.cols() + p.col] = 1;
      }
      if (T > 0) brain.trainEpisode(gEpiX, gEpiAct, T);
    }
}

// The shared backprop loop: run `teacher` on `m` and train `brain` to copy each primitive
// move (egocentric sensors -> one-hot action). Re-simulates each epoch (no big buffers).
static void distillTeacher(Net& brain, const Maze& m, const Program& teacher, int epochs) {
  for (int e = 0; e < epochs; e++) {
    Interpreter it; it.load(&teacher, &m, m.startPose(), 500);
    int guard = 0;
    while (!it.finished() && guard++ < 400) {
      Pose before = it.pose();
      float s[SENSOR_COUNT];
      senseEgo(m, before, nullptr, s);
      it.step();
      const Node* n = it.currentNode();
      if (!n || n->type != N_CMD) continue;
      int act = -1;
      switch (n->cmd) {
        case CMD_FWD:    act = 0; break;
        case CMD_TURN_L: act = 1; break;
        case CMD_TURN_R: act = 2; break;
        case CMD_JUMP:   act = 3; break;
        default: break;
      }
      if (act < 0) continue;
      float t[NET_MAX_OUT] = {0};
      t[act] = 1.0f;                       // one-hot: copy the teacher's move
      brain.trainStep(s, t);               // backprop one example
    }
  }
}

bool distillSolver(Net& brain, const Maze& m, bool allowJump, int epochs) {
  Program sol;
  if (!solveMaze(m, allowJump, sol)) return false;  // no solution -> nothing to teach
  distillTeacher(brain, m, sol, epochs);
  return true;
}

bool pathToProgram(const Maze& m, const uint8_t* tiles, int n, Program& out) {
  out.clear();
  if (n < 1) return false;
  const int cols = m.cols();
  Pose p = m.startPose();
  if (tiles[0] != (uint8_t)(p.row * cols + p.col)) return false;  // must start at the bot
  for (int i = 1; i < n; i++) {
    int r0 = tiles[i - 1] / cols, c0 = tiles[i - 1] % cols;
    int r1 = tiles[i] / cols,     c1 = tiles[i] % cols;
    int dr = r1 - r0, dc = c1 - c0;
    if (dr != 0 && dc != 0) return false;                 // no diagonals
    int dist = (dr ? (dr < 0 ? -dr : dr) : (dc < 0 ? -dc : dc));
    if (dist != 1 && dist != 2) return false;             // forward (1) or jump (2) only
    Facing target;
    if (dr < 0) target = NORTH; else if (dr > 0) target = SOUTH;
    else if (dc > 0) target = EAST; else target = WEST;
    int diff = ((int)target - (int)p.facing) & 3;         // minimal turns to face target
    if (diff == 1)      out.main.push_back(Node::command(CMD_TURN_R));
    else if (diff == 3) out.main.push_back(Node::command(CMD_TURN_L));
    else if (diff == 2) { out.main.push_back(Node::command(CMD_TURN_R));
                          out.main.push_back(Node::command(CMD_TURN_R)); }
    p.facing = target;
    out.main.push_back(Node::command(dist == 2 ? CMD_JUMP : CMD_FWD));
    p.row = (int8_t)r1; p.col = (int8_t)c1;
  }
  return true;
}

bool distillPath(Net& brain, const Maze& m, const uint8_t* tiles, int n, int epochs) {
  Program teacher;
  if (!pathToProgram(m, tiles, n, teacher)) return false;
  if (teacher.main.empty()) return false;   // a single-tile "path" teaches nothing
  distillTeacher(brain, m, teacher, epochs);
  return true;
}

}  // namespace gb
