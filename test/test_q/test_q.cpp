// NeuroBot tests: tabular Q-learning. The teaching claim is "try, fail, get reward,
// update — and a policy emerges with no code and no teacher." So: after enough
// episodes the greedy policy must SOLVE the maze, value must be higher nearer the
// goal (it "spreads back from the battery"), and runs are deterministic per seed.
#include <unity.h>
#include "game/Maze.h"
#include "game/QLearn.h"

using namespace gb;

void setUp() {}
void tearDown() {}

static Maze corridorMaze() {
  Maze m; m.reset(4, 4); m.fill(FLOOR);
  Pose s; s.row = 3; s.col = 0; s.facing = EAST;
  m.setStart(s); m.set(3, 0, START); m.setGoal(0, 3);
  return m;
}

void test_q_learns_to_solve() {
  Maze m = corridorMaze();
  QLearn q; q.init(&m, 7);
  TEST_ASSERT_FALSE(q.greedySolves());          // untrained: doesn't solve
  for (int e = 0; e < 3000; e++) q.runEpisode();
  TEST_ASSERT_TRUE(q.greedySolves());           // learned a policy to the battery
}

void test_q_value_spreads_from_goal() {
  Maze m = corridorMaze();
  QLearn q; q.init(&m, 3);
  for (int e = 0; e < 3000; e++) q.runEpisode();
  // a cell next to the goal should be worth more than the far start cell
  float nearGoal = q.maxQ(0, 2);
  float atStart  = q.maxQ(3, 0);
  TEST_ASSERT_TRUE(nearGoal > atStart);
}

void test_q_deterministic() {
  Maze m = corridorMaze();
  QLearn a; a.init(&m, 42);
  QLearn b; b.init(&m, 42);
  for (int e = 0; e < 500; e++) { a.runEpisode(); b.runEpisode(); }
  for (int s = 0; s < 16; s++)
    for (int act = 0; act < 4; act++) TEST_ASSERT_EQUAL_FLOAT(a.Q[s][act], b.Q[s][act]);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_q_learns_to_solve);
  RUN_TEST(test_q_value_spreads_from_goal);
  RUN_TEST(test_q_deterministic);
  return UNITY_END();
}
