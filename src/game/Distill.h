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

// Teach a brain to BATTLE (Sumo): imitate the hunt-and-zap policy -- turn toward the foe, zap
// when it's in the tile ahead, dodge walls/pits, else charge in -- by backprop over random foe
// placements on a battle ring. One call = a competent fighter instantly (the "Teach" button in
// Battle), where the goal is "minimise enemy distance, then zap". In place; call again to refine.
bool distillHunter(Net& brain, uint32_t seed, int epochs, bool jitterFoe = false);

// As distillHunter, but teaches a RECURRENT brain (BPTT over each chase episode) -- the RNN
// variant of the Battle "Teach", so the brain-type toggle can train a memory brain to fight.
bool distillHunterRnn(RNet& brain, uint32_t seed, int episodes);

// Q-LEARNING (reinforcement, no teacher): the brain's 5 sigmoid outputs are Q-values; we play
// simplified battle episodes (the learner moves, the foe stands) and nudge Q(s,a) toward
// r + gamma*max Q(s'). Reward = land a zap on an adjacent foe (+1, a win); ring yourself out
// (0, a loss); plus a small shaping bonus for closing the distance. Discovers "minimise enemy
// distance, then zap" purely from reward -- the trainer's "Q-Learn" engine, distinct from the
// imitation Teach and the population-based Evolve. In place; call again to keep learning.
//
// `globalDone`/`globalTotal` place this batch within a longer training run so exploration (epsilon)
// decays SMOOTHLY across all of it -- the UI runs Q-Learn as several animated chunks, and a fresh
// epsilon ramp per chunk would keep re-randomising the policy and never converge. Defaults make a
// single call self-contained (decay over its own `episodes`).
// `epsScale` (default 1.0) multiplies the exploration rate -- the Arena trainer's "Explore" knob:
// >1 tries more random moves (slower to converge, escapes ruts), 0 = greedy from the start.
bool qTrainHunter(Net& brain, uint32_t seed, int episodes, int globalDone = 0, int globalTotal = 0,
                  float epsScale = 1.0f);

// RECURRENT Q-learning: the same reward MDP as qTrainHunter, but the policy is a memory brain
// (RNet) and each episode is learned by unrolled semi-gradient TD (RNet::trainEpisodeQ). Slower
// and noisier than the feedforward version (BPTT + bootstrapping), but it's true reward-driven
// training of a recurrent fighter. In place; `globalDone`/`globalTotal` decay epsilon across chunks.
bool qTrainHunterRnn(RNet& brain, uint32_t seed, int episodes, int globalDone = 0, int globalTotal = 0,
                     float epsScale = 1.0f);

// RECURRENT Q-learning for RACE (maze-solving) -- where memory actually pays off (remember
// dead-ends). Reward = reach the goal (+1), fall in a pit (0), with BFS-distance shaping toward
// the goal. Trains across campaign levels 1..levels. Unlike the battle hunt, the goal-bearing
// senses make this learnable and the recurrent state helps escape traps. In place; chunk-friendly.
// `board`/`start` (optional): train on THIS exact maze (e.g. the trainer's current race board, so
// the result actually wins that match) instead of generated campaign levels. Null => levels 1..levels.
bool qTrainMazeRnn(RNet& brain, uint32_t seedBase, int levels, int episodes, int globalDone = 0,
                   int globalTotal = 0, const Maze* board = nullptr, const Pose* start = nullptr,
                   float epsScale = 1.0f);

// FEEDFORWARD maze Q-learning -- the Net counterpart of qTrainMazeRnn (semi-gradient TD, no BPTT),
// so the campaign maze trainer can offer reward-driven learning for a plain brain too. Same reward
// MDP (goal +1, pit 0, BFS-distance shaping). `board`/`start` train on a specific maze.
bool qTrainMaze(Net& brain, uint32_t seedBase, int levels, int episodes, int globalDone = 0,
                int globalTotal = 0, const Maze* board = nullptr, const Pose* start = nullptr,
                float epsScale = 1.0f);

// Train a RECURRENT brain (BPTT) to imitate the memory-using explorer across campaign
// mazes 1..levels — a generally-capable memory brain to drop into a program. In place,
// so calling again keeps improving it.
void rnnTrainGeneral(RNet& brain, uint32_t seedBase, int levels, int epochs);

// Brain Cam's animated backprop reuses ONE shared episode buffer (so it costs no extra
// static DRAM). captureSolverShared() records the BFS solver's (senses -> action) sequence
// for maze `m` into that buffer and returns its length T (0 if unsolvable); ffTrainShared /
// rnnTrainShared then run a few backprop passes over it in place and return the loss.
int   captureSolverShared(const Maze& m);
float ffTrainShared(Net& brain, int passes);
float rnnTrainShared(RNet& brain, int passes);

}  // namespace gb
