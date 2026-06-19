// Phase 5 showcase test: ONE general wall-follower program clears a whole generated
// multi-maze set (SPEC §7.1) — proves sensing + generalization in single asserts.
#include <unity.h>
#include <cstdio>
#include "game/Maze.h"
#include "game/MazeGen.h"
#include "game/Program.h"
#include "game/Interpreter.h"
#include "game/Bots.h"

using namespace gb;

void setUp() {}
void tearDown() {}

void test_wall_follower_clears_one_spiral() {
  Maze m;
  MazeGen::generateSpiral(m, 6, 7);
  Program wf = wallFollowerProgram();
  Interpreter it;
  it.load(&wf, &m, m.startPose(), 2000);
  TEST_ASSERT_EQUAL(OUT_WIN, it.runToEnd());
}

void test_wall_follower_clears_generated_set() {
  // For every sensing level + several profiles, the SAME program must beat ALL boards.
  Program wf = wallFollowerProgram();
  int sets = 0;
  for (uint32_t sb = 1; sb <= 10; sb++) {
    for (int level = SENSE_LEVEL; level <= SENSE_LEVEL + 12; level++) {
      Maze boards[MAX_BOARDS];
      int n = MazeGen::generateBoards(boards, MAX_BOARDS, sb, level);
      TEST_ASSERT_TRUE(n >= 2);
      for (int i = 0; i < n; i++) {
        Interpreter it;
        it.load(&wf, &boards[i], boards[i].startPose(), 2000);
        if (it.runToEnd() != OUT_WIN) {
          char msg[64];
          snprintf(msg, sizeof(msg), "follower failed sb=%u lvl=%d board=%d", sb, level, i);
          TEST_FAIL_MESSAGE(msg);
        }
      }
      sets++;
    }
  }
  TEST_ASSERT_TRUE(sets >= 100);
}

void test_normal_level_single_board() {
  Maze boards[MAX_BOARDS];
  int n = MazeGen::generateBoards(boards, MAX_BOARDS, 7, 10);
  TEST_ASSERT_EQUAL(1, n);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_wall_follower_clears_one_spiral);
  RUN_TEST(test_wall_follower_clears_generated_set);
  RUN_TEST(test_normal_level_single_board);
  return UNITY_END();
}
