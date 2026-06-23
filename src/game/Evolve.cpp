#include "game/Evolve.h"
#include "game/Program.h"
#include "game/Score.h"   // distanceToGoal
#include "game/Arena.h"

namespace gb {

static inline uint32_t lcg(uint32_t& s) { s = s * 1664525u + 1013904223u; return s; }
static inline float uni(uint32_t& s) { return (int)((lcg(s) >> 8) % 2000) / 1000.0f - 1.0f; }

float scoreBrain(const Net& brain, const Maze& m, const EnemyView* enemy, int maxSteps) {
  // Drive the brain via a one-node program so it uses the real (tested) N_NEURO path.
  Program prog;
  prog.brains.push_back(brain);
  Node loop = Node::repeatUntil(AT_GOAL);
  loop.body.push_back(Node::neuro(0));
  prog.main.push_back(loop);

  Pose start = m.startPose();
  int startDist = distanceToGoal(m, start.row, start.col);
  Interpreter it;
  it.setEnemy(enemy);
  it.load(&prog, &m, start, maxSteps);
  Outcome o = it.runToEnd();

  int endDist = distanceToGoal(m, it.pose().row, it.pose().col);
  if (endDist < 0) endDist = startDist;            // fell off the path: no progress credit
  float f = (float)(startDist - endDist);          // closer is better (the gradient)
  if (o == OUT_WIN) f += 50.0f;                    // reached the battery
  if (o == OUT_FELL || o == OUT_BONK) f -= 5.0f;   // crashed
  f -= it.primCount() * 0.05f;                     // be efficient
  return f;
}

float scoreFighter(const Net& brain, const Maze& m, const Pose& s0, const Pose& s1,
                   const Program& ai, int maxSteps, MatchType type) {
  Program bp;
  bp.brains.push_back(brain);
  Node loop = Node::repeatUntil(AT_GOAL);
  loop.body.push_back(Node::neuro(0));
  bp.main.push_back(loop);

  Arena ar;
  ar.setup(&m, &bp, &ai, s0, s1, type, maxSteps);
  ar.run();
  ArenaOutcome o = ar.outcome();

  if (type == MatchType::SUMO) {
    // No goal in Sumo: reward knocking the enemy out, surviving, and HUNTING (ending close
    // to the enemy) so evolution discovers facing + zapping rather than wandering.
    float f = 0.0f;
    if (o == ArenaOutcome::BOT0) f += 100.0f;        // shoved the enemy out -> win
    else if (o == ArenaOutcome::BOT1) f -= 40.0f;    // got knocked out
    if (ar.alive(0)) f += 15.0f;                     // still standing
    int dr = ar.pose(0).row - ar.pose(1).row, dc = ar.pose(0).col - ar.pose(1).col;
    int dEnemy = (dr < 0 ? -dr : dr) + (dc < 0 ? -dc : dc);
    f -= (float)dEnemy * 2.0f;                        // close the distance (hunt)
    return f;
  }

  int startDist = distanceToGoal(m, s0.row, s0.col);
  int endDist = distanceToGoal(m, ar.pose(0).row, ar.pose(0).col);
  if (endDist < 0) endDist = startDist;
  float f = (float)(startDist - endDist);           // progress toward the goal
  if (o == ArenaOutcome::BOT0) f += 100.0f;          // beat the AI to the goal
  else if (o == ArenaOutcome::BOT1) f -= 20.0f;      // lost
  if (ar.alive(0)) f += 5.0f;                        // survived (didn't fall/get shoved)
  return f;
}

void Evolve::evaluateArena(const Maze& m, const Pose& s0, const Pose& s1,
                           const Program& ai, int maxSteps, MatchType type) {
  for (int i = 0; i < EVO_POP; i++) fit[i] = scoreFighter(pop[i], m, s0, s1, ai, maxSteps, type);
}

void Evolve::init(int in, int hid, int out, uint32_t seed) {
  nIn = in; nHid = hid; nOut = out;
  rng = seed ? seed : 1u;
  gen = 0;
  for (int i = 0; i < EVO_POP; i++) { pop[i].config(in, hid, out, rng); lcg(rng); fit[i] = 0; }
}

void Evolve::evaluate(const Maze& m, const EnemyView* enemy, int maxSteps) {
  for (int i = 0; i < EVO_POP; i++) fit[i] = scoreBrain(pop[i], m, enemy, maxSteps);
}

int Evolve::bestIdx() const {
  int b = 0;
  for (int i = 1; i < EVO_POP; i++) if (fit[i] > fit[b]) b = i;
  return b;
}

static void mutate(Net& n, uint32_t& rng, float rate, float scale) {
  for (int j = 0; j < n.nHid; j++) {
    for (int i = 0; i < n.nIn; i++) if (((lcg(rng) >> 8) % 100) < (uint32_t)(rate * 100)) n.w1[j][i] += uni(rng) * scale;
    if (((lcg(rng) >> 8) % 100) < (uint32_t)(rate * 100)) n.b1[j] += uni(rng) * scale;
  }
  for (int k = 0; k < n.nOut; k++) {
    for (int j = 0; j < n.nHid; j++) if (((lcg(rng) >> 8) % 100) < (uint32_t)(rate * 100)) n.w2[k][j] += uni(rng) * scale;
    if (((lcg(rng) >> 8) % 100) < (uint32_t)(rate * 100)) n.b2[k] += uni(rng) * scale;
  }
}

void Evolve::breed(float rate, float scale) {
  // In place (no big stack array — must fit the ESP32's small task stack):
  // selection-sort the top EVO_KEEP survivors to the FRONT...
  for (int a = 0; a < EVO_KEEP; a++) {
    int bestj = a;
    for (int b = a + 1; b < EVO_POP; b++) if (fit[b] > fit[bestj]) bestj = b;
    if (bestj != a) {
      Net tn = pop[a]; pop[a] = pop[bestj]; pop[bestj] = tn;
      float tf = fit[a]; fit[a] = fit[bestj]; fit[bestj] = tf;
    }
  }
  // ...keep the survivors (elitism); refill the rest from mutated survivors.
  for (int i = EVO_KEEP; i < EVO_POP; i++) {
    int parent = (lcg(rng) >> 8) % EVO_KEEP;
    pop[i] = pop[parent];
    mutate(pop[i], rng, rate, scale);
  }
  gen++;
}

}  // namespace gb
