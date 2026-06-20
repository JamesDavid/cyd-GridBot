// GridBot — par + star scoring (SPEC §9). Par is the minimal Tier-1 instruction
// count to win, found by a BFS over pose-states (row, col, facing) where each of
// TURN_L / TURN_R / FORWARD (/ JUMP) costs one instruction. Exact for sequencing;
// a fair yardstick for loop/function solutions too (a loop only ever helps).
#pragma once
#include "game/Maze.h"
#include "game/Program.h"

namespace gb {

// Minimal number of primitive commands to reach the goal, or -1 if unreachable.
int shortestSolutionLen(const Maze& m, bool allowJump);

// Reconstruct an actual shortest command sequence into out.main. Returns true if a
// solution exists. Used to play levels (and by tests).
bool solveMaze(const Maze& m, bool allowJump, Program& out);

// Walking distance (in tiles) from (r,c) to the goal over walkable cells, ignoring
// facing; -1 if unreachable. Used to score Puzzle Race (closest-to-goal wins).
int distanceToGoal(const Maze& m, int r, int c);

// Star rating (SPEC §9): <=par ***, <=1.5*par **, win at all *.
inline int starsFor(int writtenCount, int par) {
  if (par <= 0) return 1;
  if (writtenCount <= par) return 3;
  if (writtenCount <= (par * 3) / 2) return 2;
  return 1;
}

}  // namespace gb
