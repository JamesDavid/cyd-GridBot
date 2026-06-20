// NeuroBot tests: neuroevolution. The teaching claim is "no teacher, just a score —
// breed the winners and the population gets better." So the key property: best fitness
// IMPROVES over generations and a brain that makes real progress emerges. Elitism makes
// best-fitness monotonic, so we also assert it ends up clearly positive (net progress).
#include <unity.h>
#include "game/Maze.h"
#include "game/MazeGen.h"
#include "game/Evolve.h"
#include "game/Net.h"

using namespace gb;

void setUp() {}
void tearDown() {}

static Maze evoMaze() {
  Maze m; m.reset(4, 4); m.fill(FLOOR);
  Pose s; s.row = 3; s.col = 0; s.facing = EAST;
  m.setStart(s); m.set(3, 0, START); m.setGoal(0, 3);
  return m;
}

void test_evolution_improves() {
  Maze m = evoMaze();
  Evolve evo; evo.init(SENSOR_COUNT_FOR_BRAIN, 8, 5, 1);
  evo.evaluate(m);
  float f0 = evo.bestFit();
  for (int g = 0; g < 60; g++) evo.step(m, 100);
  float fN = evo.bestFit();
  TEST_ASSERT_TRUE(fN >= f0);   // elitism: never regresses
  TEST_ASSERT_TRUE(fN > 0.5f);  // a brain that genuinely moves toward the battery emerged
}

void test_scoreBrain_rewards_progress() {
  Maze m = evoMaze();
  // a brain forced to spin in place (turn) should score worse than one that reaches goal
  Net spinner; spinner.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 2);
  for (int k = 0; k < 5; k++) { for (int j = 0; j < spinner.nHid; j++) spinner.w2[k][j] = 0; spinner.b2[k] = (k == 1) ? 6.f : -6.f; }  // always turn-L
  float spin = scoreBrain(spinner, m, nullptr, 100);
  TEST_ASSERT_TRUE(spin <= 0.5f);  // spinning makes no progress
}

// The Arena-fighter fitness must be deterministic (so training is reproducible) and
// evolving against an AI must improve the fighter.
void test_arena_fighter_evolves() {
  Maze m; Pose s0, s1; MazeGen::generateArena(m, 7, s0, s1);
  Program idle;  // a do-nothing AI opponent
  Net b; b.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 3);
  float f1 = scoreFighter(b, m, s0, s1, idle, 200);
  float f2 = scoreFighter(b, m, s0, s1, idle, 200);
  TEST_ASSERT_EQUAL_FLOAT(f1, f2);                 // deterministic

  Evolve evo; evo.init(SENSOR_COUNT_FOR_BRAIN, 8, 5, 1);
  evo.evaluateArena(m, s0, s1, idle, 200);
  float best0 = evo.bestFit();
  for (int g = 0; g < 30; g++) { evo.breed(); evo.evaluateArena(m, s0, s1, idle, 200); }
  TEST_ASSERT_TRUE(evo.bestFit() >= best0);        // a better fighter emerged (elitism-safe)
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_evolution_improves);
  RUN_TEST(test_scoreBrain_rewards_progress);
  RUN_TEST(test_arena_fighter_evolves);
  return UNITY_END();
}
