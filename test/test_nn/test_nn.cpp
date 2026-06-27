// NeuroBot tests: a single neuron must actually LEARN via backprop. The core
// teaching claim is "guess -> see the error -> nudge the weights -> get better",
// so the tests assert the loss falls and the learned decisions are correct on
// linearly separable problems (and that XOR, which needs a hidden layer, does NOT
// separate — the motivation for the next lesson).
#include <unity.h>
#include "game/Perceptron.h"
#include "game/Net.h"
#include "game/Maze.h"
#include "game/MazeGen.h"
#include "game/Program.h"
#include "game/Interpreter.h"
#include "game/Distill.h"
#include "game/Gauntlet.h"
#include "game/Pilot.h"
#include "game/Reactive.h"
#include "game/Sensors.h"
#include "game/RNet.h"
#include "game/Score.h"

using namespace gb;

void setUp() {}
void tearDown() {}

// "turn if wall": 1 input. x=0 -> 0 (go), x=1 -> 1 (turn).
void test_learns_turn_if_wall() {
  Perceptron p; p.nIn = 1; p.lr = 0.8f; p.reset(7);
  const float X[2] = {0.0f, 1.0f};
  const float y[2] = {0.0f, 1.0f};
  float first = p.trainEpoch(X, y, 2);
  float last = first;
  for (int e = 0; e < 4000; e++) last = p.trainEpoch(X, y, 2);
  TEST_ASSERT_TRUE(last < first);          // it learned (loss fell)
  TEST_ASSERT_TRUE(last < 0.02f);          // and learned it well
  float x0 = 0.0f, x1 = 1.0f;
  TEST_ASSERT_TRUE(p.forward(&x0) < 0.5f); // no wall -> go
  TEST_ASSERT_TRUE(p.forward(&x1) > 0.5f); // wall -> turn
}

// OR is linearly separable -> a single neuron can learn it.
void test_learns_or() {
  Perceptron p; p.nIn = 2; p.lr = 0.8f; p.reset(42);
  const float X[8] = {0,0, 0,1, 1,0, 1,1};
  const float y[4] = {0, 1, 1, 1};
  for (int e = 0; e < 6000; e++) p.trainEpoch(X, y, 4);
  for (int i = 0; i < 4; i++) {
    float pred = p.forward(&X[i * 2]);
    if (y[i] > 0.5f) TEST_ASSERT_TRUE(pred > 0.5f);
    else             TEST_ASSERT_TRUE(pred < 0.5f);
  }
}

// XOR is NOT linearly separable -> a single neuron CANNOT solve it. This is the
// lesson that motivates a hidden layer; assert it genuinely fails to separate.
void test_xor_is_unlearnable_by_one_neuron() {
  Perceptron p; p.nIn = 2; p.lr = 0.8f; p.reset(99);
  const float X[8] = {0,0, 0,1, 1,0, 1,1};
  const float y[4] = {0, 1, 1, 0};
  for (int e = 0; e < 8000; e++) p.trainEpoch(X, y, 4);
  int correct = 0;
  for (int i = 0; i < 4; i++) {
    float pred = p.forward(&X[i * 2]);
    if ((pred > 0.5f) == (y[i] > 0.5f)) correct++;
  }
  TEST_ASSERT_TRUE(correct < 4);  // can't get all four (no single line separates XOR)
}

// Deterministic init: same seed -> same starting weights (reproducible lessons).
void test_reset_is_deterministic() {
  Perceptron a, b; a.nIn = 2; b.nIn = 2;
  a.reset(123); b.reset(123);
  for (int i = 0; i < 2; i++) TEST_ASSERT_EQUAL_FLOAT(a.w[i], b.w[i]);
  TEST_ASSERT_EQUAL_FLOAT(a.b, b.b);
}

// A multi-layer net (hidden layer) CAN solve XOR — the payoff of lesson 3, and proof
// the backprop-through-hidden math is right.
void test_net_learns_xor() {
  Net net; net.config(2, 8, 1, 3); net.lr = 0.5f;
  const float X[8] = {0,0, 0,1, 1,0, 1,1};
  const float Y[4] = {0, 1, 1, 0};
  for (int e = 0; e < 30000; e++) net.trainEpoch(X, Y, 4);
  for (int i = 0; i < 4; i++) {
    float o[NET_MAX_OUT]; net.forward(&X[i * 2], o);
    TEST_ASSERT_TRUE((o[0] > 0.5f) == (Y[i] > 0.5f));  // all four correct
  }
}

// Multi-class, the lesson-2 shape: 2 sensors -> 3 actions (go/turn/jump), argmax picks.
// Rule: jump if pit (priority), else turn if wall, else go.
void test_net_multiclass_go_turn_jump() {
  Net net; net.config(2, 6, 3, 11); net.lr = 0.4f;
  // outputs: 0=go, 1=turn, 2=jump
  const float X[8] = {0,0, 0,1, 1,0, 1,1};
  const float Y[12] = {1,0,0,  0,0,1,  0,1,0,  0,0,1};
  for (int e = 0; e < 20000; e++) net.trainEpoch(X, Y, 4);
  TEST_ASSERT_EQUAL_INT(0, net.argmax(&X[0]));  // (no wall,no pit) -> go
  TEST_ASSERT_EQUAL_INT(2, net.argmax(&X[2]));  // (no wall,pit)    -> jump
  TEST_ASSERT_EQUAL_INT(1, net.argmax(&X[4]));  // (wall,no pit)    -> turn
  TEST_ASSERT_EQUAL_INT(2, net.argmax(&X[6]));  // (wall,pit)       -> jump
}

// argmax can be told to skip a forbidden action (e.g. zap in a solo maze) so an untrained output
// can't win and waste a step -- it falls through to the strongest ALLOWED action.
void test_argmax_mask_skips_forbidden_action() {
  Net n; n.config(2, 4, 5, 5);
  for (int k = 0; k < 5; k++) n.b2[k] = -6.0f;
  n.b2[4] = 6.0f;   // force the zap output (action 4) highest
  n.b2[1] = 3.0f;   // turn-left the runner-up
  float x[2] = {0.f, 0.f};
  TEST_ASSERT_EQUAL_INT(4, n.argmax(x));                 // unmasked -> zap wins
  TEST_ASSERT_EQUAL_INT(1, n.argmax(x, ~(1u << 4)));     // zap forbidden -> next best (turn-left)
}

void test_net_reset_deterministic() {
  Net a, b; a.config(3, 5, 4, 77); b.config(3, 5, 4, 77);
  for (int j = 0; j < 5; j++)
    for (int i = 0; i < 3; i++) TEST_ASSERT_EQUAL_FLOAT(a.w1[j][i], b.w1[j][i]);
}

// A NEURO node runs its brain inside a real program: "REPEAT_UNTIL at_goal { NEURO }".
// Force the brain to always pick action 0 (forward) and it should walk a corridor home.
void test_neuro_node_drives_robot() {
  Program prog;
  uint8_t idx = prog.addBrain(1);
  Net& b = prog.brains[idx];
  for (int k = 0; k < 5; k++) {                    // force argmax -> forward
    for (int j = 0; j < b.nHid; j++) b.w2[k][j] = 0.0f;
    b.b2[k] = (k == 0) ? 6.0f : -6.0f;
  }
  Node loop = Node::repeatUntil(AT_GOAL);
  loop.body.push_back(Node::neuro(idx));
  prog.main.push_back(loop);

  Maze m; m.reset(5, 5); m.fill(FLOOR);
  Pose s; s.row = 2; s.col = 0; s.facing = EAST;
  m.setStart(s); m.set(2, 0, START); m.setGoal(2, 4);

  Interpreter it; it.load(&prog, &m, m.startPose());
  TEST_ASSERT_EQUAL((int)OUT_WIN, (int)it.runToEnd());  // brain drove it to the battery
  TEST_ASSERT_TRUE(it.pose().col == 4 && it.pose().row == 2);
}

// The brain travels with the program when copied (value semantics, no shared state).
void test_brain_copies_with_program() {
  Program a; uint8_t idx = a.addBrain(5);
  a.brains[idx].b2[0] = 3.14f;
  Program b = a;                       // copy
  a.brains[idx].b2[0] = -1.0f;         // mutate the original
  TEST_ASSERT_EQUAL_FLOAT(3.14f, b.brains[idx].b2[0]);  // copy is independent
}

// Distillation (the primary trainer): the brain learns to imitate the BFS solver by
// backprop, then navigates the maze itself.
void test_distill_brain_navigates() {
  Maze m; MazeGen::generate(m, 5, 5);
  Net brain; brain.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 1);
  TEST_ASSERT_TRUE(distillSolver(brain, m, false, 500));

  Program prog; prog.brains.push_back(brain);
  Node loop = Node::repeatUntil(AT_GOAL);
  loop.body.push_back(Node::neuro(0));
  prog.main.push_back(loop);
  Interpreter it; it.load(&prog, &m, m.startPose(), 500);
  Outcome o = it.runToEnd();
  int endDist = distanceToGoal(m, it.pose().row, it.pose().col);
  TEST_ASSERT_TRUE(o == OUT_WIN || endDist <= 1);  // imitates the solver -> reaches (near) goal
}

// A drawn tile path becomes a runnable teacher program that reaches the goal, including
// when it must turn to face the route.
void test_path_to_program_walks_and_turns() {
  Maze m; m.reset(1, 3); m.fill(FLOOR);
  Pose s; s.row = 0; s.col = 0; s.facing = NORTH;  // facing away from the route
  m.setStart(s); m.set(0, 0, START); m.setGoal(0, 2);
  uint8_t tiles[3] = {0, 1, 2};                    // walk east along row 0
  Program prog;
  TEST_ASSERT_TRUE(pathToProgram(m, tiles, 3, prog));
  Interpreter it; it.load(&prog, &m, m.startPose());
  TEST_ASSERT_EQUAL(OUT_WIN, it.runToEnd());       // turns to face east, then walks to goal
}

// A jump-of-two over a pit is a legal path segment.
void test_path_to_program_jumps_pit() {
  Maze m; m.reset(1, 3); m.fill(FLOOR);
  Pose s; s.row = 0; s.col = 0; s.facing = EAST;
  m.setStart(s); m.set(0, 0, START); m.set(0, 1, PIT); m.setGoal(0, 2);
  uint8_t tiles[2] = {0, 2};                        // (0,0) -> (0,2): a jump over the pit
  Program prog;
  TEST_ASSERT_TRUE(pathToProgram(m, tiles, 2, prog));
  Interpreter it; it.load(&prog, &m, m.startPose());
  TEST_ASSERT_EQUAL(OUT_WIN, it.runToEnd());
}

// A diagonal hop isn't a legal move, so conversion fails (the UI rejects such taps).
void test_path_to_program_rejects_diagonal() {
  Maze m; m.reset(2, 2); m.fill(FLOOR);
  Pose s; s.row = 0; s.col = 0; s.facing = EAST;
  m.setStart(s); m.set(0, 0, START); m.setGoal(1, 1);
  uint8_t tiles[2] = {0, 3};                        // (0,0) -> (1,1), cols=2 -> diagonal
  Program prog;
  TEST_ASSERT_FALSE(pathToProgram(m, tiles, 2, prog));
}

// Imitation learning: a brain distilled from a drawn path follows it to the goal.
void test_distill_path_navigates() {
  Maze m; m.reset(1, 5); m.fill(FLOOR);
  Pose s; s.row = 0; s.col = 0; s.facing = EAST;
  m.setStart(s); m.set(0, 0, START); m.setGoal(0, 4);
  uint8_t tiles[5] = {0, 1, 2, 3, 4};
  Net brain; brain.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 1);
  TEST_ASSERT_TRUE(distillPath(brain, m, tiles, 5, 500));
  Program prog; prog.brains.push_back(brain);
  Node loop = Node::repeatUntil(AT_GOAL); loop.body.push_back(Node::neuro(0)); prog.main.push_back(loop);
  Interpreter it; it.load(&prog, &m, m.startPose(), 500);
  Outcome o = it.runToEnd();
  TEST_ASSERT_TRUE(o == OUT_WIN || distanceToGoal(m, it.pose().row, it.pose().col) <= 1);
}

// The Generalist gauntlet score is bounded and deterministic for a given brain.
void test_gauntlet_bounded_and_deterministic() {
  Net brain; brain.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 7);
  int a = gauntletRun(brain);
  int b = gauntletRun(brain);
  TEST_ASSERT_EQUAL(a, b);
  TEST_ASSERT_TRUE(a >= 0 && a <= GAUNTLET_MAZES);
}

// The prize must stay winnable: a reactive brain, trained by the Generalize batches,
// clears the whole held-out gauntlet. (Guards against regressing into an impossible badge.)
void test_gauntlet_is_winnable() {
  Net brain; brain.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 1);
  gauntletTrain(brain, 50);
  TEST_ASSERT_EQUAL_MESSAGE(GAUNTLET_MAZES, gauntletRun(brain),
                            "a trained reactive brain should clear the whole gauntlet");
}

// Pilot mode (planner waypoints + a pilot-trained follower) clears the campaign that a bare
// reactive brain cannot — the planner-decides-where / brain-decides-how split.
void test_pilot_clears_campaign() {
  const uint32_t seed = 4242;
  Net brain; brain.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 1);
  pilotTrain(brain, seed, 30, 30);          // learn to steer toward waypoints on lvls 1..30
  TEST_ASSERT_EQUAL_MESSAGE(30, pilotRun(brain, seed, 30),
                            "a pilot-trained brain should clear the whole campaign chunk");
}

// TEMP: does an RNN (memory) beat a feedforward brain (no memory) at solving mazes ALONE?
// Both imitate the memory-using explorer from the SAME 10 senses (no trail input). The RNN
// must build memory internally via its recurrent state; the feedforward brain cannot.
// Roll out the explorer on a maze, capturing (senses, action) for each step.
static int explorerRollout(const Maze& m, float* X, int* act, int maxT) {
  uint8_t vis[MAZE_MAX_CELLS] = {0};
  Pose p = m.startPose(); vis[p.row * m.cols() + p.col] = 1;
  int T = 0;
  for (int s = 0; s < maxT; s++) {
    if (m.isGoal(p.row, p.col)) break;
    senseEgo(m, p, nullptr, X + T * SENSOR_COUNT);          // standard 10 senses, no trail
    Cmd c = exploreActionTo(m, p, vis, m.goalRow(), m.goalCol());
    act[T] = reactiveCmdToAction(c); T++;
    if (reactiveApply(m, p, c) != OUT_OK) break;
    vis[p.row * m.cols() + p.col] = 1;
  }
  return T;
}
static int ffBrainCount(const Net& brain, uint32_t seed, int N) {
  int n = 0;
  for (int l = 1; l <= N; l++) {
    Maze m; MazeGen::generate(m, seed, l);
    Pose p = m.startPose(); bool won = false;
    for (int s = 0; s < 300; s++) {
      if (m.isGoal(p.row, p.col)) { won = true; break; }
      float sv[SENSOR_COUNT]; senseEgo(m, p, nullptr, sv);
      int a = brain.argmax(sv);
      Cmd c = a == 1 ? CMD_TURN_L : a == 2 ? CMD_TURN_R : a == 3 ? CMD_JUMP : CMD_FWD;
      Outcome o = reactiveApply(m, p, c);
      if (o == OUT_WIN) { won = true; break; }
      if (o == OUT_BONK || o == OUT_FELL) break;
    }
    if (won) n++;
  }
  return n;
}
static int rnnBrainCount(RNet& brain, uint32_t seed, int N) {
  int n = 0;
  for (int l = 1; l <= N; l++) {
    Maze m; MazeGen::generate(m, seed, l);
    Pose p = m.startPose(); bool won = false; brain.resetState();
    for (int s = 0; s < 300; s++) {
      if (m.isGoal(p.row, p.col)) { won = true; break; }
      float sv[SENSOR_COUNT]; senseEgo(m, p, nullptr, sv);
      int a = brain.argmaxStep(sv);
      Cmd c = a == 1 ? CMD_TURN_L : a == 2 ? CMD_TURN_R : a == 3 ? CMD_JUMP : CMD_FWD;
      Outcome o = reactiveApply(m, p, c);
      if (o == OUT_WIN) { won = true; break; }
      if (o == OUT_BONK || o == OUT_FELL) break;
    }
    if (won) n++;
  }
  return n;
}
// Memory wins: an RNN trained (BPTT) to imitate the explorer solves more mazes ALONE than a
// feedforward brain given the identical training examples — because escaping dead-ends needs
// state the feedforward brain can't hold. Guards the RNN lesson's core claim.
void test_rnn_memory_beats_feedforward() {
  const uint32_t seed = 4242; const int N = 30;
  // Same 8-neuron hidden layer as the real robot brain — the ONLY difference is the RNN's
  // recurrent memory loop, so the win is attributable to memory, not extra capacity.
  Net ff; ff.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 1);
  RNet rnn; rnn.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 1); rnn.lr = 0.05f;
  static float X[56 * SENSOR_COUNT]; static int act[56];
  for (int e = 0; e < 100; e++)
    for (int l = 1; l <= N; l++) {
      Maze m; MazeGen::generate(m, seed, l);
      int T = explorerRollout(m, X, act, 56);
      if (T <= 0) continue;
      rnn.trainEpisode(X, act, T);
      for (int t = 0; t < T; t++) {            // same examples for the feedforward brain
        float tgt[NET_MAX_OUT] = {0}; tgt[act[t]] = 1.0f;
        ff.trainStep(X + t * SENSOR_COUNT, tgt);
      }
    }
  int ffN = ffBrainCount(ff, seed, N);
  int rnnN = rnnBrainCount(rnn, seed, N);
  TEST_ASSERT_TRUE_MESSAGE(rnnN >= ffN + 3, "the RNN's memory should clear clearly more mazes");
}

// A NEURO node with rnn=true runs the program's recurrent brain (rbrains), not the
// feedforward one — verifies the interpreter wiring + per-run memory reset.
void test_rnn_brain_node_runs() {
  Program prog;
  uint8_t idx = prog.addBrain(1);             // creates brains[idx] AND rbrains[idx]
  RNet& r = prog.rbrains[idx];
  for (int m = 0; m < 5; m++) {               // force argmax -> forward (action 0)
    for (int j = 0; j < r.nHid; j++) r.who[m][j] = 0.0f;
    r.bo[m] = (m == 0) ? 6.0f : -6.0f;
  }
  Node loop = Node::repeatUntil(AT_GOAL);
  loop.body.push_back(Node::rnnBrain(idx));   // the RECURRENT brain
  prog.main.push_back(loop);

  Maze m; m.reset(1, 5); m.fill(FLOOR);
  Pose s; s.row = 0; s.col = 0; s.facing = EAST;
  m.setStart(s); m.set(0, 0, START); m.setGoal(0, 4);
  Interpreter it; it.load(&prog, &m, m.startPose());
  TEST_ASSERT_EQUAL((int)OUT_WIN, (int)it.runToEnd());   // the rnn node drove it to the goal
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_rnn_memory_beats_feedforward);
  RUN_TEST(test_rnn_brain_node_runs);
  RUN_TEST(test_pilot_clears_campaign);
  RUN_TEST(test_distill_brain_navigates);
  RUN_TEST(test_gauntlet_bounded_and_deterministic);
  RUN_TEST(test_gauntlet_is_winnable);
  RUN_TEST(test_path_to_program_walks_and_turns);
  RUN_TEST(test_path_to_program_jumps_pit);
  RUN_TEST(test_path_to_program_rejects_diagonal);
  RUN_TEST(test_distill_path_navigates);
  RUN_TEST(test_learns_turn_if_wall);
  RUN_TEST(test_learns_or);
  RUN_TEST(test_xor_is_unlearnable_by_one_neuron);
  RUN_TEST(test_reset_is_deterministic);
  RUN_TEST(test_net_learns_xor);
  RUN_TEST(test_net_multiclass_go_turn_jump);
  RUN_TEST(test_argmax_mask_skips_forbidden_action);
  RUN_TEST(test_net_reset_deterministic);
  RUN_TEST(test_neuro_node_drives_robot);
  RUN_TEST(test_brain_copies_with_program);
  return UNITY_END();
}
