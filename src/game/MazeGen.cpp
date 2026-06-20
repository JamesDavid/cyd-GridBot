#include "game/MazeGen.h"
#include "game/Score.h"
#include "core/Util.h"

namespace gb {

// SPEC §7 difficulty curve. Tunable constants, not hard rules.
Difficulty difficultyFor(int level) {
  // Compressed to match the unlock curve (jump@6, repeat@10, func@15, sense@22).
  Difficulty d{};
  if (level <= 2)        { d.rows = 4; d.cols = 4; d.pitDensityPct = 0;  d.wallDensityPct = 8;  d.allowPitGaps = false; }
  else if (level <= 5)   { d.rows = 5; d.cols = 5; d.pitDensityPct = 0;  d.wallDensityPct = 16; d.allowPitGaps = false; }
  else if (level <= 9)   { d.rows = 5; d.cols = 6; d.pitDensityPct = 10; d.wallDensityPct = 18; d.allowPitGaps = true; }   // jump@6
  else if (level <= 14)  { d.rows = 6; d.cols = 7; d.pitDensityPct = 14; d.wallDensityPct = 20; d.allowPitGaps = true; }   // repeat@10
  else if (level <= 21)  { d.rows = 6; d.cols = 8; d.pitDensityPct = 16; d.wallDensityPct = 22; d.allowPitGaps = true; }   // func@15
  else if (level <= 34)  { d.rows = 8; d.cols = 9; d.pitDensityPct = 20; d.wallDensityPct = 24; d.allowPitGaps = true; }   // sense@22
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

  // After Jump unlocks, place single-tile PIT gaps ON straight stretches of the
  // path so a jump is actually required (SPEC §6 fairness). Only on corridor cells
  // (exactly two collinear path-neighbours), never adjacent to another gap (no
  // 2-wide gaps, which Jump can't clear), never on START/GOAL.
  if (d.allowPitGaps) {
    int gaps = 0, maxGaps = (R + C) / 4;
    const int dR[4] = {-1, 1, 0, 0}, dC[4] = {0, 0, -1, 1};
    for (int r = 0; r < R && gaps < maxGaps; r++) {
      for (int c = 0; c < C && gaps < maxGaps; c++) {
        if (!path[r * C + c]) continue;
        if ((r == sr && c == sc) || (r == gr && c == gc)) continue;
        // keep the cells right next to START/GOAL clean (fairness: first/last move)
        if (absv(r - sr) + absv(c - sc) <= 1) continue;
        if (absv(r - gr) + absv(c - gc) <= 1) continue;
        int ndir[4], ncnt = 0;
        bool nearPit = false;
        for (int k = 0; k < 4; k++) {
          int nr = r + dR[k], nc = c + dC[k];
          if (!out.inBounds(nr, nc)) continue;
          if (out.at(nr, nc) == PIT) nearPit = true;
          if (path[nr * C + nc]) ndir[ncnt++] = k;
        }
        if (nearPit || ncnt != 2) continue;
        // collinear if the two path-neighbours are opposite directions
        bool collinear = (dR[ndir[0]] + dR[ndir[1]] == 0) &&
                         (dC[ndir[0]] + dC[ndir[1]] == 0);
        if (!collinear) continue;
        if (rng.chance(35)) { out.set(r, c, PIT); gaps++; }
      }
    }
  }

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

void generateSpiral(Maze& out, int rows, int cols) {
  if (rows < 3) rows = 3;
  if (cols < 3) cols = 3;
  out.reset(rows, cols);
  out.fill(WALL);
  const int b = rows - 1, t = 0, l = 0, r = cols - 1;
  for (int c = l; c <= r; c++) out.set(b, c, FLOOR);   // bottom row
  for (int row = b; row >= t; row--) out.set(row, r, FLOOR);  // right column
  for (int c = r; c >= l; c--) out.set(t, c, FLOOR);   // top row
  Pose s; s.row = (int8_t)b; s.col = (int8_t)l; s.facing = EAST;
  out.setStart(s);
  out.set(b, l, START);
  out.setGoal(t, l);  // top-left, end of the 3-sided corridor
}

void generateArena(Maze& out, uint32_t seed, Pose& s0, Pose& s1) {
  Rng rng(seed);
  int rows = 5 + (int)rng.below(2);       // 5..6 (odd-ish), keep a clear mid row
  int cols = 9;                            // odd so there's a true centre column
  out.reset(rows, cols);
  out.fill(FLOOR);
  int rmid = rows / 2, cmid = cols / 2;

  // Mirror a few symmetric walls/pits so neither side is favoured (no RNG asymmetry).
  int decos = 1 + (int)rng.below(2);
  for (int k = 0; k < decos; k++) {
    int rr = (int)rng.below(rows);
    int cc = (int)rng.below(cmid - 1);     // left half, away from the centre column
    if (rr == rmid || cc == 0) continue;   // keep the mid row + start columns clear
    Tile t = rng.chance(50) ? WALL : PIT;
    out.set(rr, cc, t);
    out.set(rr, cols - 1 - cc, t);         // mirror to the right half
  }

  out.setGoal(rmid, cmid);
  s0.row = (int8_t)rmid; s0.col = 0;        s0.facing = EAST;
  s1.row = (int8_t)rmid; s1.col = (int8_t)(cols - 1); s1.facing = WEST;
  out.set(s0.row, s0.col, START);
  out.set(s1.row, s1.col, START);
}

int generateBoards(Maze* out, int maxOut, uint32_t seedBase, int level) {
  if (!isMultiLevel(level)) {
    generate(out[0], seedBase, level);
    return 1;
  }
  // 2-3 wall-follower-solvable spirals of varying size; same START facing so one
  // general program clears them all (SPEC §7.1).
  Rng rng(seedFor(seedBase, (uint32_t)level));
  int count = 2 + (int)rng.below(2);  // 2 or 3
  if (count > maxOut) count = maxOut;
  if (count > MAX_BOARDS) count = MAX_BOARDS;
  for (int i = 0; i < count; i++) {
    int rows = 5 + (int)rng.below(3) + i;          // 5..9
    int cols = 6 + (int)rng.below(4);              // 6..9
    if (rows > MAZE_MAX_ROWS) rows = MAZE_MAX_ROWS;
    if (cols > MAZE_MAX_COLS) cols = MAZE_MAX_COLS;
    generateSpiral(out[i], rows, cols);
  }
  return count;
}

}  // namespace MazeGen
}  // namespace gb
