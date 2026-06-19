// GridBot — per-kid profile model (SPEC §5.1, §9, §11.1). Plain data; JSON
// (de)serialization + LittleFS live in ProfileStore (Phase 3). The engine never
// depends on this — profiles are a shell concern, but the struct is platform-agnostic
// so unlock logic can be unit-tested on the host.
#pragma once
#include <string>
#include <vector>
#include "game/Program.h"

namespace gb {

// Command histogram buckets for stats (SPEC §9).
enum CmdStat : uint8_t { CS_FWD, CS_BACK, CS_TURN, CS_JUMP, CS_REPEAT, CS_CALL, CS_SENSE, CS_COUNT };

struct Unlocks {
  bool backward = false;  // Tier 1.5
  bool jump = false;
  bool repeat = false;
  bool func = false;
  bool sense = false;
};

struct Settings {
  bool sound = true;
  uint8_t animSpeed = 1;   // 0=slow,1=med,2=fast
  bool miniMap = false;    // optional code-view mini-map (default off, SPEC §10)
};

struct Stats {
  uint32_t levelsCompleted = 0;
  uint32_t totalRuns = 0;
  uint32_t totalWins = 0;
  uint32_t bonks = 0;
  uint32_t falls = 0;
  uint32_t starsTotal = 0;
  uint32_t totalPlaySeconds = 0;
  uint32_t currentStreak = 0;        // consecutive first-try wins
  uint16_t commandsUsed[CS_COUNT] = {0};
};

struct LibEntry {
  std::string name;
  Program program;
};

struct Profile {
  std::string id;          // "u01"
  std::string name = "Player";
  uint8_t avatar = 0;      // roster index (SPEC §4.1)
  uint32_t level = 1;      // current/highest level
  uint32_t seedBase = 0;   // per-profile RNG salt
  Unlocks unlocks;
  Settings settings;
  Stats stats;

  // Per-level resume slot (SPEC §11.1).
  uint32_t workLevel = 0;
  Program work;

  // Named, reusable solutions (SPEC §11.1). Bounded ~8-12.
  std::vector<LibEntry> library;
};

// Unlock gating by level (SPEC §7). Sticky: once reached, stays unlocked. This is
// the single source of truth; ProfileStore persists the resulting flags.
inline Unlocks computeUnlocks(uint32_t level) {
  Unlocks u;
  u.backward = level >= 4;
  u.jump     = level >= 9;
  u.repeat   = level >= 15;
  u.func     = level >= 25;
  u.sense    = level >= 55;
  return u;
}

// Animation step interval in ms from the settings knob (SPEC §8.3, 250-600ms).
inline uint16_t animStepMs(const Settings& s) {
  switch (s.animSpeed) {
    case 0:  return 600;
    case 2:  return 250;
    default: return 400;
  }
}

constexpr int LIBRARY_MAX = 12;

}  // namespace gb
