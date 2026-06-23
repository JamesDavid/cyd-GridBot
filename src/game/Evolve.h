// GridBot / NeuroBot — neuroevolution: the "learn from a score, no teacher" lesson and
// the combat trainer. A population of brains runs the maze; the ones that get furthest
// breed (copy a winner + mutate). No backprop, no labels — just selection. Reuses the
// N_NEURO interpreter path to score a brain. Arduino-free, host-tested.
#pragma once
#include "game/Maze.h"
#include "game/Net.h"
#include "game/Interpreter.h"  // EnemyView
#include "game/Arena.h"        // MatchType

namespace gb {

constexpr int EVO_POP = 16;   // population (kept modest: 3 screens each hold one in DRAM)
constexpr int EVO_KEEP = 5;   // top survivors that get to breed

// Fitness of one brain in a maze: progress toward the goal + reach bonus − falls −
// a small efficiency cost. (Reuses the interpreter + N_NEURO; deterministic.)
float scoreBrain(const Net& brain, const Maze& m, const EnemyView* enemy, int maxSteps);

// Fitness as an ARENA FIGHTER vs an AI program. RACE = score by progress/win to the goal;
// SUMO = score by knocking the enemy out + surviving + hunting it down (no goal).
float scoreFighter(const Net& brain, const Maze& m, const Pose& s0, const Pose& s1,
                   const Program& ai, int maxSteps, MatchType type = MatchType::RACE);

struct Evolve {
  Net pop[EVO_POP];
  float fit[EVO_POP] = {0};
  uint32_t rng = 1;
  int gen = 0;
  int nIn = SENSOR_COUNT_FOR_BRAIN, nHid = 8, nOut = 5;

  void init(int in, int hid, int out, uint32_t seed);
  void evaluate(const Maze& m, const EnemyView* enemy = nullptr, int maxSteps = 120);
  // score every brain as a fighter vs the AI (for the Arena trainer)
  void evaluateArena(const Maze& m, const Pose& s0, const Pose& s1, const Program& ai,
                     int maxSteps = 200, MatchType type = MatchType::RACE);
  // selection + mutation -> next generation. `rate`/`scale` tune mutation (how many weights
  // change, and by how much); defaults match the classic combat trainer. The Arena trainer's
  // "Explore" knob raises `scale` to mutate harder (more variety, less stable).
  void breed(float rate = 0.15f, float scale = 0.6f);
  void step(const Maze& m, int maxSteps = 120) { evaluate(m, nullptr, maxSteps); breed(); }

  int   bestIdx() const;
  float bestFit() const { return fit[bestIdx()]; }
  const Net& best() const { return pop[bestIdx()]; }
};

}  // namespace gb
