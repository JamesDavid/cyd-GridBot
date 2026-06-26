// Host harness: how do HAND-CODED (block-editor) bots stack up against TRAINED (neural) ones in
// each discipline -- maze, battle, soccer -- on the real Arena engine? Backs the hand-coding guide.
// Host-only; not part of the firmware build.
#include <cstdio>
#include "game/Arena.h"
#include "game/Program.h"
#include "game/Bots.h"
#include "game/MazeGen.h"
#include "game/Distill.h"
#include "game/Evolve.h"
#include "game/Types.h"

using namespace gb;

static Node ifDo(Cond c, Cmd cmd) { Node n = Node::ifCond(c); n.body.push_back(Node::command(cmd)); return n; }

// A neural fighter: NEURO node inside a repeatUntil loop so the brain acts EVERY tick.
static Program neuroFighter(const Net& brain) {
  Program p; p.addBrain(0); p.brains[0] = brain;
  Node loop = Node::repeatUntil(AT_GOAL); loop.body.push_back(Node::neuro(0));
  p.main.push_back(loop);
  return p;
}

// ---------------- SOCCER ----------------
static Program socChaser() {                      // naive: face ball, drive
  Program p; Node loop = Node::repeatUntil(AT_GOAL);
  loop.body.push_back(ifDo(BALL_LEFT, CMD_TURN_L));
  loop.body.push_back(ifDo(BALL_RIGHT, CMD_TURN_R));
  loop.body.push_back(Node::command(CMD_FWD));
  p.main.push_back(loop); return p;
}
static Program socBehind() {                      // circle behind the ball, then push
  Program p; Node loop = Node::repeatUntil(AT_GOAL);
  Node onBall = Node::ifCond(BALL_AHEAD);
  onBall.body.push_back(ifDo(NET_LEFT, CMD_TURN_R));
  onBall.body.push_back(ifDo(NET_RIGHT, CMD_TURN_L));
  loop.body.push_back(onBall);
  loop.body.push_back(ifDo(BALL_LEFT, CMD_TURN_L));
  loop.body.push_back(ifDo(BALL_RIGHT, CMD_TURN_R));
  loop.body.push_back(Node::command(CMD_FWD));
  p.main.push_back(loop); return p;
}
static Program socCommit() {                      // behind + commit (double-push when net is ahead)
  Program p; Node loop = Node::repeatUntil(AT_GOAL);
  Node onBall = Node::ifCond(BALL_AHEAD);
  onBall.body.push_back(ifDo(NET_LEFT, CMD_TURN_R));
  onBall.body.push_back(ifDo(NET_RIGHT, CMD_TURN_L));
  onBall.body.push_back(Node::command(CMD_FWD));   // first push
  loop.body.push_back(onBall);
  loop.body.push_back(ifDo(BALL_LEFT, CMD_TURN_L));
  loop.body.push_back(ifDo(BALL_RIGHT, CMD_TURN_R));
  loop.body.push_back(Node::command(CMD_FWD));
  p.main.push_back(loop); return p;
}
static Program socBehindWall() {                  // "behind" + don't jam against the pitch/goal walls
  Program p; Node loop = Node::repeatUntil(AT_GOAL);
  Node onBall = Node::ifCond(BALL_AHEAD);
  onBall.body.push_back(ifDo(NET_LEFT, CMD_TURN_R));
  onBall.body.push_back(ifDo(NET_RIGHT, CMD_TURN_L));
  loop.body.push_back(onBall);
  loop.body.push_back(ifDo(BALL_LEFT, CMD_TURN_L));
  loop.body.push_back(ifDo(BALL_RIGHT, CMD_TURN_R));
  loop.body.push_back(ifDo(WALL_AHEAD, CMD_TURN_R));   // unstick from a wall (ball is never on a wall)
  loop.body.push_back(Node::command(CMD_FWD));
  p.main.push_back(loop); return p;
}

static Program distilledStriker(uint32_t seed, int epochs = 5000) {
  Net b; b.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, seed); distillSoccer(b, seed, epochs);
  return neuroFighter(b);
}
static Program evolvedStriker(uint32_t seed, const Program& spar, int gens) {
  Net t; t.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, seed); distillSoccer(t, seed, 5000);
  Evolve e; e.init(SENSOR_COUNT_FOR_BRAIN, 8, 5, seed); e.seedFrom(t, 0.2f);
  for (int g = 0; g < gens; g++) {
    uint32_t s = seed + 31u * (uint32_t)g; Maze m; Pose s0, s1, ball, g0, g1;
    MazeGen::generateSoccerPitch(m, s, s0, s1, ball, g0, g1);
    e.evaluateArena(m, s0, s1, spar, 200, MatchType::SOCCER, &ball, &g0, &g1); e.breed();
  }
  return neuroFighter(e.best());
}

static void playSoccer(const char* name, const Program& mine, const Program& opp, int seeds) {
  int w = 0, d = 0, l = 0, gf = 0, ga = 0;
  for (int s = 0; s < seeds; s++) {
    uint32_t seed = 1000u + 137u * (uint32_t)s; Maze m; Pose s0, s1, ball, g0, g1;
    MazeGen::generateSoccerPitch(m, seed, s0, s1, ball, g0, g1);
    Arena ar; ar.setup(&m, &mine, &opp, s0, s1, MatchType::SOCCER); ar.configSoccer(ball, g0, g1); ar.run();
    int a = ar.goals(0), b = ar.goals(1); gf += a; ga += b;
    if (a > b) w++; else if (a < b) l++; else d++;
  }
  printf("  %-12s W-D-L %2d-%2d-%2d  goals %d-%d\n", name, w, d, l, gf, ga);
}

// ---------------- MAZE ----------------
static uint32_t mazeSeed(int s) { return 500u + 7u * (uint32_t)s; }
static int mazeLevel(int s) { return 5 + (s % 6); }   // a spread of campaign sizes (5..10)

// solo: does the bot REACH the goal within the cap? returns ticks-to-goal, or -1 if it failed.
static int solveTicks(const Program& mine, const Maze& m) {
  Program idle; Pose s = m.startPose(), s1 = s;
  Arena ar; ar.setup(&m, &mine, &idle, s, s1, MatchType::RACE, 4000);  // generous cap: measure if it CAN solve
  ar.run();
  return (ar.outcome() == ArenaOutcome::BOT0) ? ar.ticks() : -1;
}
// The complete hand-coded maze solver: wall-follower that JUMPS pits (the functions-lesson bot).
//   REPEAT_UNTIL AT_GOAL { IF WALL_AHEAD TURN_LEFT; IF PIT_AHEAD JUMP; FORWARD }
static Program wallFollowerJump() {
  Program p; Node loop = Node::repeatUntil(AT_GOAL);
  loop.body.push_back(ifDo(WALL_AHEAD, CMD_TURN_L));
  loop.body.push_back(ifDo(PIT_AHEAD, CMD_JUMP));
  loop.body.push_back(Node::command(CMD_FWD));
  p.main.push_back(loop); return p;
}
static int countSolved(const Program& pr, int n) {
  int solved = 0;
  for (int s = 1; s <= n; s++) { Maze m; MazeGen::generate(m, mazeSeed(s), mazeLevel(s));
    if (solveTicks(pr, m) >= 0) solved++; }
  return solved;
}
static void mazeReport(int n) {
  Maze trainMaze; MazeGen::generate(trainMaze, mazeSeed(0), mazeLevel(0));
  Net brain; brain.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 7); distillSolver(brain, trainMaze, true, 1500);
  Program brainProg = neuroFighter(brain);
  printf("  plain wall-follower (no pit handling): solved %2d/%d unseen mazes\n",
         countSolved(wallFollowerProgram(), n), n);
  printf("  wall-follower + JUMP pits (hand-coded): solved %2d/%d unseen mazes\n",
         countSolved(wallFollowerJump(), n), n);
  printf("  brain that LEARNED one maze           : solved %2d/%d unseen mazes\n",
         countSolved(brainProg, n), n);
}

// ---------------- BATTLE (Sumo) ----------------
static Program distilledHunter(uint32_t seed) {
  Net b; b.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, seed); distillHunter(b, seed, 2000, true);
  return neuroFighter(b);
}
static Program evolvedFighter(uint32_t seed, const Program& spar, int gens) {
  Net t; t.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, seed); distillHunter(t, seed, 2000, true);
  Evolve e; e.init(SENSOR_COUNT_FOR_BRAIN, 8, 5, seed); e.seedFrom(t, 0.2f);
  for (int g = 0; g < gens; g++) {
    Maze m; Pose s0, s1; MazeGen::generateArena(m, seed + 17u * (uint32_t)g, s0, s1);
    e.evaluateArena(m, s0, s1, spar, 200, MatchType::SUMO); e.breed();
  }
  return neuroFighter(e.best());
}
static void playSumo(const char* name, const Program& mine, const Program& opp, int seeds) {
  int w = 0, d = 0, l = 0;
  for (int s = 0; s < seeds; s++) {
    Maze m; Pose s0, s1; MazeGen::generateArena(m, 300u + 11u * (uint32_t)s, s0, s1);
    Arena ar; ar.setup(&m, &mine, &opp, s0, s1, MatchType::SUMO); ar.run();
    ArenaOutcome o = ar.outcome();
    if (o == ArenaOutcome::BOT0) w++; else if (o == ArenaOutcome::BOT1) l++; else d++;
  }
  printf("  %-14s W-D-L %2d-%2d-%2d\n", name, w, d, l);
}

int main() {
  const int N = 16;

  printf("=================  MAZE  =================\n");
  mazeReport(N);

  printf("\n================ BATTLE ================\n");
  Program hunter = hunterProgram();
  Program dh = distilledHunter(7);
  Program ef = evolvedFighter(7, hunter, 40);
  printf("hand-coded hunter vs trained fighters (%d rings):\n", N);
  playSumo("vs distilled", hunter, dh, N);
  playSumo("vs evolved",   hunter, ef, N);

  printf("\n================ SOCCER ================\n");
  Program A = socChaser();
  Program oD5 = distilledStriker(7, 5000), oD5b = distilledStriker(19, 5000),
          oD20 = distilledStriker(7, 20000), oEvo = evolvedStriker(7, A, 40);
  struct { const char* n; Program p; } opps[] = {
    {"distill s7", oD5}, {"distill s19", oD5b}, {"distill-20k", oD20}, {"Teach->Evolve", oEvo}};
  Program socs[] = {socChaser(), socBehind(), socCommit(), socBehindWall()};
  const char* socn[] = {"chaser", "behind", "commit", "behind+wall"};
  for (auto& o : opps) {
    printf("vs %s:\n", o.n);
    for (int i = 0; i < 4; i++) playSoccer(socn[i], socs[i], o.p, N);
  }
  return 0;
}
