// GridBot — per-kid profile model (SPEC §5.1, §9, §11.1). Plain data; JSON
// (de)serialization + LittleFS live in ProfileStore (Phase 3). The engine never
// depends on this — profiles are a shell concern, but the struct is platform-agnostic
// so unlock logic can be unit-tested on the host.
#pragma once
#include <string>
#include <vector>
#include <cstdio>
#include "game/Program.h"

namespace gb {

// Command histogram buckets for stats (SPEC §9). (Backward was removed from the game.)
enum CmdStat : uint8_t { CS_FWD, CS_TURN, CS_JUMP, CS_REPEAT, CS_CALL, CS_SENSE, CS_ZAP, CS_NEURO, CS_COUNT };

struct Unlocks {
  bool jump = false;
  bool repeat = false;
  bool func = false;
  bool sense = false;
  bool neuro = false;     // NeuroBot: train a brain (Teach) and drop it in your program
  // NeuroBot training tools unlock progressively after the brain (paced like the blocks):
  bool nDraw = false;     // tag a path (Draw) and train to it
  bool nEvolve = false;   // neuroevolution
  bool nPilot = false;    // planner + follower (brain mode + trainer)
  bool nRnn = false;      // recurrent memory brain
};

struct Settings {
  bool sound = true;
  uint8_t animSpeed = 1;   // 0=slow,1=med,2=fast
  bool miniMap = false;    // optional code-view mini-map (default off, SPEC §10)
  bool music = true;       // background music on (sound modal)
  bool sfx = true;         // sound effects on   (sound modal)
  uint8_t volume = 2;      // 0 mute .. 3 loud    (sound modal)
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
  uint32_t arenaWins = 0;
  uint32_t threeStarWins = 0;
  uint16_t commandsUsed[CS_COUNT] = {0};
  // NeuroBot
  uint16_t brainsTrained = 0;   // brains trained & used/saved
  uint16_t neuroWins = 0;       // levels cleared with a NEURO brain in the program
  uint16_t fightersSaved = 0;   // brains saved as Arena fighters
  uint8_t  gauntletBest = 0;    // most consecutive campaign levels a frozen brain cleared
};

// Where a saved bot came from (shown in the library / "My Bots" manager).
enum LibSource : uint8_t { LIB_UNKNOWN, LIB_CODE, LIB_NEURO, LIB_BRAINCAM, LIB_ARENA, LIB_RADIO };

struct LibEntry {
  std::string name;
  Program program;
  uint8_t source = LIB_UNKNOWN;  // code editor / neuro trainer / Brain Cam / arena / radio
  uint16_t srcLevel = 0;         // campaign level it was saved from (code editor); 0 = n/a
};

// Per-level best result, for the level-select "go for gold" browser. Tiny on purpose: the best
// PROGRAM is kept in a separate flash file (ProfileStore::saveLevelProgram), loaded only when a kid
// reopens that level to revise -- so a profile holds these 3 bytes/level, not 40+ programs in RAM.
struct LevelRec {
  uint8_t stars = 0;       // best stars earned (0-3)
  uint8_t bestBlocks = 0;  // fewest blocks used to win (0 = not completed yet)
  uint8_t par = 0;         // the maze's shortest-solution length (for the "best N / par M" display)
};

struct Profile {
  std::string id;          // "u01" (device-local filename id)
  std::string uuid;        // stable global id for friend sync across devices
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

  // Per-level best result, index = level-1 (grows as you climb). Drives the level-select browser.
  std::vector<LevelRec> levelRecs;

  const LevelRec* levelRec(uint32_t level) const {
    return (level >= 1 && (size_t)level <= levelRecs.size()) ? &levelRecs[level - 1] : nullptr;
  }
  // Fold a win into the per-level record. Returns true if this run is a NEW best by block count
  // (so the caller should overwrite that level's saved best-program file).
  bool recordLevelResult(uint32_t level, uint8_t stars, uint8_t blocks, uint8_t par) {
    if (level < 1) return false;
    if (levelRecs.size() < (size_t)level) levelRecs.resize(level);
    LevelRec& r = levelRecs[level - 1];
    if (par) r.par = par;
    bool newBest = blocks > 0 && (r.bestBlocks == 0 || blocks < r.bestBlocks);
    if (stars > r.stars) r.stars = stars;
    if (newBest) r.bestBlocks = blocks;
    return newBest;
  }

  // Optional custom 16x16 sprites drawn in the pixel editor (palette indices,
  // 0 = empty). Empty vector = use the roster art for this avatar. Shareable.
  std::vector<uint8_t> customChar;
  std::vector<uint8_t> customGoal;

  uint32_t achievements = 0;  // bitmask of earned badges (see Achievements.h)
  uint32_t coins = 0;         // spendable currency (collected in levels)

  // Onboarding: bitmask of which intro lessons have been auto-played once at their unlock level
  // (bit 0 = Move @ L1, bits 1-4 = Jump/Repeat/Sense/Functions as each power unlocks). The lesson
  // auto-plays the first time the kid reaches the level that introduces it, then never again; the
  // intro card's "Learn it >" button still replays it on demand. See App::lessonBit / introLessonFor.
  uint32_t tutorialsSeen = 0;

  // Shop: a custom robot tint (0 = use roster colour) + an equipped emoji (0 = none),
  // and bitmasks of what's been purchased.
  uint8_t shopColor = 0;      // index into ShopColors (1-based; 0 = none)
  uint8_t shopEmoji = 0;      // index into ShopEmojis (1-based; 0 = none)
  uint32_t ownedColors = 0;   // bitmask of purchased colours
  uint32_t ownedEmojis = 0;   // bitmask of purchased emojis
};

constexpr int PIX_DIM = 16;
constexpr int PIX_CELLS = PIX_DIM * PIX_DIM;

// Unlock gating by level (SPEC §7). Sticky: once reached, stays unlocked. This is
// the single source of truth; ProfileStore persists the resulting flags.
inline Unlocks computeUnlocks(uint32_t level) {
  // Compressed curve so the conceptual payoff (sensing, SPEC §7.1) arrives soon —
  // the whole toolset is unlocked within ~22 levels rather than 55.
  Unlocks u;
  u.jump     = level >= 6;
  u.repeat   = level >= 10;
  u.sense    = level >= 15;  // if / repeat-until: the wall-follower payoff, taught first
  u.func     = level >= 20;  // functions come AFTER sensing, so you can wrap if-logic in one
  u.neuro    = level >= 28;  // graduation: train a brain (Teach), a few levels after sensing
  u.nDraw    = level >= 31;  // then the training tools arrive one at a time, with a lesson each
  u.nEvolve  = level >= 34;
  u.nPilot   = level >= 37;
  u.nRnn     = level >= 40;
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

// Auto-name a freshly-saved bot by where it came from (+ level for code/brain), with a " vN"
// suffix to keep it unique: "code L8", then "code L8 v2"...; "fighter"; "braincam".
inline std::string autoLibName(const Profile& p, uint8_t source, uint16_t level,
                               const char* baseOverride = nullptr) {
  char base[24];
  if (baseOverride && baseOverride[0]) {   // caller-chosen noun (e.g. Arena: runner/fighter/player)
    snprintf(base, sizeof(base), "%s", baseOverride);
  } else switch (source) {
    case LIB_CODE:     snprintf(base, sizeof(base), "code L%u", (unsigned)level); break;
    case LIB_NEURO:    snprintf(base, sizeof(base), "brain L%u", (unsigned)level); break;
    case LIB_BRAINCAM: snprintf(base, sizeof(base), "braincam"); break;
    case LIB_ARENA:    snprintf(base, sizeof(base), "fighter"); break;
    default:           snprintf(base, sizeof(base), "bot"); break;
  }
  auto taken = [&](const char* nm) {
    for (const auto& e : p.library) if (e.name == nm) return true;
    return false;
  };
  if (!taken(base)) return std::string(base);
  for (int v = 2; v < 100; v++) {
    char c[32]; snprintf(c, sizeof(c), "%s v%d", base, v);
    if (!taken(c)) return std::string(c);
  }
  return std::string(base);
}

}  // namespace gb
