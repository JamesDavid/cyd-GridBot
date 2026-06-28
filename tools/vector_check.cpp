// Decisive test of the "did the goal vector get mixed up?" hypothesis. Train a real soccer brain,
// then put it in CLEAN, unambiguous positions and print the action it chooses + which way that moves
// the ball. If the goal vector were inverted, a bot standing BEHIND the ball facing its attack net
// would push the ball AWAY from goal (toward its own). bot0 attacks goal0 (the RIGHT/east net).
//   g++ -std=gnu++17 -O2 -DGRIDBOT_NATIVE -Isrc tools/vector_check.cpp $(ls src/game/*.cpp | grep -viE 'ProgramJson') -o vector_check.exe
#include <cstdio>
#include "game/Net.h"
#include "game/Sensors.h"
#include "game/MazeGen.h"
#include "game/Distill.h"
using namespace gb;

static const char* ACT[5] = {"forward", "turnL", "turnR", "jump", "zap"};

// What a `forward` would do to the ball if the bot is standing right behind it.
static const char* pushDir(Facing f) {
  switch (f) { case EAST: return "ball EAST  (toward goal0/right)";
               case WEST: return "ball WEST  (toward goal1/left = OWN net)";
               case NORTH: return "ball NORTH"; default: return "ball SOUTH"; }
}

static void probe(const char* label, Net& brain, const Maze& m, Pose bot, Pose ball, Pose goal) {
  Pose rival; rival.row = 0; rival.col = 0; rival.facing = SOUTH;   // parked far in a corner
  float in[SENSOR_COUNT], out[NET_MAX_OUT];
  senseSoccer(m, bot, &ball, &rival, &goal, in);
  brain.forward(in, out);
  int a = 0; for (int k = 1; k < 5; k++) if (out[k] > out[a]) a = k;
  printf("%-32s | net bearing ahead=%+.2f right=%+.2f | brain picks %-7s | a fwd here pushes %s\n",
         label, in[6], in[7], ACT[a], pushDir(bot.facing));
}

int main() {
  Net b; b.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 11); distillSoccer(b, 11, 8000);   // a house-strength striker
  Maze m; Pose s0, s1, kick, g0, g1;
  MazeGen::generateSoccerPitch(m, 1, s0, s1, kick, g0, g1);   // g0 = right net (bot0 attacks), g1 = left
  int r = m.rows() / 2;
  printf("Pitch %dx%d. bot0 ATTACKS goal0 at col %d (right). Own net = goal1 at col %d (left).\n\n",
         m.rows(), m.cols(), g0.col, g1.col);

  // 1) Bot BEHIND the ball (to its west), facing the attack net. A forward pushes the ball goalward.
  { Pose bot; bot.row=r; bot.col=4; bot.facing=EAST;  Pose ball; ball.row=r; ball.col=5;
    probe("behind ball, facing attack net", b, m, bot, ball, g0); }
  // 2) Bot on the WRONG side (east of the ball), facing its own net. A forward would own-goal.
  { Pose bot; bot.row=r; bot.col=5; bot.facing=WEST;  Pose ball; ball.row=r; ball.col=4;
    probe("wrong side, facing OWN net", b, m, bot, ball, g0); }
  // 3) Bot below the ball, facing the attack net (ball is to its... let me face EAST with ball east).
  { Pose bot; bot.row=r+1; bot.col=4; bot.facing=EAST; Pose ball; ball.row=r; ball.col=5;
    probe("below+behind, facing attack net", b, m, bot, ball, g0); }
  return 0;
}
