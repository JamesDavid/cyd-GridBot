// Own-goal audit: play ACTUAL full soccer matches and count own goals (a bot shoving the ball into
// the net it defends, now tracked by Arena::ownGoals). Answers the question: do PROPERLY soccer-
// trained strikers own-goal -- and why do the device "fighters" own-goal? (They're BATTLE-trained
// bots that don't know soccer.)
//   g++ -std=gnu++17 -O2 -DGRIDBOT_NATIVE -Isrc tools/owngoal_check.cpp $(ls src/game/*.cpp | grep -viE 'ProgramJson') -o owngoal_check.exe
#include <cstdio>
#include "game/Arena.h"
#include "game/MazeGen.h"
#include "game/Distill.h"
using namespace gb;

static Program neuroFighter(const Net& brain) {
  Program p; p.addBrain(0); p.brains[0] = brain;
  Node loop = Node::repeatUntil(AT_GOAL); loop.body.push_back(Node::neuro(0));
  p.main.push_back(loop);
  return p;
}
static Node ifDo(Cond c, Cmd cmd) { Node x = Node::ifCond(c); x.body.push_back(Node::command(cmd)); return x; }
static Program socChaser() {                      // naive: face ball, drive (pushes whatever way it faces)
  Program p; Node loop = Node::repeatUntil(AT_GOAL);
  loop.body.push_back(ifDo(BALL_LEFT, CMD_TURN_L));
  loop.body.push_back(ifDo(BALL_RIGHT, CMD_TURN_R));
  loop.body.push_back(Node::command(CMD_FWD));
  p.main.push_back(loop); return p;
}
static Program socBehind() {                      // circle behind the ball, then push (avoids own goals)
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

static void playMatches(const char* label, const Program& a, const Program& b, int matches) {
  int gf = 0, ga = 0, og0 = 0, og1 = 0;
  for (int s = 0; s < matches; s++) {
    uint32_t seed = 1000u + 137u * (uint32_t)s;
    Maze m; Pose s0, s1, ball, g0, g1;
    MazeGen::generateSoccerPitch(m, seed, s0, s1, ball, g0, g1);
    uint32_t hh = seed * 2654435761u; int rows = m.rows(), cols = m.cols();   // vary the kickoff
    ball.row = (int8_t)(1 + (int)(hh % (uint32_t)(rows - 2)));
    ball.col = (int8_t)(2 + (int)((hh >> 9) % (uint32_t)(cols - 4)));
    Arena ar; ar.setup(&m, &a, &b, s0, s1, MatchType::SOCCER, 300);
    ar.configSoccer(ball, g0, g1); ar.run();
    gf += ar.goals(0); ga += ar.goals(1);
    og0 += ar.ownGoals(0); og1 += ar.ownGoals(1);
  }
  int goals = gf + ga, og = og0 + og1;
  printf("%-44s  score %3d-%-3d  | own goals  P0=%-2d P1=%-2d  -> %d of %d goals (%d%%) were OWN GOALS\n",
         label, gf, ga, og0, og1, og, goals, goals ? (100 * og / goals) : 0);
}

int main() {
  const int M = 10;
  printf("=== Own-goal audit: %d full 300-tick soccer matches per row ===\n\n", M);

  // PROPER soccer strikers: distillSoccer imitates a "get behind the ball" expert, and the trainer
  // penalises wrong-way pushes -- so they should rarely own-goal.
  Net sa; sa.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 11); distillSoccer(sa, 11, 5000);
  Net sb; sb.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 22); distillSoccer(sb, 22, 5000);
  Program idle;   // empty program -> stands still; isolates ONE bot's solo finishing
  playMatches("Soccer-trained  vs  IDLE (solo finishing)", neuroFighter(sa), idle, M);
  playMatches("Soccer-trained  vs  Soccer-trained", neuroFighter(sa), neuroFighter(sb), M);

  // Hand-coded baselines: socBehind is DESIGNED to get behind the ball (should own-goal little);
  // socChaser pushes whatever way it faces (should own-goal a lot). Calibrates the detector + game.
  playMatches("HandCoded behind  vs  HandCoded behind", socBehind(), socBehind(), M);
  playMatches("HandCoded chaser  vs  HandCoded chaser", socChaser(), socChaser(), M);
  playMatches("Soccer-trained    vs  HandCoded behind", neuroFighter(sa), socBehind(), M);

  // BATTLE-trained hunters fielded in SOCCER: they were trained to chase/zap a foe on the HUNT
  // senses, so in soccer they misread ball/net and shove it any which way -> lots of own goals.
  Net ha; ha.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 11); distillHunter(ha, 11, 2000, true);
  Net hb; hb.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 22); distillHunter(hb, 22, 2000, true);
  playMatches("Battle-trained  vs  Battle-trained (in soccer)", neuroFighter(ha), neuroFighter(hb), M);

  playMatches("Soccer-trained  vs  Battle-trained", neuroFighter(sa), neuroFighter(hb), M);

  return 0;
}
