// Achievements: evaluate() must light up the right badges from profile state.
#include <unity.h>
#include "game/Profile.h"
#include "game/Achievements.h"

using namespace gb;

void setUp() {}
void tearDown() {}

void test_first_win_and_levels() {
  Profile p;
  TEST_ASSERT_EQUAL_UINT32(0, evaluateAchievements(p));
  p.stats.totalWins = 1;
  TEST_ASSERT_TRUE(evaluateAchievements(p) & A_FIRST_WIN);
  p.level = 10;
  TEST_ASSERT_TRUE(evaluateAchievements(p) & A_LVL10);
  p.level = 20;
  uint32_t a = evaluateAchievements(p);
  TEST_ASSERT_TRUE(a & A_LVL20);
}

void test_command_badges() {
  Profile p;
  p.stats.commandsUsed[CS_JUMP] = 1;
  p.stats.commandsUsed[CS_REPEAT] = 1;
  p.stats.commandsUsed[CS_CALL] = 1;
  p.stats.commandsUsed[CS_SENSE] = 1;
  uint32_t a = evaluateAchievements(p);
  TEST_ASSERT_TRUE(a & A_JUMP);
  TEST_ASSERT_TRUE(a & A_LOOP);
  TEST_ASSERT_TRUE(a & A_FUNC);
  TEST_ASSERT_TRUE(a & A_SENSE);
}

void test_arena_artist_stars_streak() {
  Profile p;
  p.stats.arenaWins = 1;
  p.stats.starsTotal = 60;
  p.stats.currentStreak = 11;
  p.stats.threeStarWins = 2;
  p.customChar.assign(PIX_CELLS, 0);
  p.customChar[10] = 3;  // a drawn pixel
  uint32_t a = evaluateAchievements(p);
  TEST_ASSERT_TRUE(a & A_ARENA);
  TEST_ASSERT_TRUE(a & A_COLLECT50);
  TEST_ASSERT_TRUE(a & A_STREAK5);
  TEST_ASSERT_TRUE(a & A_STREAK10);
  TEST_ASSERT_TRUE(a & A_THREE_STAR);
  TEST_ASSERT_TRUE(a & A_ARTIST);
}

void test_neurobot_badges() {
  Profile p;
  TEST_ASSERT_FALSE(evaluateAchievements(p) & (A_NEURO | A_NEURO_WIN | A_FIGHTER));
  p.stats.brainsTrained = 1;
  TEST_ASSERT_TRUE(evaluateAchievements(p) & A_NEURO);
  p.stats.neuroWins = 1;
  TEST_ASSERT_TRUE(evaluateAchievements(p) & A_NEURO_WIN);
  p.stats.fightersSaved = 1;
  TEST_ASSERT_TRUE(evaluateAchievements(p) & A_FIGHTER);
  // the Generalist prize: only when the whole gauntlet is cleared
  TEST_ASSERT_FALSE(evaluateAchievements(p) & A_GENERALIST);
  p.stats.gauntletBest = (uint8_t)(GAUNTLET_MAZES - 1);
  TEST_ASSERT_FALSE(evaluateAchievements(p) & A_GENERALIST);
  p.stats.gauntletBest = (uint8_t)GAUNTLET_MAZES;
  TEST_ASSERT_TRUE(evaluateAchievements(p) & A_GENERALIST);
  // names exist for the new bits (13..16)
  TEST_ASSERT_EQUAL_STRING("Brainiac", achievementName(13));
  TEST_ASSERT_EQUAL_STRING("Battle-Ready", achievementName(15));
  TEST_ASSERT_EQUAL_STRING("Generalist", achievementName(16));
}

void test_count() {
  TEST_ASSERT_EQUAL(0, achievementCount(0));
  TEST_ASSERT_EQUAL(3, achievementCount(A_FIRST_WIN | A_JUMP | A_LOOP));
  TEST_ASSERT_EQUAL(17, ACH_COUNT);
}

// The NeuroBot training tools unlock progressively AFTER the brain, one at a time,
// each paced a few levels apart (so the L28 graduation isn't a firehose).
void test_neuro_tool_progression() {
  using namespace gb;
  // before the brain: nothing neural
  Unlocks before = computeUnlocks(27);
  TEST_ASSERT_FALSE(before.neuro);
  TEST_ASSERT_FALSE(before.nDraw);
  TEST_ASSERT_FALSE(before.nEvolve);
  TEST_ASSERT_FALSE(before.nPilot);
  TEST_ASSERT_FALSE(before.nRnn);
  // brain (Teach) arrives at 28, but the other tools are still locked
  Unlocks u28 = computeUnlocks(28);
  TEST_ASSERT_TRUE(u28.neuro);
  TEST_ASSERT_FALSE(u28.nDraw);
  TEST_ASSERT_FALSE(u28.nPilot);
  // then one tool at a time
  TEST_ASSERT_TRUE(computeUnlocks(31).nDraw);
  TEST_ASSERT_FALSE(computeUnlocks(31).nEvolve);
  TEST_ASSERT_TRUE(computeUnlocks(34).nEvolve);
  TEST_ASSERT_FALSE(computeUnlocks(34).nPilot);
  TEST_ASSERT_TRUE(computeUnlocks(37).nPilot);
  TEST_ASSERT_FALSE(computeUnlocks(37).nRnn);
  TEST_ASSERT_TRUE(computeUnlocks(40).nRnn);
  // sticky: everything stays unlocked at higher levels
  Unlocks late = computeUnlocks(99);
  TEST_ASSERT_TRUE(late.neuro && late.nDraw && late.nEvolve && late.nPilot && late.nRnn);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_first_win_and_levels);
  RUN_TEST(test_command_badges);
  RUN_TEST(test_arena_artist_stars_streak);
  RUN_TEST(test_neurobot_badges);
  RUN_TEST(test_neuro_tool_progression);
  RUN_TEST(test_count);
  return UNITY_END();
}
