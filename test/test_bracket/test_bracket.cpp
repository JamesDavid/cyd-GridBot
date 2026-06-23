// Single-elimination bracket scheduler tests (the networked-tournament core).
#include <unity.h>
#include "game/Bracket.h"

using namespace gb;

void setUp() {}
void tearDown() {}

// Run a whole bracket where, in every match, the LOWER player id wins. Player 0 should always
// reach and win the final, and the round count must be ceil(log2(n)).
static int playOut(int numPlayers, int& rounds) {
  Bracket b; b.init(numPlayers);
  rounds = 0;
  while (!b.done()) {
    for (int m = 0; m < b.matchCount(); m++) {
      int a, x; b.pair(m, a, x);
      if (x < 0) continue;                 // bye: auto-advances 'a'
      b.reportWinner(m, a < x ? a : x);    // lower id wins
    }
    TEST_ASSERT_TRUE(b.roundComplete());
    b.advance();
    rounds++;
  }
  return b.champion();
}

void test_bracket_power_of_two() {
  int r; int champ = playOut(8, r);
  TEST_ASSERT_EQUAL(0, champ);     // lowest id wins everything
  TEST_ASSERT_EQUAL(3, r);         // 8 -> 4 -> 2 -> 1
}

void test_bracket_byes_five() {
  Bracket b; b.init(5);
  TEST_ASSERT_EQUAL(8, b.nSlots);
  // round 0: exactly one real match (players 0 vs 1); players 2,3,4 get byes; NO bye-vs-bye
  int real = 0, byes = 0, both = 0;
  for (int m = 0; m < b.matchCount(); m++) {
    int a, x; b.pair(m, a, x);
    if (a < 0 && x < 0) both++;
    else if (x < 0) byes++;
    else real++;
  }
  TEST_ASSERT_EQUAL(0, both);     // never two byes in one match
  TEST_ASSERT_EQUAL(1, real);
  TEST_ASSERT_EQUAL(3, byes);
  int r; int champ = playOut(5, r);
  TEST_ASSERT_EQUAL(0, champ);
  TEST_ASSERT_EQUAL(3, r);        // 5 -> 4 -> 2 -> 1
}

void test_bracket_three() {
  int r; int champ = playOut(3, r);
  TEST_ASSERT_EQUAL(0, champ);
  TEST_ASSERT_EQUAL(2, r);        // 3 -> 2 -> 1
}

void test_bracket_one_player() {
  Bracket b; b.init(1);
  TEST_ASSERT_TRUE(b.done());
  TEST_ASSERT_EQUAL(0, b.champion());
}

// A non-trivial winner: in 8-player, make player 5 win all its matches and confirm it's champ.
void test_bracket_specific_winner() {
  Bracket b; b.init(8);
  while (!b.done()) {
    for (int m = 0; m < b.matchCount(); m++) {
      int a, x; b.pair(m, a, x);
      if (x < 0) continue;
      // player 5 always wins; otherwise the higher id wins (arbitrary but deterministic)
      int w = (a == 5 || x == 5) ? 5 : (a > x ? a : x);
      b.reportWinner(m, w);
    }
    b.advance();
  }
  TEST_ASSERT_EQUAL(5, b.champion());
}

void test_bracket_no_byes_at_16() {
  Bracket b; b.init(16);
  TEST_ASSERT_EQUAL(16, b.nSlots);
  for (int m = 0; m < b.matchCount(); m++) {
    int a, x; b.pair(m, a, x);
    TEST_ASSERT_TRUE(a >= 0 && x >= 0);   // a full field has no byes
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_bracket_power_of_two);
  RUN_TEST(test_bracket_byes_five);
  RUN_TEST(test_bracket_three);
  RUN_TEST(test_bracket_one_player);
  RUN_TEST(test_bracket_specific_winner);
  RUN_TEST(test_bracket_no_byes_at_16);
  return UNITY_END();
}
