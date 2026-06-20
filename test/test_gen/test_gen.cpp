// Phase 2 tests: the generator's core safety property (always solvable across the
// difficulty curve), determinism (same seed -> identical maze), and star math.
#include <unity.h>
#include <cstdio>
#include "game/Maze.h"
#include "game/MazeGen.h"
#include "game/Score.h"
#include "game/Program.h"
#include "game/Interpreter.h"

using namespace gb;

void setUp() {}
void tearDown() {}

static bool sameMaze(const Maze& a, const Maze& b) {
  if (a.rows() != b.rows() || a.cols() != b.cols()) return false;
  if (!(a.startPose() == b.startPose())) return false;
  if (a.goalRow() != b.goalRow() || a.goalCol() != b.goalCol()) return false;
  for (int r = 0; r < a.rows(); r++)
    for (int c = 0; c < a.cols(); c++)
      if (a.at(r, c) != b.at(r, c)) return false;
  return true;
}

// THE safety property: hundreds of mazes across the curve must be BFS-solvable.
void test_always_solvable_across_curve() {
  int checked = 0;
  for (uint32_t seedBase = 1; seedBase <= 40; seedBase++) {
    for (int level = 1; level <= 70; level++) {
      Maze m;
      MazeGen::generate(m, seedBase, level);
      Difficulty d = difficultyFor(level);
      int sol = shortestSolutionLen(m, d.allowPitGaps);
      if (sol <= 0) {
        char msg[64];
        snprintf(msg, sizeof(msg), "unsolvable seedBase=%u level=%d", seedBase, level);
        TEST_FAIL_MESSAGE(msg);
      }
      checked++;
    }
  }
  TEST_ASSERT_TRUE(checked >= 2800);
}

void test_deterministic_same_seed() {
  for (int level = 1; level <= 60; level += 7) {
    Maze a, b;
    MazeGen::generate(a, 12345, level);
    MazeGen::generate(b, 12345, level);
    TEST_ASSERT_TRUE(sameMaze(a, b));
  }
}

void test_different_profiles_differ() {
  Maze a, b;
  MazeGen::generate(a, 111, 20);
  MazeGen::generate(b, 222, 20);
  // overwhelmingly likely to differ; assert at least one tile or pose differs
  TEST_ASSERT_FALSE(sameMaze(a, b));
}

void test_start_faces_open_cell() {
  // The first FORWARD from START must be legal (fairness rule, SPEC §6).
  for (uint32_t sb = 1; sb <= 20; sb++) {
    for (int level = 1; level <= 60; level += 3) {
      Maze m;
      MazeGen::generate(m, sb, level);
      Pose s = m.startPose();
      int dr, dc; facingDelta(s.facing, dr, dc);
      int ar = s.row + dr, ac = s.col + dc;
      TEST_ASSERT_TRUE(m.inBounds(ar, ac));
      TEST_ASSERT_TRUE(m.isWalkable(ar, ac));
    }
  }
}

void test_grid_sizes_follow_curve() {
  Maze m;
  MazeGen::generate(m, 5, 1);
  TEST_ASSERT_EQUAL(4, m.rows());
  TEST_ASSERT_EQUAL(4, m.cols());
  MazeGen::generate(m, 5, 60);
  TEST_ASSERT_EQUAL(8, m.rows());
  TEST_ASSERT_EQUAL(10, m.cols());
}

// The solver must produce a winning program for every generated single-board level.
void test_solver_wins_across_curve() {
  int checked = 0;
  for (uint32_t sb = 1; sb <= 20; sb++) {
    for (int level = 1; level <= 21; level++) {  // single-board levels
      Maze m; MazeGen::generate(m, sb, level);
      Difficulty d = difficultyFor(level);
      Program p;
      bool solved = solveMaze(m, d.allowPitGaps, p);
      TEST_ASSERT_TRUE(solved);
      Interpreter it; it.load(&p, &m, m.startPose());
      if (it.runToEnd() != OUT_WIN) {
        char msg[64]; snprintf(msg, sizeof(msg), "solver failed sb=%u lvl=%d", sb, level);
        TEST_FAIL_MESSAGE(msg);
      }
      // solver is optimal -> matches the par length
      TEST_ASSERT_EQUAL(shortestSolutionLen(m, d.allowPitGaps), (int)p.main.size());
      checked++;
    }
  }
  TEST_ASSERT_TRUE(checked >= 400);
}

// STAR gems are an OPTIONAL bonus: they must be walkable, never on start/goal, always
// have a walkable orthogonal neighbour (so the robot can step onto them), and must not
// break solvability. They should also actually appear somewhere across the sweep.
void test_gems_reachable_and_optional() {
  int totalGems = 0;
  const int dR[4] = {-1, 1, 0, 0}, dC[4] = {0, 0, -1, 1};
  for (uint32_t sb = 1; sb <= 25; sb++) {
    for (int level = 1; level <= 60; level += 2) {
      Maze m; MazeGen::generate(m, sb, level);
      for (int r = 0; r < m.rows(); r++)
        for (int c = 0; c < m.cols(); c++) {
          if (m.at(r, c) != STAR) continue;
          totalGems++;
          TEST_ASSERT_TRUE(m.isWalkable(r, c));            // can be stepped on
          TEST_ASSERT_FALSE(m.isGoal(r, c));               // not the goal
          Pose s = m.startPose();
          TEST_ASSERT_FALSE(r == s.row && c == s.col);     // not the start
          bool neigh = false;                              // reachable from an adjacent tile
          for (int k = 0; k < 4; k++)
            if (m.isWalkable(r + dR[k], c + dC[k])) neigh = true;
          TEST_ASSERT_TRUE(neigh);
        }
      // solvability is unaffected (gems are walkable, off the required path)
      TEST_ASSERT_TRUE(shortestSolutionLen(m, difficultyFor(level).allowPitGaps) > 0);
    }
  }
  TEST_ASSERT_TRUE(totalGems > 0);  // the feature actually produces gems
}

void test_star_math() {
  TEST_ASSERT_EQUAL(3, starsFor(5, 5));    // == par
  TEST_ASSERT_EQUAL(3, starsFor(4, 5));    // under par
  TEST_ASSERT_EQUAL(2, starsFor(7, 5));    // <= 1.5*par (7.5)
  TEST_ASSERT_EQUAL(1, starsFor(9, 5));    // over
  TEST_ASSERT_EQUAL(1, starsFor(100, 0));  // degenerate par
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_always_solvable_across_curve);
  RUN_TEST(test_deterministic_same_seed);
  RUN_TEST(test_different_profiles_differ);
  RUN_TEST(test_start_faces_open_cell);
  RUN_TEST(test_grid_sizes_follow_curve);
  RUN_TEST(test_solver_wins_across_curve);
  RUN_TEST(test_gems_reachable_and_optional);
  RUN_TEST(test_star_math);
  return UNITY_END();
}
