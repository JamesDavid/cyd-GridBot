#include "game/MazeGen.h"
#include "game/Score.h"
#include "core/Util.h"

namespace gb {

// SPEC §7 difficulty curve. Tunable constants, not hard rules.
Difficulty difficultyFor(int level) {
  Difficulty d{};
  if (level <= 3)        { d.rows = 4; d.cols = 4; d.pitDensityPct = 0;  d.wallDensityPct = 8;  d.allowPitGaps = false; }
  else if (level <= 8)   { d.rows = 5; d.cols = 5; d.pitDensityPct = 6;  d.wallDensityPct = 16; d.allowPitGaps = false; }
  else if (level <= 14)  { d.rows = 5; d.cols = 6; d.pitDensityPct = 12; d.wallDensityPct = 18; d.allowPitGaps = true; }
  else if (level <= 24)  { d.rows = 6; d.cols = 7; d.pitDensityPct = 16; d.wallDensityPct = 20; d.allowPitGaps = true; }
  else if (level <= 39)  { d.rows = 6; d.cols = 8; d.pitDensityPct = 18; d.wallDensityPct = 22; d.allowPitGaps = true; }
  else if (level <= 54)  { d.rows = 8; d.cols = 10; d.pitDensityPct = 20; d.wallDensityPct = 24; d.allowPitGaps = true; }
  else                   { d.rows = 8; d.cols = 10; d.pitDensityPct = 24; d.wallDensityPct = 26; d.allowPitGaps = true; }
  d.minPathLen = (d.rows + d.cols);
  return d;
}

namespace MazeGen {

static Facing dirFromStep(int sr, int sc) {
  if (sr < 0) return NORTH;
  if (sr > 0) return SOUTH;
  if (sc > 0) return EAST;
  return WEST;
}

// Phase 1: carve a guaranteed solution path from START to GOAL with a biased walk
// (mostly toward the goal, occasionally wandering to add turns). Greedy fallback
// after a step budget guarantees the path always reaches the goal -> solvable (§6).
static void carvePath(int R, int C, int sr, int sc, int gr, int gc, Rng& rng,
                      bool* path, Facing& firstDir) {
  auto inB = [&](int r, int c) { return r >= 0 && r < R && c >= 0 && c < C; };
  int r = sr, c = sc;
  path[r * C + c] = true;
  bool gotFirst = false;
  firstDir = EAST;
  int steps = 0;
  const int wanderBudget = (R + C) * 2;
  while (r != gr || c != gc) {
    int dr = (gr > r) ? 1 : (gr < r ? -1 : 0);
    int dc = (gc > c) ? 1 : (gc < c ? -1 : 0);
    int sR = 0, sC = 0;
    bool wander = (steps < wanderBudget) && rng.chance(30);
    if (wander) {
      // pick a random in-bounds orthogonal step (adds turns / dead-end-free detours)
      for (int tries = 0; tries < 4; tries++) {
        int pick = rng.below(4);
        int tr = (pick == 0) ? -1 : (pick == 1 ? 1 : 0);
        int tc = (pick == 2) ? -1 : (pick == 3 ? 1 : 0);
        if (inB(r + tr, c + tc)) { sR = tr; sC = tc; break; }
      }
      if (sR == 0 && sC == 0) wander = false;
    }
    if (!wander) {
      // greedy: step along the axis with the larger remaining gap
      if (dr != 0 && dc != 0) { if (rng.chance(50)) { sR = dr; } else { sC = dc; } }
      else if (dr != 0) sR = dr;
      else sC = dc;
    }
    int nr = r + sR, nc = c + sC;
    if (!inB(nr, nc)) continue;
    if (!gotFirst) { firstDir = dirFromStep(sR, sC); gotFirst = true; }
    r = nr; c = nc;
    path[r * C + c] = true;
    steps++;
  }
}

static bool tryGenerate(Maze& out, const Difficulty& d, uint32_t seed) {
  const int R = d.rows, C = d.cols;
  Rng rng(seed);
  out.reset(R, C);
  out.fill(FLOOR);

  // START/GOAL on opposite corners (max distance) for a meaty path.
  const int cr[4] = {0, 0, R - 1, R - 1};
  const int cc[4] = {0, C - 1, 0, C - 1};
  int corner = rng.below(4);
  int sr = cr[corner], sc = cc[corner];
  int gr = cr[3 - corner], gc = cc[3 - corner];

  static bool path[MAZE_MAX_CELLS];
  for (int i = 0; i < R * C; i++) path[i] = false;
  Facing firstDir;
  carvePath(R, C, sr, sc, gr, gc, rng, path, firstDir);

  // Decorate non-path cells with WALL or PIT at difficulty densities (§6.2).
  for (int r = 0; r < R; r++) {
    for (int c = 0; c < C; c++) {
      if (path[r * C + c]) continue;
      if (rng.chance(d.wallDensityPct)) out.set(r, c, WALL);
      else if (rng.chance(d.pitDensityPct)) out.set(r, c, PIT);
    }
  }

  Pose s; s.row = (int8_t)sr; s.col = (int8_t)sc; s.facing = firstDir;
  out.setStart(s);
  out.set(sr, sc, START);
  out.setGoal(gr, gc);  // also sets the GOAL tile

  // Verify (cheap safety net, §6.3). The carved path is always walkable, so this
  // essentially never fails, but the assert protects against edge cases.
  return shortestSolutionLen(out, d.allowPitGaps) > 0;
}

void generate(Maze& out, uint32_t seedBase, int level) {
  Difficulty d = difficultyFor(level);
  uint32_t base = seedFor(seedBase, (uint32_t)level);
  for (int attempt = 0; attempt < 8; attempt++) {
    if (tryGenerate(out, d, base + attempt)) return;  // reroll with seed+1 (§6.3)
  }
  // Fallback: an all-floor grid with corner start/goal is trivially solvable.
  out.reset(d.rows, d.cols);
  out.fill(FLOOR);
  Pose s; s.row = (int8_t)(d.rows - 1); s.col = 0; s.facing = NORTH;
  out.setStart(s);
  out.set(s.row, s.col, START);
  out.setGoal(0, d.cols - 1);
}

}  // namespace MazeGen
}  // namespace gb
