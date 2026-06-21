// GridBot / NeuroBot — distillation: teach a brain to imitate an algorithm by backprop.
// The design's PRIMARY trainer ("learn from a teacher"): the BFS solver provides perfect
// (egocentric sensors -> action) labels for the maze, and the brain is trained to copy
// them. The headline lesson lands here — the solver is correct by construction; the brain
// only approximates it, so it may fumble an unseen maze. Arduino-free, host-tested.
#pragma once
#include "game/Maze.h"
#include "game/Net.h"
#include "game/RNet.h"

namespace gb {

// Train `brain` to imitate the optimal solver on `m` (re-simulates each epoch; no large
// buffers, so it fits the ESP32 stack). Returns false if the maze can't be solved.
bool distillSolver(Net& brain, const Maze& m, bool allowJump, int epochs);

// Turn an ordered list of tile indices (row*cols+col), starting at the maze's start tile,
// into a teacher program of turns + forward/jump that walks that route. Returns false if
// any consecutive pair isn't a single straight forward step or a straight jump-of-2.
struct Program;  // fwd-decl (game/Program.h)
bool pathToProgram(const Maze& m, const uint8_t* tiles, int n, Program& out);

// Imitation learning by demonstration: distill `brain` to copy a KID-DRAWN path instead
// of the auto-solver. `brain` is trained in place, so passing an already-trained brain
// fine-tunes it on the new path. Returns false if the path isn't walkable.
bool distillPath(Net& brain, const Maze& m, const uint8_t* tiles, int n, int epochs);

// Train a RECURRENT brain (BPTT) to imitate the memory-using explorer across campaign
// mazes 1..levels — a generally-capable memory brain to drop into a program. In place,
// so calling again keeps improving it.
void rnnTrainGeneral(RNet& brain, uint32_t seedBase, int levels, int epochs);

}  // namespace gb
