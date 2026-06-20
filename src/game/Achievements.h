// GridBot — achievements / badges. A sticky bitmask in the profile. evaluate()
// derives which badges SHOULD be unlocked from current stats; the caller ORs it in
// and celebrates any newly-set bits. Platform-agnostic (host-testable).
#pragma once
#include "game/Profile.h"

namespace gb {

enum Achievement : uint32_t {
  A_FIRST_WIN  = 1u << 0,   // win your first level
  A_THREE_STAR = 1u << 1,   // earn 3 stars on a level
  A_JUMP       = 1u << 2,   // use a Jump
  A_LOOP       = 1u << 3,   // use a Repeat loop
  A_FUNC       = 1u << 4,   // use a Function
  A_SENSE      = 1u << 5,   // clear a sensing level
  A_ARENA      = 1u << 6,   // win an arena match
  A_STREAK5    = 1u << 7,   // 5-win streak
  A_STREAK10   = 1u << 8,   // 10-win streak
  A_LVL10      = 1u << 9,   // reach level 10
  A_LVL20      = 1u << 10,  // reach level 20
  A_ARTIST     = 1u << 11,  // draw a custom sprite
  A_COLLECT50  = 1u << 12,  // earn 50 total stars
  ACH_COUNT    = 13
};

inline const char* achievementName(int bit) {
  static const char* N[ACH_COUNT] = {
    "First Steps", "Bright Spark", "Hopper", "Looper", "Architect",
    "Sixth Sense", "Champion", "On Fire", "Unstoppable", "Explorer",
    "Veteran", "Artist", "Star Collector"};
  return (bit >= 0 && bit < ACH_COUNT) ? N[bit] : "?";
}

// Which badges the profile has earned given its current state.
inline uint32_t evaluateAchievements(const Profile& p) {
  const Stats& s = p.stats;
  uint32_t a = 0;
  if (s.totalWins >= 1)               a |= A_FIRST_WIN;
  if (s.threeStarWins >= 1)           a |= A_THREE_STAR;
  if (s.commandsUsed[CS_JUMP] > 0)    a |= A_JUMP;
  if (s.commandsUsed[CS_REPEAT] > 0)  a |= A_LOOP;
  if (s.commandsUsed[CS_CALL] > 0)    a |= A_FUNC;
  if (s.commandsUsed[CS_SENSE] > 0)   a |= A_SENSE;
  if (s.arenaWins >= 1)               a |= A_ARENA;
  if (s.currentStreak >= 5)           a |= A_STREAK5;
  if (s.currentStreak >= 10)          a |= A_STREAK10;
  if (p.level >= 10)                  a |= A_LVL10;
  if (p.level >= 20)                  a |= A_LVL20;
  if (!p.customChar.empty() || !p.customGoal.empty()) a |= A_ARTIST;
  if (s.starsTotal >= 50)             a |= A_COLLECT50;
  return a;
}

inline int achievementCount(uint32_t mask) {
  int n = 0;
  for (int i = 0; i < ACH_COUNT; i++) if (mask & (1u << i)) n++;
  return n;
}

}  // namespace gb
