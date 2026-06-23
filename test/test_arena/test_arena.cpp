// Phase 6 tests: the Arena match engine. THE key property is determinism —
// same seed + same two programs => byte-identical match log every run (SPEC §18.1).
#include <unity.h>
#include "game/Maze.h"
#include "game/MazeGen.h"
#include "game/Program.h"
#include "game/Arena.h"
#include "game/Bots.h"
#include "game/Score.h"

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
  // Two navigators race to a photo-finish — it must PLAY OUT over several ticks.
  Maze m; Pose s0, s1;
  MazeGen::generateArena(m, 7, s0, s1);
  Program a, b;
  solveMazeFrom(m, s0, true, a);
  solveMazeFrom(m, s1, true, b);
  Arena ar;
  ar.setup(&m, &a, &b, s0, s1, MatchType::RACE);
  ArenaOutcome o = ar.run();
  TEST_ASSERT_NOT_EQUAL((int)ArenaOutcome::RUNNING, (int)o);
  TEST_ASSERT_TRUE(ar.ticks() >= 3);  // bots travel the lane before it resolves
}

void test_arena_faster_bot_wins() {
  // A navigator vs an idler (empty program): the navigator reaches the goal and wins.
  Maze m; Pose s0, s1;
  MazeGen::generateArena(m, 11, s0, s1);
  Program fast; solveMazeFrom(m, s0, true, fast);
  Program idle;  // empty -> DONE_NO_WIN immediately
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

// Sumo: a zapper shoves an idler standing in front of a pit -> idler out, zapper wins.
void test_sumo_zap_into_pit() {
  Maze m;
  m.reset(1, 4);
  m.fill(FLOOR);
  m.set(0, 3, PIT);                 // pit at the right end
  Pose s0; s0.row = 0; s0.col = 1; s0.facing = EAST;  // zapper
  Pose s1; s1.row = 0; s1.col = 2; s1.facing = WEST;  // victim, just left of the pit
  // zapper program: FIRE ("zap") repeatedly
  Program zapper;
  Node loop = Node::repeatUntil(AT_GOAL);
  loop.body.push_back(Node::command(CMD_FIRE));
  zapper.main.push_back(loop);
  Program idle;  // empty -> stands still then done
  Arena ar;
  ar.setup(&m, &zapper, &idle, s0, s1, MatchType::SUMO, 50);
  ArenaOutcome o = ar.run();
  TEST_ASSERT_EQUAL((int)ArenaOutcome::BOT0, (int)o);
  TEST_ASSERT_FALSE(ar.alive(1));
}

// A trained brain that always picks "zap" (output 4) must shove like a code FIRE.
// This exercises the N_NEURO -> lastCmd() path the arena now reads — the original bug
// was that a brain's chosen action was invisible to Sumo combat resolution.
void test_sumo_brain_zap_shoves() {
  Maze m;
  m.reset(1, 4);
  m.fill(FLOOR);
  m.set(0, 3, PIT);
  Pose s0; s0.row = 0; s0.col = 1; s0.facing = EAST;   // brain zapper
  Pose s1; s1.row = 0; s1.col = 2; s1.facing = WEST;   // victim by the pit
  Program zapper;
  uint8_t idx = zapper.addBrain(1);
  Net& brain = zapper.brains[idx];
  brain.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 1);
  for (int k = 0; k < 5; k++) brain.b2[k] = (k == 4) ? 100.f : -100.f;  // force argmax = zap
  Node loop = Node::repeatUntil(AT_GOAL);
  loop.body.push_back(Node::neuro(idx));
  zapper.main.push_back(loop);
  Program idle;
  Arena ar;
  ar.setup(&m, &zapper, &idle, s0, s1, MatchType::SUMO, 50);
  ArenaOutcome o = ar.run();
  TEST_ASSERT_EQUAL((int)ArenaOutcome::BOT0, (int)o);
  TEST_ASSERT_FALSE(ar.alive(1));
}

// Sumo: walking forward INTO an enemy pinned against the edge shoves it off -> rammer wins.
// This is the fix for "sumo matches always Draw" -- contact now decides, not just a rare zap.
void test_sumo_ram_off_edge() {
  Maze m;
  m.reset(1, 3);
  m.fill(FLOOR);
  Pose s0; s0.row = 0; s0.col = 1; s0.facing = EAST;   // rammer, walks forward
  Pose s1; s1.row = 0; s1.col = 2; s1.facing = EAST;   // victim, against the right edge
  Program rammer;
  Node loop = Node::repeatUntil(AT_GOAL);
  loop.body.push_back(Node::command(CMD_FWD));
  rammer.main.push_back(loop);
  Program idle;
  Arena ar;
  ar.setup(&m, &rammer, &idle, s0, s1, MatchType::SUMO, 50);
  ArenaOutcome o = ar.run();
  TEST_ASSERT_EQUAL((int)ArenaOutcome::BOT0, (int)o);   // rammer wins (no Draw)
  TEST_ASSERT_FALSE(ar.alive(1));
}

// The seeking hunter TURNS toward a foe off to its side, then zaps it off the edge.
// Verifies the directional ENEMY_LEFT/RIGHT sensing the brawl relies on.
void test_sumo_seeker_turns_and_kos() {
  Maze m; m.reset(3, 3); m.fill(FLOOR);
  Pose s0; s0.row = 1; s0.col = 1; s0.facing = NORTH;  // hunter, facing away from the foe
  Pose s1; s1.row = 1; s1.col = 2; s1.facing = NORTH;  // idle foe to the hunter's right, at the edge
  Program hunter = hunterProgram();
  Program idle;
  Arena ar;
  ar.setup(&m, &hunter, &idle, s0, s1, MatchType::SUMO, 50);
  ArenaOutcome o = ar.run();
  TEST_ASSERT_EQUAL((int)ArenaOutcome::BOT0, (int)o);   // turns east, zaps the foe off-board
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

// A smart navigator (solved path) must beat a blind dasher on the arena board
// (the dash wall makes the race decisive, not a draw).
void test_arena_smart_beats_dasher() {
  int decisive = 0;
  for (uint32_t s = 1; s <= 20; s++) {
    Maze m; Pose s0, s1;
    MazeGen::generateArena(m, s, s0, s1);
    Program smart, dumb = dashProgram();
    if (!solveMazeFrom(m, s0, true, smart)) continue;  // s0 should be solvable
    Arena ar;
    ar.setup(&m, &smart, &dumb, s0, s1, MatchType::RACE);
    ArenaOutcome o = ar.run();
    if (o == ArenaOutcome::BOT0) decisive++;
    TEST_ASSERT_NOT_EQUAL((int)ArenaOutcome::BOT1, (int)o);  // the dasher never wins
  }
  TEST_ASSERT_TRUE(decisive >= 12);  // the navigator wins most of the time
}

// A NeuroBot brain battles in the arena via the normal program path (the brain travels
// with the program; the arena already feeds each bot an EnemyView for combat sensing).
// It must run and be deterministic, like any other bot.
static Program neuroProgram(uint32_t seed) {
  Program p;
  uint8_t idx = p.addBrain(seed);
  Node loop = Node::repeatUntil(AT_GOAL);
  loop.body.push_back(Node::neuro(idx));
  p.main.push_back(loop);
  return p;
}

void test_arena_neuro_brains_battle() {
  Maze m; Pose s0, s1;
  MazeGen::generateArena(m, 7, s0, s1);
  Program a = neuroProgram(1), b = neuroProgram(2);
  Arena a1, a2;
  a1.setup(&m, &a, &b, s0, s1, MatchType::RACE); a1.run();
  a2.setup(&m, &a, &b, s0, s1, MatchType::RACE); a2.run();
  TEST_ASSERT_EQUAL_UINT32(a1.logHash(), a2.logHash());                 // deterministic
  TEST_ASSERT_NOT_EQUAL((int)ArenaOutcome::RUNNING, (int)a1.outcome()); // it resolves
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_arena_neuro_brains_battle);
  RUN_TEST(test_arena_board_in_bounds);
  RUN_TEST(test_arena_smart_beats_dasher);
  RUN_TEST(test_arena_deterministic_log);
  RUN_TEST(test_arena_match_plays_out);
  RUN_TEST(test_arena_faster_bot_wins);
  RUN_TEST(test_arena_replay_independent_of_instances);
  RUN_TEST(test_sumo_zap_into_pit);
  RUN_TEST(test_sumo_brain_zap_shoves);
  RUN_TEST(test_sumo_ram_off_edge);
  RUN_TEST(test_sumo_seeker_turns_and_kos);
  RUN_TEST(test_sumo_deterministic);
  return UNITY_END();
}
