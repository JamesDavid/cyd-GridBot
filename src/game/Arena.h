// GridBot — Arena match engine (SPEC §18). N=2 interpreter contexts over one shared
// Maze, advanced one primitive per tick, then a resolution pass. Fully deterministic
// (NO RNG during play, SPEC §18.1) so matches replay byte-identically. Race first;
// Sumo/Zap (PUSH/FIRE verbs) layer on later via a match-type module.
#pragma once
#include "game/Maze.h"
#include "game/Program.h"
#include "game/Interpreter.h"

namespace gb {

enum class MatchType : uint8_t { RACE, SUMO };  // ZAP (FIRE verb) -> backlog
enum class ArenaOutcome : uint8_t { RUNNING, BOT0, BOT1, DRAW };

constexpr int ARENA_STEP_CAP = 300;

class Arena {
 public:
  // Programs and starts are referenced, not owned; keep them alive for the match.
  void setup(const Maze* maze, const Program* p0, const Program* p1,
             const Pose& s0, const Pose& s1, MatchType type = MatchType::RACE,
             int stepCap = ARENA_STEP_CAP);

  // Advance one tick (both live bots one primitive + resolution). Returns the
  // running/terminal outcome.
  ArenaOutcome tick();

  ArenaOutcome run();             // tick to completion (tests / spectate)
  uint32_t logHash() const { return _hash; }  // determinism fingerprint

  const Pose& pose(int i) const { return _bot[i].it.pose(); }
  bool alive(int i) const { return _bot[i].alive; }
  bool won(int i) const { return _bot[i].won; }
  ArenaOutcome outcome() const { return _outcome; }
  int ticks() const { return _ticks; }

 private:
  struct Bot {
    Interpreter it;
    Pose start;
    EnemyView enemy;
    bool alive = true;
    bool won = false;
    bool done = false;  // program ended without winning
  };
  void foldLog();

  const Maze* _maze = nullptr;
  MatchType _type = MatchType::RACE;
  Bot _bot[2];
  int _ticks = 0;
  int _stepCap = ARENA_STEP_CAP;
  ArenaOutcome _outcome = ArenaOutcome::RUNNING;
  uint32_t _hash = 2166136261u;  // FNV-ish running fingerprint of the match log
};

}  // namespace gb
