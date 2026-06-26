// GridBot — Arena match engine (SPEC §18). N=2 interpreter contexts over one shared
// Maze, advanced one primitive per tick, then a resolution pass. Fully deterministic
// (NO RNG during play, SPEC §18.1) so matches replay byte-identically. Two match
// types: Race (first to the goal) and Sumo (zap a rival into a pit / off the board).
#pragma once
#include "game/Maze.h"
#include "game/Program.h"
#include "game/Interpreter.h"

namespace gb {

enum class MatchType : uint8_t { RACE, SUMO, SOCCER };  // ZAP (FIRE verb) -> backlog
enum class ArenaOutcome : uint8_t { RUNNING, BOT0, BOT1, DRAW };

constexpr int ARENA_STEP_CAP = 300;
// Sumo health: a zap that connects costs 1 HP; lose all HP (or get shoved off the ring) -> out.
// HP slowly regenerates, so a match is a sustained brawl of several hits, not one shove.
constexpr int SUMO_HP = 4;
constexpr int SUMO_REGEN_TICKS = 28;  // +1 HP this often (if below max and still alive)
// After landing a zap a bot must recover before it can zap again -- this stops one bot from
// chaining 3 hits uncontested and gives the foe a window to re-engage and hit BACK, so the
// health bars trade down together (a real back-and-forth brawl, not a one-sided beatdown).
constexpr int SUMO_ZAP_COOLDOWN = 3;

class Arena {
 public:
  // Programs and starts are referenced, not owned; keep them alive for the match.
  void setup(const Maze* maze, const Program* p0, const Program* p1,
             const Pose& s0, const Pose& s1, MatchType type = MatchType::RACE,
             int stepCap = ARENA_STEP_CAP);

  // Soccer: place the ball and each bot's TARGET goal (bot i scores by pushing the ball onto
  // goal[i]). Call AFTER setup() with MatchType::SOCCER. Both bots sense the ball as their target
  // (slots 4-6) and their own goal as the "enemy" bearing (slots 7-9), then learn to shove it home.
  void configSoccer(const Pose& ball, const Pose& goal0, const Pose& goal1);
  const Pose& ball() const { return _ball; }
  const Pose& goal(int i) const { return _goal[i]; }

  // Advance one tick (both live bots one primitive + resolution). Returns the
  // running/terminal outcome.
  ArenaOutcome tick();

  ArenaOutcome run();             // tick to completion (tests / spectate)
  uint32_t logHash() const { return _hash; }  // determinism fingerprint

  const Pose& pose(int i) const { return _bot[i].it.pose(); }
  bool alive(int i) const { return _bot[i].alive; }
  int hp(int i) const { return _bot[i].hp; }   // Sumo health (for the health bar)
  bool won(int i) const { return _bot[i].won; }
  int goals(int i) const { return _goals[i]; }  // Soccer: goals scored by bot i (the scoreline)
  bool justScored() const { return _justScored; }  // a goal landed THIS tick (for a "GOAL!" burst)
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
    int hp = SUMO_HP;   // Sumo only
    int zapCd = 0;      // ticks until this bot's zap is ready again (Sumo)
  };
  void foldLog();

  // Soccer state (deterministic, folded into the hash like every other field). A soccer match is
  // TIMED, not sudden-death: a goal increments the scorer's tally and re-kicks off (ball + bots to
  // their starts); the higher score at the cap wins (level -> ball-position tiebreak, then draw).
  bool pushBall(int mover, bool* squeezed = nullptr);   // bot `mover` stands on the ball -> shove it;
                              // false if blocked; *squeezed = true if it popped out sideways into a wall
  void kickoff();             // reset the ball to centre + both bots to their starts (after a goal)
  Pose _ball;                 // ball tile
  Pose _kickoff;              // the centre kickoff tile (ball returns here after each goal)
  Pose _goal[2];              // goal[i] = the tile bot i is trying to push the ball onto
  int8_t _goals[2] = {0, 0};  // goals scored by each bot (the scoreline)
  bool _justScored = false;   // a goal landed this tick (one-frame flag for the "GOAL!" burst)
  int16_t _ballStall = 0;     // soccer: ticks the ball has sat untouched (a "loose ball" timer)
  void refDriftBall();        // drop a long-stalled ball at a fresh (deterministic) spot to scramble for

  const Maze* _maze = nullptr;
  MatchType _type = MatchType::RACE;
  Bot _bot[2];
  int _ticks = 0;
  int _stepCap = ARENA_STEP_CAP;
  ArenaOutcome _outcome = ArenaOutcome::RUNNING;
  uint32_t _hash = 2166136261u;  // FNV-ish running fingerprint of the match log
};

}  // namespace gb
