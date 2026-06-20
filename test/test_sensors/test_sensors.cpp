// NeuroBot tests: egocentric sensors. The brain only generalizes if its inputs are
// relative to the robot's heading, so the key property is rotation-invariance: the
// SAME relative situation produces the SAME senses regardless of absolute facing.
#include <unity.h>
#include "game/Maze.h"
#include "game/Sensors.h"

using namespace gb;

void setUp() {}
void tearDown() {}

static Maze openMaze(int goalR, int goalC) {
  Maze m; m.reset(5, 5); m.fill(FLOOR); m.setGoal(goalR, goalC);
  return m;
}

void test_wall_ahead_is_egocentric() {
  Maze m = openMaze(4, 4);
  m.set(2, 3, WALL);
  Pose p; p.row = 2; p.col = 2; p.facing = EAST;  // wall is directly ahead (east)
  float s[SENSOR_COUNT];
  senseEgo(m, p, nullptr, s);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, s[0]);  // wall ahead
  // turn to face north: the same wall is now to the RIGHT, not ahead
  p.facing = NORTH;
  senseEgo(m, p, nullptr, s);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, s[0]);  // nothing ahead
  TEST_ASSERT_EQUAL_FLOAT(1.0f, s[2]);  // wall on the right
}

void test_goal_bearing() {
  Maze m = openMaze(0, 4);            // goal top-right
  Pose p; p.row = 0; p.col = 0;
  float s[SENSOR_COUNT];
  p.facing = EAST;                   // goal is straight ahead
  senseEgo(m, p, nullptr, s);
  TEST_ASSERT_TRUE(s[4] > 0.9f);     // aheadness ~ +1
  TEST_ASSERT_TRUE(s[5] > -0.2f && s[5] < 0.2f);  // rightness ~ 0
  p.facing = NORTH;                  // goal is now to the right
  senseEgo(m, p, nullptr, s);
  TEST_ASSERT_TRUE(s[5] > 0.9f);     // rightness ~ +1
  TEST_ASSERT_TRUE(s[4] > -0.2f && s[4] < 0.2f);  // aheadness ~ 0
}

void test_rotation_invariance() {
  // Goal straight ahead in two different absolute orientations -> identical senses.
  Maze a = openMaze(0, 2);   // goal north of centre
  Pose pa; pa.row = 2; pa.col = 2; pa.facing = NORTH;
  Maze b = openMaze(2, 4);   // goal east of centre
  Pose pb; pb.row = 2; pb.col = 2; pb.facing = EAST;
  float sa[SENSOR_COUNT], sb[SENSOR_COUNT];
  senseEgo(a, pa, nullptr, sa);
  senseEgo(b, pb, nullptr, sb);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, sa[4], sb[4]);  // same aheadness
  TEST_ASSERT_FLOAT_WITHIN(0.001f, sa[5], sb[5]);  // same rightness
  TEST_ASSERT_FLOAT_WITHIN(0.001f, sa[6], sb[6]);  // same distance
}

void test_no_enemy_is_neutral() {
  Maze m = openMaze(0, 4);
  Pose p; p.row = 2; p.col = 2; p.facing = EAST;
  float s[SENSOR_COUNT];
  senseEgo(m, p, nullptr, s);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, s[7]);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, s[8]);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, s[9]);  // "far away"
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_wall_ahead_is_egocentric);
  RUN_TEST(test_goal_bearing);
  RUN_TEST(test_rotation_invariance);
  RUN_TEST(test_no_enemy_is_neutral);
  return UNITY_END();
}
