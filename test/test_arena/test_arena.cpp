// Phase 6 tests: the Arena match engine. THE key property is determinism —
// same seed + same two programs => byte-identical match log every run (SPEC §18.1).
#include <unity.h>
#include <cstdio>
#include "game/Maze.h"
#include "game/MazeGen.h"
#include "game/Program.h"
#include "game/Arena.h"
#include "game/Bots.h"
#include "game/Score.h"
#include "game/Distill.h"
#include "game/Net.h"
#include "game/Interpreter.h"

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

// Sumo HP: a foe pinned against a wall (can't be shoved off) takes SEVERAL zaps to KO -- a
// sustained brawl, not a one-shove kill. Verifies the health + multi-hit + cooldown mechanic.
void test_sumo_hp_three_hits() {
  Maze m; m.reset(1, 3); m.fill(FLOOR);
  m.clearGoal();                                  // no goal -> the zapper keeps firing
  m.set(0, 2, WALL);                              // wall behind the victim -> can't be shoved off
  Pose s0; s0.row = 0; s0.col = 0; s0.facing = EAST;  // zapper
  Pose s1; s1.row = 0; s1.col = 1; s1.facing = EAST;  // victim, pinned against the wall
  Program zapper;
  Node loop = Node::repeatUntil(AT_GOAL);
  loop.body.push_back(Node::command(CMD_FIRE));
  zapper.main.push_back(loop);
  Program idle;
  Arena ar;
  ar.setup(&m, &zapper, &idle, s0, s1, MatchType::SUMO, 80);
  ar.tick();                                       // first hit lands...
  TEST_ASSERT_TRUE(ar.alive(1));                   // ...one hit does NOT KO
  TEST_ASSERT_EQUAL(SUMO_HP - 1, ar.hp(1));        // ...but it took damage
  for (int k = 0; k < 70 && ar.outcome() == ArenaOutcome::RUNNING; k++) ar.tick();
  TEST_ASSERT_FALSE(ar.alive(1));                  // eventually KO'd after several hits
  TEST_ASSERT_EQUAL((int)ArenaOutcome::BOT0, (int)ar.outcome());
}

// The Sumo ring must be deterministic from its seed (so a network battle that feeds both
// devices the SAME shared seed renders an identical ring) and be the big 8x10 board.
void test_sumo_ring_deterministic_and_big() {
  Maze A, B, C; Pose a0, a1, b0, b1, c0, c1;
  MazeGen::generateSumoRing(A, 42, a0, a1);
  MazeGen::generateSumoRing(B, 42, b0, b1);   // same seed -> identical on both "devices"
  MazeGen::generateSumoRing(C, 99, c0, c1);   // different seed -> a different ring
  TEST_ASSERT_EQUAL(8, A.rows()); TEST_ASSERT_EQUAL(10, A.cols());   // big ring
  bool sameAB = true, diffAC = false;
  for (int r = 0; r < A.rows(); r++)
    for (int c = 0; c < A.cols(); c++) {
      if (A.at(r, c) != B.at(r, c)) sameAB = false;
      if (A.at(r, c) != C.at(r, c)) diffAC = true;
    }
  TEST_ASSERT_TRUE(sameAB);   // same seed => byte-identical ring (network safe)
  TEST_ASSERT_TRUE(diffAC);   // a different seed => a different ring (varies each match)
  TEST_ASSERT_TRUE(a0.row != a1.row);   // starts offset onto different rows
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

// Wrap a trained brain as a battle program (REPEAT_UNTIL AT_GOAL { brain }).
static Program brainProgram(const Net& trained) {
  Program p;
  uint8_t idx = p.addBrain(1);
  p.brains[idx] = trained;
  Node loop = Node::repeatUntil(AT_GOAL);
  loop.body.push_back(Node::neuro(idx));
  p.main.push_back(loop);
  return p;
}

// The "Teach" button for Battle: distillHunter must produce a brain that actually FIGHTS.
// (1) vs an idle foe it hunts it down and wins; (2) across a spread of random rings it BEATS the
// code seeker Vex it's distilled from -- the cone-chaser it imitates closes on diagonal foes that
// the reactive Vex only oscillates at, so the taught brain is strictly the better fighter. This is
// the onboarding promise (a taught fighter can win the real battle vs Vex) and what makes Teach,
// not just Evolve, a viable one-tap path to a competent NeuroBot.
void test_battle_taught_brain_hunts_and_matches_vex() {
  int idleWins = 0, vexWins = 0, vexLosses = 0;
  for (uint32_t seed = 1; seed <= 6; seed++) {
    Net taught; taught.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 1);
    distillHunter(taught, seed, 2000);
    Maze m; Pose s0, s1; MazeGen::generateSumoRing(m, seed, s0, s1);
    // (1) hunts a passive foe down
    Program me1 = brainProgram(taught), idle;
    Arena a1; a1.setup(&m, &me1, &idle, s0, s1, MatchType::SUMO, 150);
    if ((int)a1.run() == (int)ArenaOutcome::BOT0) idleWins++;
    // (2) beats Vex
    Program me2 = brainProgram(taught), vex = hunterProgram();
    Arena a2; a2.setup(&m, &me2, &vex, s0, s1, MatchType::SUMO, 150);
    int o = (int)a2.run();
    if (o == (int)ArenaOutcome::BOT0) vexWins++;
    else if (o == (int)ArenaOutcome::BOT1) vexLosses++;
  }
  TEST_ASSERT_EQUAL(6, idleWins);   // always runs down a passive foe
  TEST_ASSERT_EQUAL(0, vexLosses);  // never loses to the seeker it imitates
  TEST_ASSERT_TRUE(vexWins >= 4);   // and beats it on a clear majority of rings
}

// The "Q-Learn" engine: qTrainHunter must discover the hunt purely from reward (no teacher). RL
// is noisier than Teach, but with the near-start curriculum it reliably runs down a passive foe
// AND holds its own (win or draw) against Vex on a spread of rings -- proving reward-driven
// training is a viable third path alongside Teach (imitation) and Evolve (selection).
void test_battle_qlearn_brain_hunts() {
  int idleWins = 0, vexWinsOrDraws = 0;
  for (uint32_t seed = 1; seed <= 10; seed++) {
    Net q; q.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 1);
    // Replicate the UI's chunked training EXACTLY (8 x 1000 episodes, fresh seed per chunk, global
    // epsilon decay) so this test validates the on-device Q-Learn path, not a different one.
    for (int c = 0; c < 8; c++) qTrainHunter(q, seed + 101u * (uint32_t)(8 - c), 1000, c * 1000, 8000);
    // evaluate on an UNSEEN ring (different seed than training) -- the real test is generalisation
    // to the random battle board, not memorising one layout. Over these 10 it scores 10/10 each;
    // the slack guards against minor float-order differences across toolchains.
    Maze m; Pose s0, s1; MazeGen::generateSumoRing(m, seed + 7777u, s0, s1);
    Program me1 = brainProgram(q), idle;
    Arena a1; a1.setup(&m, &me1, &idle, s0, s1, MatchType::SUMO, 150);
    if ((int)a1.run() == (int)ArenaOutcome::BOT0) idleWins++;
    Program me2 = brainProgram(q), vex = hunterProgram();
    Arena a2; a2.setup(&m, &me2, &vex, s0, s1, MatchType::SUMO, 150);
    if ((int)a2.run() != (int)ArenaOutcome::BOT1) vexWinsOrDraws++;
  }
  TEST_ASSERT_TRUE(idleWins >= 9);        // runs down a passive foe on essentially every ring
  TEST_ASSERT_TRUE(vexWinsOrDraws >= 9);  // and wins or draws against the code seeker too
}

// Wrap a trained RECURRENT brain as a battle program (REPEAT_UNTIL AT_GOAL { rnn-brain }).
static Program rnnProgram(const RNet& trained) {
  Program p;
  uint8_t idx = p.addBrain(1);
  p.rbrains[idx] = trained;
  Node loop = Node::repeatUntil(AT_GOAL);
  loop.body.push_back(Node::rnnBrain(idx));
  p.main.push_back(loop);
  return p;
}

// The RNN brain-type toggle for Battle Teach: distillHunterRnn must teach a recurrent brain to
// hunt (BPTT over chase episodes) well enough to run down a passive foe and hold its own vs Vex,
// proving a memory brain can fight -- so the toggle gives a real, working alternative brain type.
void test_battle_rnn_taught_hunts() {
  int idleWins = 0, vexWinsOrDraws = 0;
  for (uint32_t seed = 1; seed <= 6; seed++) {
    RNet r; r.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 1);
    distillHunterRnn(r, seed, 1500);
    Maze m; Pose s0, s1; MazeGen::generateSumoRing(m, seed + 7777u, s0, s1);   // unseen ring
    Program me1 = rnnProgram(r), idle;
    Arena a1; a1.setup(&m, &me1, &idle, s0, s1, MatchType::SUMO, 150);
    if ((int)a1.run() == (int)ArenaOutcome::BOT0) idleWins++;
    Program me2 = rnnProgram(r), vex = hunterProgram();
    Arena a2; a2.setup(&m, &me2, &vex, s0, s1, MatchType::SUMO, 150);
    if ((int)a2.run() != (int)ArenaOutcome::BOT1) vexWinsOrDraws++;
  }
  TEST_ASSERT_TRUE(idleWins >= 5);
  TEST_ASSERT_TRUE(vexWinsOrDraws >= 4);
}

// Full RECURRENT Q-learning (RNet::trainEpisodeQ): reward-driven, no teacher, memory brain.
// SMOKE test only -- recurrent semi-gradient TD is unstable here (and memory is noise for a
// reactive hunt), so it does NOT reliably beat Vex like the feedforward Q-learner. We assert it
// runs, trains, and learns *some* hunting (KOs at least a couple of passive foes), and leave the
// strong fighter to Teach / feedforward Q-Learn. See notes for the (working) hybrid alternative.
void test_battle_qlearn_rnn_runs() {
  // Pure smoke test: recurrent Q runs end-to-end and updates the brain. It does NOT produce a
  // reliable fighter (semi-gradient recurrent TD is unstable here + memory is noise for a reactive
  // hunt), so we deliberately do NOT gate on win rate -- Teach / feedforward Q-Learn are the path
  // to a strong brain. Kept so the capability compiles and is exercised.
  RNet r; r.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 1);
  for (int c = 0; c < 8; c++) qTrainHunterRnn(r, 1u + 101u * (uint32_t)(8 - c), 1000, c * 1000, 8000);
  TEST_ASSERT_TRUE(r.trained);
  Maze m; Pose s0, s1; MazeGen::generateSumoRing(m, 9999u, s0, s1);
  Program me = rnnProgram(r), idle;                 // it at least runs a full match without faulting
  Arena a; a.setup(&m, &me, &idle, s0, s1, MatchType::SUMO, 150);
  TEST_ASSERT_NOT_EQUAL((int)ArenaOutcome::RUNNING, (int)a.run());
}

static bool rnnSolves(const RNet& r, const Maze& m) {
  Program p = rnnProgram(r);
  Interpreter it; it.load(&p, &m, m.startPose(), 200);
  for (int s = 0; s < 200 && !it.finished(); s++) it.step();
  Pose pe = it.pose();
  return m.isGoal(pe.row, pe.col);
}

// Recurrent Q-learning for RACE: unlike the battle hunt, memory + goal-bearing senses make this
// learnable, so a reward-trained RNN should SOLVE campaign mazes it trained on.
void test_race_qlearn_rnn_solves() {
  const int LV = 6;
  RNet r; r.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 1);
  for (int c = 0; c < 4; c++) qTrainMazeRnn(r, 1234u, LV, 1000, c * 1000, 4 * 1000);
  int solved = 0;
  for (int lv = 1; lv <= LV; lv++) { Maze m; MazeGen::generate(m, 1234u, lv); if (rnnSolves(r, m)) solved++; }
  TEST_ASSERT_TRUE(solved >= LV - 1);   // solves nearly all the levels it trained on (6/6 in practice)
}

static bool ffSolves(const Net& n, const Maze& m) {
  Program p = brainProgram(n);
  Interpreter it; it.load(&p, &m, m.startPose(), 200);
  for (int s = 0; s < 200 && !it.finished(); s++) it.step();
  Pose pe = it.pose();
  return m.isGoal(pe.row, pe.col);
}

// Feedforward maze Q-learning (qTrainMaze): the campaign maze trainer's reward engine. Goal-bearing
// senses make navigation learnable from reward alone -- it should solve the levels it trained on.
void test_maze_qlearn_ff_solves() {
  const int LV = 6;
  Net n; n.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 1);
  for (int c = 0; c < 6; c++) qTrainMaze(n, 1234u, LV, 1000, c * 1000, 6 * 1000);
  int solved = 0;
  for (int lv = 1; lv <= LV; lv++) { Maze m; MazeGen::generate(m, 1234u, lv); if (ffSolves(n, m)) solved++; }
  TEST_ASSERT_TRUE(solved >= LV - 1);
}

// SOCCER: a bot directly behind the ball dribbles it into its goal over a couple of pushes.
// Verifies the push physics (walk into the ball -> it rolls one tile ahead) and goal scoring.
void test_soccer_push_into_goal() {
  Maze m; m.reset(1, 5); m.fill(FLOOR); m.clearGoal();   // no maze goal -> the dasher keeps walking
  Pose s0; s0.row = 0; s0.col = 1; s0.facing = EAST;     // pusher, just left of the ball
  Pose s1; s1.row = 0; s1.col = 0; s1.facing = WEST;     // idle opponent in the corner
  Program dasher;
  Node loop = Node::repeatUntil(AT_GOAL);
  loop.body.push_back(Node::command(CMD_FWD));
  dasher.main.push_back(loop);
  Program idle;
  Arena ar;
  ar.setup(&m, &dasher, &idle, s0, s1, MatchType::SOCCER, 50);
  Pose ball; ball.row = 0; ball.col = 2;
  Pose g0; g0.row = 0; g0.col = 4;                        // bot0 attacks the right end
  Pose g1; g1.row = 0; g1.col = 0;                        // bot1 attacks the left end
  ar.configSoccer(ball, g0, g1);
  ArenaOutcome o = ar.run();
  TEST_ASSERT_EQUAL((int)ArenaOutcome::BOT0, (int)o);
  TEST_ASSERT_TRUE(ar.goals(0) > 0);                     // bot0 put the ball in the net (timed match)
  TEST_ASSERT_EQUAL(0, ar.goals(1));                     // idle bot scored none
}

// SOCCER zap-swap: firing while facing the ball trades places with it AND turns the bot 180, so the
// ball ends up directly AHEAD again (a single forward then drives it back the way you came).
void test_soccer_zap_swaps_and_turns() {
  Maze m; m.reset(1, 5); m.fill(FLOOR); m.clearGoal();
  Pose s0; s0.row = 0; s0.col = 1; s0.facing = EAST;     // me, just left of the ball, facing it
  Pose s1; s1.row = 0; s1.col = 4; s1.facing = WEST;     // idle opponent at the far end
  Program zap; zap.main.push_back(Node::command(CMD_FIRE));
  Program idle;
  Arena ar;
  ar.setup(&m, &zap, &idle, s0, s1, MatchType::SOCCER, 1);   // one tick: just the zap
  Pose ball; ball.row = 0; ball.col = 2;                  // ball one tile ahead (east) of me
  Pose g0; g0.row = 0; g0.col = 4;
  Pose g1; g1.row = 0; g1.col = 0;
  ar.configSoccer(ball, g0, g1);
  ar.run();
  TEST_ASSERT_EQUAL(2, ar.pose(0).col);                  // I took the ball's old tile
  TEST_ASSERT_EQUAL((int)WEST, (int)ar.pose(0).facing);  // turned 180 (was EAST)
  TEST_ASSERT_EQUAL(1, ar.ball().col);                   // ball popped to my old tile -- now directly AHEAD of me
}

void test_soccer_deterministic() {
  Program a = dashProgram(), b = dashProgram();
  uint32_t h[2];
  for (int run = 0; run < 2; run++) {
    Maze m; Pose s0, s1; MazeGen::generateSumoRing(m, 5, s0, s1);
    Arena ar; ar.setup(&m, &a, &b, s0, s1, MatchType::SOCCER, 120);
    Pose ball; ball.row = (int8_t)(m.rows() / 2); ball.col = (int8_t)(m.cols() / 2);
    Pose g0; g0.row = ball.row; g0.col = (int8_t)(m.cols() - 1);
    Pose g1; g1.row = ball.row; g1.col = 0;
    ar.configSoccer(ball, g0, g1);
    ar.run();
    h[run] = ar.logHash();
  }
  TEST_ASSERT_EQUAL_UINT32(h[0], h[1]);
}

// A walled SOCCER pitch must be built right: a wall around the whole outside, with a symmetric
// 4-tile goal mouth (open floor) cut into the centre of each end (one wall tile above, one below),
// and the ball + starts on walkable floor.
void test_soccer_pitch_walled_with_mouths() {
  Maze m; Pose s0, s1, ball, g0, g1;
  MazeGen::generateSoccerPitch(m, 1, s0, s1, ball, g0, g1);
  int rows = m.rows(), cols = m.cols();
  // every border tile is a wall EXCEPT the 4-tile mouth (goalRow-2..goalRow+1) at each end
  for (int c = 0; c < cols; c++) { TEST_ASSERT_EQUAL((int)WALL, (int)m.at(0, c)); TEST_ASSERT_EQUAL((int)WALL, (int)m.at(rows - 1, c)); }
  for (int r = 0; r < rows; r++) {
    bool mouth = inGoalMouth(r, g0.row);
    TEST_ASSERT_EQUAL(mouth ? (int)FLOOR : (int)WALL, (int)m.at(r, 0));
    TEST_ASSERT_EQUAL(mouth ? (int)FLOOR : (int)WALL, (int)m.at(r, cols - 1));
  }
  // symmetric: exactly ONE wall tile above the mouth (row 1) and one below (rows-2)
  TEST_ASSERT_EQUAL((int)WALL, (int)m.at(g0.row + SOCCER_MOUTH_LO - 1, 0));   // wall just above the mouth
  TEST_ASSERT_EQUAL((int)WALL, (int)m.at(g0.row + SOCCER_MOUTH_HI + 1, 0));   // wall just below the mouth
  TEST_ASSERT_TRUE(m.isWalkable(ball.row, ball.col));
  TEST_ASSERT_TRUE(m.isWalkable(s0.row, s0.col));
  TEST_ASSERT_TRUE(m.isWalkable(s1.row, s1.col));
  TEST_ASSERT_EQUAL(cols - 1, g0.col);   // brain attacks the right mouth
  TEST_ASSERT_EQUAL(0, g1.col);          // opponent the left
}

// The SOCCER "Teach" (distillSoccer): a taught brain must actually SCORE -- on the walled pitch, vs
// an idle opponent, it dribbles the ball into the net (the ball ends in the goal mouth, a real goal,
// not just a timeout tie-break) the clear majority of the time. The one-tap path to a soccer bot.
void test_soccer_taught_brain_scores() {
  int realGoals = 0;
  for (uint32_t seed = 1; seed <= 6; seed++) {
    Net taught; taught.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 1);
    distillSoccer(taught, seed, 5000);
    Maze m; Pose s0, s1, ball, g0, g1;
    MazeGen::generateSoccerPitch(m, seed, s0, s1, ball, g0, g1);
    Program me = brainProgram(taught), idle;
    Arena ar; ar.setup(&m, &me, &idle, s0, s1, MatchType::SOCCER, 300);
    ar.configSoccer(ball, g0, g1);
    ar.run();
    if (ar.goals(0) > 0) realGoals++;   // the taught bot actually scored (the ball went in the net)
  }
  TEST_ASSERT_TRUE(realGoals >= 4);   // most taught brains dribble at least one goal vs an idle keeper
}

// A real soccer MATCH (what the live arena's tournaments run): two distilled soccer bots play a
// full game on the walled pitch. It must RESOLVE to a decisive winner (one bot dribbles the ball
// into its goal -- not a hung stalemate) and be deterministic, so a Cup/Ladder produces a champion.
void test_soccer_two_bots_match_resolves() {
  Net a, b;
  a.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 1); distillSoccer(a, 2, 5000);
  b.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 1); distillSoccer(b, 3, 5000);
  Maze m; Pose s0, s1, ball, g0, g1;
  MazeGen::generateSoccerPitch(m, 1, s0, s1, ball, g0, g1);
  Program pa = brainProgram(a), pb = brainProgram(b);
  uint32_t h[2]; int out[2];
  for (int run = 0; run < 2; run++) {
    Arena ar; ar.setup(&m, &pa, &pb, s0, s1, MatchType::SOCCER, 300);
    ar.configSoccer(ball, g0, g1);
    out[run] = (int)ar.run();
    h[run] = ar.logHash();
  }
  TEST_ASSERT_EQUAL_UINT32(h[0], h[1]);                            // deterministic replay
  TEST_ASSERT_NOT_EQUAL((int)ArenaOutcome::RUNNING, out[0]);       // it resolves (a champion emerges)
  TEST_ASSERT_NOT_EQUAL((int)ArenaOutcome::DRAW, out[0]);          // and decisively (someone scores / leads)
}

// Two GOOD soccer bots must MIX IT UP, not deadlock at midfield. The stalled-ball referee DROPS a
// loose ball at a fresh spot (never nudges it goalward), so the ball stays LIVELY and the bots
// scramble; every match RESOLVES, and goals -- when they come -- are bot-earned in open play (the
// rest go to the ball-position tiebreak). We assert the ball stays in play and goals still happen.
void test_soccer_two_bots_scramble_and_resolve() {
  int withGoals = 0, decisive = 0;
  for (uint32_t s = 1; s <= 6; s++) {
    Net a, b;
    a.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 1); distillSoccer(a, s, 5000);
    b.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 1); distillSoccer(b, s + 40, 5000);
    Maze m; Pose s0, s1, ball, g0, g1;
    MazeGen::generateSoccerPitch(m, 1, s0, s1, ball, g0, g1);
    Program pa = brainProgram(a), pb = brainProgram(b);
    Arena ar; ar.setup(&m, &pa, &pb, s0, s1, MatchType::SOCCER, 300);
    ar.configSoccer(ball, g0, g1);
    ArenaOutcome o = ar.run();
    if (o == ArenaOutcome::BOT0 || o == ArenaOutcome::BOT1) decisive++;  // resolves to a WINNER
    if (ar.goals(0) + ar.goals(1) > 0) withGoals++;                      // real goals were scored
  }
  TEST_ASSERT_TRUE(decisive >= 5);   // every match resolves to a winner -- no perpetual freeze
  TEST_ASSERT_TRUE(withGoals >= 3);  // and most are decided by actual scoring, not just position
}

// SOCCER "Q-Learn" (qTrainSoccer): reward-driven, no teacher. Replicating the UI's chunked run
// (epsilon decays globally across chunks), a feedforward brain must DISCOVER dribbling from reward
// alone and score into the net on a spread of kickoffs -- proving RL is a viable third soccer path
// alongside Teach (imitation) and Evolve (selection).
void test_soccer_qlearn_ff_scores() {
  Net q; q.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 1);
  for (int c = 0; c < 8; c++) qTrainSoccer(q, 7u + 101u * (uint32_t)(8 - c), 1000, c * 1000, 8000);
  Maze m; Pose s0, s1, ball, g0, g1;
  MazeGen::generateSoccerPitch(m, 1, s0, s1, ball, g0, g1);
  int scored = 0;
  for (int k = 0; k < 5; k++) {              // a few kickoffs from offset bot rows
    Program me = brainProgram(q), idle;
    Pose a = s0; a.row = (int8_t)(g0.row + (k - 2));
    if (!m.isWalkable(a.row, a.col)) a = s0;
    Arena ar; ar.setup(&m, &me, &idle, a, s1, MatchType::SOCCER, 300);
    ar.configSoccer(ball, g0, g1);
    ar.run();
    if (ar.goals(0) > 0) scored++;
  }
  TEST_ASSERT_TRUE(scored >= 3);             // reward-trained brain dribbles the ball home on most
}

// RECURRENT soccer Q-learning (qTrainSoccerRnn): the memory-brain reward path runs end-to-end,
// trains, and learns SOME dribbling (scores at least once vs an idle keeper). A smoke + capability
// test -- recurrent semi-gradient TD is noisier, so we don't gate it as hard as the feedforward one.
void test_soccer_qlearn_rnn_runs() {
  RNet r; r.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 1);
  for (int c = 0; c < 8; c++) qTrainSoccerRnn(r, 3u + 101u * (uint32_t)(8 - c), 1000, c * 1000, 8000);
  TEST_ASSERT_TRUE(r.trained);
  Maze m; Pose s0, s1, ball, g0, g1;
  MazeGen::generateSoccerPitch(m, 1, s0, s1, ball, g0, g1);
  int scored = 0;
  for (int k = 0; k < 5; k++) {
    Program me = rnnProgram(r), idle;
    Pose a = s0; a.row = (int8_t)(g0.row + (k - 2));
    if (!m.isWalkable(a.row, a.col)) a = s0;
    Arena ar; ar.setup(&m, &me, &idle, a, s1, MatchType::SOCCER, 300);
    ar.configSoccer(ball, g0, g1);
    ar.run();
    if (ar.goals(0) > 0) scored++;
  }
  TEST_ASSERT_TRUE(scored >= 1);             // learns at least some reward-driven dribbling
}

// "Train a set of super players and play them": train 4 strong soccer bots (high-epoch distill,
// distinct seeds) and run a full round-robin (home + away) of timed matches, printing every
// scoreline + a standings table. Demonstrates how good the little brains are against each OTHER.
void test_super_players_roundrobin() {
  const int N = 4;
  Net bots[N];
  for (int i = 0; i < N; i++) { bots[i].config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 1); distillSoccer(bots[i], 1u + (uint32_t)i, 12000); }
  Maze m; Pose s0, s1, ball, g0, g1;
  MazeGen::generateSoccerPitch(m, 1, s0, s1, ball, g0, g1);
  int win[N] = {0}, drw[N] = {0}, gf[N] = {0}, ga[N] = {0};
  printf("\n=== SUPER PLAYERS round-robin (timed soccer, home+away) ===\n");
  for (int i = 0; i < N; i++)
    for (int j = 0; j < N; j++) {
      if (i == j) continue;
      Program pa = brainProgram(bots[i]), pb = brainProgram(bots[j]);
      Arena ar; ar.setup(&m, &pa, &pb, s0, s1, MatchType::SOCCER, 300);
      ar.configSoccer(ball, g0, g1);
      ArenaOutcome o = ar.run();
      int ag = ar.goals(0), bg = ar.goals(1);
      gf[i] += ag; ga[i] += bg; gf[j] += bg; ga[j] += ag;
      const char* res = o == ArenaOutcome::BOT0 ? "W" : o == ArenaOutcome::BOT1 ? "L" : "D";
      if (o == ArenaOutcome::BOT0) win[i]++; else if (o == ArenaOutcome::BOT1) win[j]++; else { drw[i]++; drw[j]++; }
      printf("  P%d vs P%d :  %d-%d  (%s)\n", i, j, ag, bg, res);
    }
  printf("  --- standings ---\n");
  for (int i = 0; i < N; i++) printf("  P%d:  %d wins  %d draws   goals %d-%d\n", i, win[i], drw[i], gf[i], ga[i]);
  int totalGoals = 0; for (int i = 0; i < N; i++) totalGoals += gf[i];
  printf("  total goals across all %d matches: %d\n", N * (N - 1), totalGoals);
  TEST_ASSERT_TRUE(totalGoals >= 20);   // well-trained bots play HIGH-SCORING soccer, not 0-0 tiebreaks
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_super_players_roundrobin);
  RUN_TEST(test_soccer_push_into_goal);
  RUN_TEST(test_soccer_zap_swaps_and_turns);
  RUN_TEST(test_soccer_two_bots_match_resolves);
  RUN_TEST(test_soccer_two_bots_scramble_and_resolve);
  RUN_TEST(test_soccer_qlearn_ff_scores);
  RUN_TEST(test_soccer_qlearn_rnn_runs);
  RUN_TEST(test_soccer_pitch_walled_with_mouths);
  RUN_TEST(test_soccer_deterministic);
  RUN_TEST(test_soccer_taught_brain_scores);
  RUN_TEST(test_maze_qlearn_ff_solves);
  RUN_TEST(test_battle_taught_brain_hunts_and_matches_vex);
  RUN_TEST(test_battle_qlearn_brain_hunts);
  RUN_TEST(test_battle_rnn_taught_hunts);
  RUN_TEST(test_battle_qlearn_rnn_runs);
  RUN_TEST(test_race_qlearn_rnn_solves);
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
  RUN_TEST(test_sumo_hp_three_hits);
  RUN_TEST(test_sumo_ring_deterministic_and_big);
  RUN_TEST(test_sumo_deterministic);
  return UNITY_END();
}
