// GridBot / NeuroBot — tabular Q-learning: the "learn from reward" lesson. A grid MDP
// (state = cell, 4 move actions N/E/S/W) where the robot tries, fails, and updates a
// value table until a policy emerges — no code, no teacher, just consequences. The big
// visual payoff: value spreads backward from the battery. Arduino-free, host-tested.
#pragma once
#include "game/Maze.h"

namespace gb {

struct QLearn {
  const Maze* m = nullptr;
  float Q[MAZE_MAX_CELLS][4] = {{0}};   // value of taking action a in cell s
  float alpha = 0.5f, gamma = 0.9f, epsilon = 0.2f;
  uint32_t rng = 1;
  int episodes = 0;

  void init(const Maze* maze, uint32_t seed);

  // Run one episode from the maze start (try until goal/pit/step-cap), updating Q via
  // the Bellman rule. Returns true if it reached the goal this episode.
  bool runEpisode(int maxSteps = 200);

  int   bestAction(int r, int c) const;  // greedy action (for the policy arrows)
  float maxQ(int r, int c) const;        // value of a cell (for the heatmap)
  // Follow the greedy policy from start; true if it reaches the goal (learned to solve).
  bool greedySolves(int maxSteps = 200) const;
};

}  // namespace gb
