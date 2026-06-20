// GridBot / NeuroBot — neuroevolution: the "learn from a score, no teacher" lesson and
// the combat trainer. A population of brains runs the maze; the ones that get furthest
// breed (copy a winner + mutate). No backprop, no labels — just selection. Reuses the
// N_NEURO interpreter path to score a brain. Arduino-free, host-tested.
#pragma once
#include "game/Maze.h"
#include "game/Net.h"
#include "game/Interpreter.h"  // EnemyView

namespace gb {

constexpr int EVO_POP = 16;   // population (kept modest: 3 screens each hold one in DRAM)
constexpr int EVO_KEEP = 5;   // top survivors that get to breed

// Fitness of one brain in a maze: progress toward the goal + reach bonus − falls −
// a small efficiency cost. (Reuses the interpreter + N_NEURO; deterministic.)
float scoreBrain(const Net& brain, const Maze& m, const EnemyView* enemy, int maxSteps);

// Fitness as an ARENA FIGHTER: run a real Race match vs an AI program and score by
// outcome (win >> progress >> loss) + survival. For training bots to battle.
float scoreFighter(const Net& brain, const Maze& m, const Pose& s0, const Pose& s1,
                   const Program& ai, int maxSteps);

struct Evolve {
  Net pop[EVO_POP];
  float fit[EVO_POP] = {0};
  uint32_t rng = 1;
  int gen = 0;
  int nIn = SENSOR_COUNT_FOR_BRAIN, nHid = 8, nOut = 5;

  void init(int in, int hid, int out, uint32_t seed);
  void evaluate(const Maze& m, const EnemyView* enemy = nullptr, int maxSteps = 120);
  // score every brain as a fighter vs the AI (for the Arena trainer)
  void evaluateArena(const Maze& m, const Pose& s0, const Pose& s1, const Program& ai, int maxSteps = 200);
  void breed();                       // selection + mutation -> next generation
  void step(const Maze& m, int maxSteps = 120) { evaluate(m, nullptr, maxSteps); breed(); }

  int   bestIdx() const;
  float bestFit() const { return fit[bestIdx()]; }
  const Net& best() const { return pop[bestIdx()]; }
};

}  // namespace gb
