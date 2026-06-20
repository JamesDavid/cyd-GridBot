#include "game/Distill.h"
#include "game/Program.h"
#include "game/Interpreter.h"
#include "game/Sensors.h"
#include "game/Score.h"

namespace gb {

bool distillSolver(Net& brain, const Maze& m, bool allowJump, int epochs) {
  Program sol;
  if (!solveMaze(m, allowJump, sol)) return false;  // no solution -> nothing to teach
  for (int e = 0; e < epochs; e++) {
    Interpreter it; it.load(&sol, &m, m.startPose(), 500);
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
      t[act] = 1.0f;                       // one-hot: copy the solver's move
      brain.trainStep(s, t);               // backprop one example
    }
  }
  return true;
}

}  // namespace gb
