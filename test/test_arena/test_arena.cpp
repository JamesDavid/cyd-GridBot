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

void test_arena_symmetric_dash_is_draw() {
  // Two identical dashers from mirrored starts collide at the centre goal -> draw.
  Program a = dashProgram(), b = dashProgram();
  Maze m; Pose s0, s1;
  MazeGen::generateArena(m, 7, s0, s1);
  Arena ar;
  ar.setup(&m, &a, &b, s0, s1, MatchType::RACE);
  ArenaOutcome o = ar.run();
  TEST_ASSERT_EQUAL((int)ArenaOutcome::DRAW, (int)o);
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

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_arena_deterministic_log);
  RUN_TEST(test_arena_symmetric_dash_is_draw);
  RUN_TEST(test_arena_faster_bot_wins);
  RUN_TEST(test_arena_replay_independent_of_instances);
  return UNITY_END();
}
