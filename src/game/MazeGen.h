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

// Sensing tier begins at this level (SPEC §7). At/after it, a level is a
// "generalization" set: one program must clear 2-3 boards (SPEC §7.1).
constexpr int SENSE_LEVEL = 55;
constexpr int MAX_BOARDS = 3;
inline bool isMultiLevel(int level) { return level >= SENSE_LEVEL; }

namespace MazeGen {
// Fill `out` with the maze for (seedBase, level). Always solvable (SPEC §6).
void generate(Maze& out, uint32_t seedBase, int level);

// A left-turn "C-spiral": all walls except a 3-sided corridor (bottom -> right ->
// top), START bottom-left facing EAST, GOAL top-left. Solvable by the canonical
// wall-follower `REPEAT_UNTIL AT_GOAL { IF WALL_AHEAD TURN_L; FORWARD }` — the §7.1
// payoff. Used for multi-maze sensing levels.
void generateSpiral(Maze& out, int rows, int cols);

// Fill out[0..count) with the boards for (seedBase, level). count==1 for normal
// levels; 2-3 wall-follower-solvable spirals for sensing levels (SPEC §7.1).
// Returns the board count (<= MAX_BOARDS).
int generateBoards(Maze* out, int maxOut, uint32_t seedBase, int level);

// Symmetric Race arena (SPEC §18.1): a mirrored board with the GOAL on the centre
// column and two starts equidistant from it (left facing E, right facing W), so
// neither bot has a positional edge. Returns the two start poses via s0/s1.
void generateArena(Maze& out, uint32_t seed, Pose& s0, Pose& s1);
}  // namespace MazeGen

}  // namespace gb
