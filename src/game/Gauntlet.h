// GridBot / NeuroBot — the "Generalist" challenge. A frozen brain can't plan a whole
// campaign maze (it only senses what's nearby), but it CAN learn a reactive strategy
// that clears many *open* mazes it has never seen. The Gauntlet is a fixed set of such
// reactive-solvable mazes; the "Generalize" trainer teaches the brain a reactive teacher
// across a separate training set, and the prize is clearing the held-out test set.
// Platform-agnostic (host-tested).
#pragma once
#include <stdint.h>
#include "game/Net.h"

namespace gb {

class Maze;

constexpr int GAUNTLET_MAZES = 10;  // held-out test set size = the Generalist prize bar

// Fill `out` with gauntlet maze `idx`. `train`=true draws from the (disjoint) training
// space, false from the held-out test space. Every returned maze is reactive-solvable.
bool gauntletMaze(Maze& out, int idx, bool train);

// One "Generalize" batch: distill `brain` (in place) to imitate a reactive navigator
// across the training mazes. Call repeatedly to keep improving the same brain.
void gauntletTrain(Net& brain, int epochs);

// How many of the GAUNTLET_MAZES held-out test mazes the FROZEN `brain` clears (0..N).
int gauntletRun(const Net& brain);

}  // namespace gb
