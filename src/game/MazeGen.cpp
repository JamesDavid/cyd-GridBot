#include "game/MazeGen.h"
#include "core/Util.h"

namespace gb {

// SPEC §7 difficulty curve. Tunable constants, not hard rules.
Difficulty difficultyFor(int level) {
  Difficulty d{};
  if (level <= 3)        { d.rows = 4; d.cols = 4; d.pitDensityPct = 0;  d.wallDensityPct = 10; d.allowPitGaps = false; }
  else if (level <= 8)   { d.rows = 5; d.cols = 5; d.pitDensityPct = 6;  d.wallDensityPct = 18; d.allowPitGaps = false; }
  else if (level <= 14)  { d.rows = 5; d.cols = 6; d.pitDensityPct = 12; d.wallDensityPct = 20; d.allowPitGaps = true; }
  else if (level <= 24)  { d.rows = 6; d.cols = 7; d.pitDensityPct = 18; d.wallDensityPct = 22; d.allowPitGaps = true; }
  else if (level <= 39)  { d.rows = 6; d.cols = 8; d.pitDensityPct = 20; d.wallDensityPct = 24; d.allowPitGaps = true; }
  else if (level <= 54)  { d.rows = 8; d.cols = 10; d.pitDensityPct = 24; d.wallDensityPct = 26; d.allowPitGaps = true; }
  else                   { d.rows = 8; d.cols = 10; d.pitDensityPct = 28; d.wallDensityPct = 28; d.allowPitGaps = true; }
  d.minPathLen = (d.rows + d.cols);
  return d;
}

namespace MazeGen {

// --- Phase 1: fixed hand-built mazes (replaced by the seeded generator in Phase 2).
static void buildFixed(Maze& m, int level) {
  m.reset(4, 4);
  m.fill(FLOOR);
  // START bottom-left facing EAST, GOAL top-right.
  Pose s; s.row = 3; s.col = 0; s.facing = EAST;
  m.setStart(s);
  m.set(s.row, s.col, START);
  m.setGoal(0, 3);

  int v = level % 3;
  if (v == 2) {
    // A wall to route around: must turn, not just barrel forward.
    m.set(3, 2, WALL);
    m.set(2, 2, WALL);
  } else if (v == 0) {
    // A pit adjacent to the path (a wrong Forward drops you in).
    m.set(2, 0, PIT);
    m.set(1, 2, WALL);
  }
}

void generate(Maze& out, uint32_t seedBase, int level) {
  (void)seedBase;  // fixed mazes ignore the seed for now
  buildFixed(out, level);
}

}  // namespace MazeGen
}  // namespace gb
