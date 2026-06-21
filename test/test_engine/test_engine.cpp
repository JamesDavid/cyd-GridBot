// Phase 1 engine tests: Maze model, AST, and the steppable Interpreter outcomes
// against hand-built fixtures (SPEC §8). These are the primary correctness proof.
#include <unity.h>
#include "game/Maze.h"
#include "game/Program.h"
#include "game/Interpreter.h"

using namespace gb;

void setUp() {}
void tearDown() {}

// 3x3 all-floor maze, START at (2,0) facing EAST, GOAL at (2,2).
static Maze straightMaze() {
  Maze m;
  m.reset(3, 3);
  m.fill(FLOOR);
  Pose s; s.row = 2; s.col = 0; s.facing = EAST;
  m.setStart(s);
  m.set(2, 0, START);
  m.setGoal(2, 2);
  return m;
}

void test_forward_to_goal_wins() {
  Maze m = straightMaze();
  Program p;
  p.main.push_back(Node::command(CMD_FWD));
  p.main.push_back(Node::command(CMD_FWD));
  Interpreter it;
  it.load(&p, &m, m.startPose());
  TEST_ASSERT_EQUAL(OUT_WIN, it.runToEnd());
  TEST_ASSERT_EQUAL(2, it.pose().col);
}

void test_into_wall_bonks() {
  Maze m = straightMaze();
  m.set(2, 1, WALL);
  Program p;
  p.main.push_back(Node::command(CMD_FWD));
  Interpreter it;
  it.load(&p, &m, m.startPose());
  TEST_ASSERT_EQUAL(OUT_BONK, it.runToEnd());
  TEST_ASSERT_EQUAL(0, it.pose().col);  // didn't move
}

void test_off_board_bonks() {
  Maze m = straightMaze();
  Program p;  // facing EAST at col0, turn left -> NORTH, forward off the top? no:
  p.main.push_back(Node::command(CMD_TURN_R));  // EAST->SOUTH
  p.main.push_back(Node::command(CMD_FWD));     // row3 off-board
  Interpreter it;
  it.load(&p, &m, m.startPose());
  TEST_ASSERT_EQUAL(OUT_BONK, it.runToEnd());
}

void test_into_pit_falls() {
  Maze m = straightMaze();
  m.set(2, 1, PIT);
  Program p;
  p.main.push_back(Node::command(CMD_FWD));
  Interpreter it;
  it.load(&p, &m, m.startPose());
  TEST_ASSERT_EQUAL(OUT_FELL, it.runToEnd());
}

void test_runs_out_no_win() {
  Maze m = straightMaze();
  Program p;
  p.main.push_back(Node::command(CMD_FWD));  // to (2,1), not goal, program ends
  Interpreter it;
  it.load(&p, &m, m.startPose());
  TEST_ASSERT_EQUAL(OUT_DONE_NO_WIN, it.runToEnd());
}

void test_turns_dont_fail() {
  Maze m = straightMaze();
  Program p;
  for (int i = 0; i < 8; i++) p.main.push_back(Node::command(CMD_TURN_L));
  Interpreter it;
  it.load(&p, &m, m.startPose());
  TEST_ASSERT_EQUAL(OUT_DONE_NO_WIN, it.runToEnd());
  TEST_ASSERT_EQUAL(EAST, it.pose().facing);  // 8 left turns = full circle
}

void test_step_is_one_primitive() {
  Maze m = straightMaze();
  Program p;
  p.main.push_back(Node::command(CMD_FWD));
  p.main.push_back(Node::command(CMD_FWD));
  Interpreter it;
  it.load(&p, &m, m.startPose());
  TEST_ASSERT_EQUAL(OUT_OK, it.step());   // one move
  TEST_ASSERT_EQUAL(1, it.pose().col);
  TEST_ASSERT_EQUAL(OUT_WIN, it.step());  // second move -> goal
}

// REPEAT: a 5x1 corridor cleared with REPEAT 4 { FWD }.
void test_repeat_loop_moves_count_times() {
  Maze m;
  m.reset(1, 5);
  m.fill(FLOOR);
  Pose s; s.row = 0; s.col = 0; s.facing = EAST;
  m.setStart(s); m.set(0, 0, START); m.setGoal(0, 4);
  Program p;
  Node rep = Node::repeat(4);
  rep.body.push_back(Node::command(CMD_FWD));
  p.main.push_back(rep);
  Interpreter it;
  it.load(&p, &m, m.startPose());
  TEST_ASSERT_EQUAL(OUT_WIN, it.runToEnd());
  TEST_ASSERT_EQUAL(4, it.primCount());
}

// Functions: CALL F1 twice.
void test_function_call() {
  Maze m;
  m.reset(1, 5);
  m.fill(FLOOR);
  Pose s; s.row = 0; s.col = 0; s.facing = EAST;
  m.setStart(s); m.set(0, 0, START); m.setGoal(0, 4);
  Program p;
  p.f1.push_back(Node::command(CMD_FWD));
  p.f1.push_back(Node::command(CMD_FWD));
  p.main.push_back(Node::call(1));
  p.main.push_back(Node::call(1));
  Interpreter it;
  it.load(&p, &m, m.startPose());
  TEST_ASSERT_EQUAL(OUT_WIN, it.runToEnd());
  TEST_ASSERT_EQUAL(4, it.primCount());
}

// MAIN calls F1 then F2 — both function bodies must execute (regression: F2 used to
// be unreachable because the editor couldn't aim a call at it).
void test_call_f1_and_f2() {
  Maze m;
  m.reset(1, 5);
  m.fill(FLOOR);
  Pose s; s.row = 0; s.col = 0; s.facing = EAST;
  m.setStart(s); m.set(0, 0, START); m.setGoal(0, 4);
  Program p;
  p.f1.push_back(Node::command(CMD_FWD));
  p.f1.push_back(Node::command(CMD_FWD));
  p.f2.push_back(Node::command(CMD_FWD));
  p.f2.push_back(Node::command(CMD_FWD));
  p.main.push_back(Node::call(1));
  p.main.push_back(Node::call(2));
  Interpreter it;
  it.load(&p, &m, m.startPose());
  TEST_ASSERT_EQUAL(OUT_WIN, it.runToEnd());
  TEST_ASSERT_EQUAL(4, it.primCount());
}

// Recursive function (F1 calls F1) must terminate cleanly without exhausting the heap:
// the frame-depth cap stops the runaway and the step cap ends the run.
void test_recursive_call_is_bounded() {
  Maze m;
  m.reset(1, 3);
  m.fill(FLOOR);
  Pose s; s.row = 0; s.col = 0; s.facing = EAST;
  m.setStart(s); m.set(0, 0, START); m.setGoal(0, 2);
  Program p;
  p.f1.push_back(Node::call(1));  // F1 calls itself forever
  p.main.push_back(Node::call(1));
  Interpreter it;
  it.load(&p, &m, m.startPose(), 50);
  TEST_ASSERT_EQUAL(OUT_DONE_NO_WIN, it.runToEnd());  // no crash, no win
}

// JUMP over a single pit gap onto the goal.
void test_jump_over_pit() {
  Maze m;
  m.reset(1, 3);
  m.fill(FLOOR);
  Pose s; s.row = 0; s.col = 0; s.facing = EAST;
  m.setStart(s); m.set(0, 0, START);
  m.set(0, 1, PIT);
  m.setGoal(0, 2);
  Program p;
  p.main.push_back(Node::command(CMD_JUMP));
  Interpreter it;
  it.load(&p, &m, m.startPose());
  TEST_ASSERT_EQUAL(OUT_WIN, it.runToEnd());
}

// REPEAT_UNTIL AT_GOAL { FWD } — sense loop terminates on the goal.
void test_repeat_until_goal() {
  Maze m;
  m.reset(1, 4);
  m.fill(FLOOR);
  Pose s; s.row = 0; s.col = 0; s.facing = EAST;
  m.setStart(s); m.set(0, 0, START); m.setGoal(0, 3);
  Program p;
  Node ru = Node::repeatUntil(AT_GOAL);
  ru.body.push_back(Node::command(CMD_FWD));
  p.main.push_back(ru);
  Interpreter it;
  it.load(&p, &m, m.startPose());
  TEST_ASSERT_EQUAL(OUT_WIN, it.runToEnd());
}

// REPEAT_UNTIL that never satisfies -> step cap -> DONE_NO_WIN (SPEC §8.3).
void test_repeat_until_step_cap() {
  Maze m;
  m.reset(1, 3);
  m.fill(FLOOR);
  Pose s; s.row = 0; s.col = 0; s.facing = WEST;  // walk away from goal forever
  m.setStart(s); m.set(0, 2, START); m.setGoal(0, 0);
  // Actually face EAST into a wall-free corridor but condition WALL_AHEAD never true
  // until edge; use IF-less spin: REPEAT_UNTIL WALL_AHEAD { TURN_R } never moves.
  Program p;
  Node ru = Node::repeatUntil(PIT_AHEAD);  // no pit ahead, only turns -> spins
  ru.body.push_back(Node::command(CMD_TURN_R));
  p.main.push_back(ru);
  Interpreter it;
  it.load(&p, &m, m.startPose(), 50);
  TEST_ASSERT_EQUAL(OUT_DONE_NO_WIN, it.runToEnd());
  TEST_ASSERT_TRUE(it.primCount() >= 50);
}

void test_written_count_rewards_loops() {
  Program p;
  Node rep = Node::repeat(5);
  rep.body.push_back(Node::command(CMD_FWD));
  p.main.push_back(rep);  // written = REPEAT + FWD = 2, not 5
  TEST_ASSERT_EQUAL(2, programWrittenCount(p));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_forward_to_goal_wins);
  RUN_TEST(test_into_wall_bonks);
  RUN_TEST(test_off_board_bonks);
  RUN_TEST(test_into_pit_falls);
  RUN_TEST(test_runs_out_no_win);
  RUN_TEST(test_turns_dont_fail);
  RUN_TEST(test_step_is_one_primitive);
  RUN_TEST(test_repeat_loop_moves_count_times);
  RUN_TEST(test_function_call);
  RUN_TEST(test_call_f1_and_f2);
  RUN_TEST(test_recursive_call_is_bounded);
  RUN_TEST(test_jump_over_pit);
  RUN_TEST(test_repeat_until_goal);
  RUN_TEST(test_repeat_until_step_cap);
  RUN_TEST(test_written_count_rewards_loops);
  return UNITY_END();
}
