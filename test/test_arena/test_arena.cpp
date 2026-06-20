// Phase 6 tests: the Arena match engine. THE key property is determinism —
// same seed + same two programs => byte-identical match log every run (SPEC §18.1).
#include <unity.h>
#include "game/Maze.h"
#include "game/MazeGen.h"
#include "game/Program.h"
#include "game/Arena.h"
#include "game/Bots.h"

using namespace gb;

void setUp() {}
void tearDown() {}

// A straight dash: REPEAT_UNTIL AT_GOAL { FORWARD }
static Program dashProgram() {
  Program p;
  Node loop = Node::repeatUntil(AT_GOAL);
  loop.body.push_back(Node::command(CMD_FWD));
  p.main.push_back(loop);
  return p;
}

void test_arena_deterministic_log() {
  Program a = dashProgram(), b = dashProgram();
  uint32_t h1 = 0, h2 = 0;
  for (int run = 0; run < 2; run++) {
    Maze m; Pose s0, s1;
    MazeGen::generateArena(m, 42, s0, s1);
    Arena ar;
    ar.setup(&m, &a, &b, s0, s1, MatchType::RACE);
    ar.run();
    if (run == 0) h1 = ar.logHash(); else h2 = ar.logHash();
  }
  TEST_ASSERT_EQUAL_UINT32(h1, h2);
}

void test_arena_match_plays_out() {
  // A race must PLAY OUT over several ticks (watchable), not resolve instantly.
  Program a = dashProgram(), b = dashProgram();
  Maze m; Pose s0, s1;
  MazeGen::generateArena(m, 7, s0, s1);
  Arena ar;
  ar.setup(&m, &a, &b, s0, s1, MatchType::RACE);
  ArenaOutcome o = ar.run();
  TEST_ASSERT_NOT_EQUAL((int)ArenaOutcome::RUNNING, (int)o);
  TEST_ASSERT_TRUE(ar.ticks() >= 3);  // bots travel the lane before it resolves
}

void test_arena_faster_bot_wins() {
  // A dasher vs an idler (empty program): the dasher reaches the goal and wins.
  Program fast = dashProgram();
  Program idle;  // empty -> DONE_NO_WIN immediately
  Maze m; Pose s0, s1;
  MazeGen::generateArena(m, 11, s0, s1);
  Arena ar;
  ar.setup(&m, &fast, &idle, s0, s1, MatchType::RACE);
  ArenaOutcome o = ar.run();
  TEST_ASSERT_EQUAL((int)ArenaOutcome::BOT0, (int)o);
  TEST_ASSERT_TRUE(ar.won(0));
}

void test_arena_replay_independent_of_instances() {
  // Fresh Arena instances with the same inputs must agree (no hidden state/RNG).
  Program a = wallFollowerProgram(), b = dashProgram();
  Maze m; Pose s0, s1;
  MazeGen::generateArena(m, 99, s0, s1);
  Arena a1, a2;
  a1.setup(&m, &a, &b, s0, s1, MatchType::RACE); a1.run();
  a2.setup(&m, &a, &b, s0, s1, MatchType::RACE); a2.run();
  TEST_ASSERT_EQUAL_UINT32(a1.logHash(), a2.logHash());
  TEST_ASSERT_EQUAL((int)a1.outcome(), (int)a2.outcome());
}

// Sumo: a pusher shoves an idler standing in front of a pit -> idler out, pusher wins.
void test_sumo_push_into_pit() {
  Maze m;
  m.reset(1, 4);
  m.fill(FLOOR);
  m.set(0, 3, PIT);                 // pit at the right end
  Pose s0; s0.row = 0; s0.col = 1; s0.facing = EAST;  // pusher
  Pose s1; s1.row = 0; s1.col = 2; s1.facing = WEST;  // victim, just left of the pit
  // pusher program: PUSH repeatedly
  Program pusher;
  Node loop = Node::repeatUntil(AT_GOAL);
  loop.body.push_back(Node::command(CMD_PUSH));
  pusher.main.push_back(loop);
  Program idle;  // empty -> stands still then done
  Arena ar;
  ar.setup(&m, &pusher, &idle, s0, s1, MatchType::SUMO, 50);
  ArenaOutcome o = ar.run();
  TEST_ASSERT_EQUAL((int)ArenaOutcome::BOT0, (int)o);
  TEST_ASSERT_FALSE(ar.alive(1));
}

void test_sumo_deterministic() {
  Program a = hunterProgram(), b = hunterProgram();
  Maze m; Pose s0, s1;
  MazeGen::generateArena(m, 5, s0, s1);
  Arena a1, a2;
  a1.setup(&m, &a, &b, s0, s1, MatchType::SUMO); a1.run();
  a2.setup(&m, &a, &b, s0, s1, MatchType::SUMO); a2.run();
  TEST_ASSERT_EQUAL_UINT32(a1.logHash(), a2.logHash());
}

// Regression: the arena board must fit MAZE_MAX, and both starts + the goal must be
// in bounds (a clamped cols once put bot 1 off-board, making it single-player).
void test_arena_board_in_bounds() {
  for (uint32_t s = 1; s <= 30; s++) {
    Maze m; Pose s0, s1;
    MazeGen::generateArena(m, s, s0, s1);
    TEST_ASSERT_TRUE(m.cols() <= MAZE_MAX_COLS && m.rows() <= MAZE_MAX_ROWS);
    TEST_ASSERT_TRUE(m.inBounds(s0.row, s0.col));
    TEST_ASSERT_TRUE(m.inBounds(s1.row, s1.col));
    TEST_ASSERT_TRUE(m.inBounds(m.goalRow(), m.goalCol()));
    TEST_ASSERT_TRUE(m.isWalkable(s0.row, s0.col));
    TEST_ASSERT_TRUE(m.isWalkable(s1.row, s1.col));
    // both bots can take a first step toward the centre (not walled/pitted in)
    int d0r, d0c; facingDelta(s0.facing, d0r, d0c);
    int d1r, d1c; facingDelta(s1.facing, d1r, d1c);
    TEST_ASSERT_TRUE(m.isWalkable(s0.row + d0r, s0.col + d0c));
    TEST_ASSERT_TRUE(m.isWalkable(s1.row + d1r, s1.col + d1c));
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_arena_board_in_bounds);
  RUN_TEST(test_arena_deterministic_log);
  RUN_TEST(test_arena_match_plays_out);
  RUN_TEST(test_arena_faster_bot_wins);
  RUN_TEST(test_arena_replay_independent_of_instances);
  RUN_TEST(test_sumo_push_into_pit);
  RUN_TEST(test_sumo_deterministic);
  return UNITY_END();
}
