// GridBot / NeuroBot — distillation: teach a brain to imitate an algorithm by backprop.
// The design's PRIMARY trainer ("learn from a teacher"): the BFS solver provides perfect
// (egocentric sensors -> action) labels for the maze, and the brain is trained to copy
// them. The headline lesson lands here — the solver is correct by construction; the brain
// only approximates it, so it may fumble an unseen maze. Arduino-free, host-tested.
#pragma once
#include "game/Maze.h"
#include "game/Net.h"

namespace gb {

// Train `brain` to imitate the optimal solver on `m` (re-simulates each epoch; no large
// buffers, so it fits the ESP32 stack). Returns false if the maze can't be solved.
bool distillSolver(Net& brain, const Maze& m, bool allowJump, int epochs);

}  // namespace gb
