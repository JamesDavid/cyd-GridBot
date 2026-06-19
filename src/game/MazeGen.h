// GridBot — maze generation (SPEC §6, §7). Deterministic: a level is fully
// determined by (seedBase, level), so mazes are never stored (SPEC §5.2).
//
// Phase 1 ships a small set of FIXED hand-built mazes to prove the loop; Phase 2
// replaces the body with the seeded carve-path + decorate + BFS-verify generator.
// The signature is final so the shell never changes.
#pragma once
#include "game/Maze.h"

namespace gb {

// Difficulty knobs derived from the level number (SPEC §7). Public so tests and the
// generator share one source of truth.
struct Difficulty {
  int rows, cols;
  int pitDensityPct;   // chance a decorated non-path cell becomes a PIT
  int wallDensityPct;  // chance a decorated non-path cell becomes a WALL
  bool allowPitGaps;   // single-tile pit gaps on the path (needs Jump unlocked)
  int minPathLen;      // carved solution path length target
};

Difficulty difficultyFor(int level);

namespace MazeGen {
// Fill `out` with the maze for (seedBase, level). Always solvable (SPEC §6).
void generate(Maze& out, uint32_t seedBase, int level);
}  // namespace MazeGen

}  // namespace gb
