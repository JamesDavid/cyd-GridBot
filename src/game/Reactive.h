// GridBot / NeuroBot — the reactive navigator shared by the Gauntlet and Pilot trainers.
// A memoryless policy that uses ONLY sensor-equivalent info (straight-line bearing to a
// target + the wall/pit immediately around the bot), so its decisions are a function of
// the brain's inputs and can be imitated by backprop. Header-only (inline). Arduino-free.
#pragma once
#include "game/Maze.h"
#include "game/Types.h"

namespace gb {

// Apply one primitive to a pose on a maze (mirrors Interpreter's move/jump rules).
inline Outcome reactiveApply(const Maze& m, Pose& p, Cmd cmd) {
  int dr, dc; facingDelta(p.facing, dr, dc);
  switch (cmd) {
    case CMD_TURN_L: p.facing = turnLeft(p.facing);  return OUT_OK;
    case CMD_TURN_R: p.facing = turnRight(p.facing); return OUT_OK;
    case CMD_FWD: {
      int nr = p.row + dr, nc = p.col + dc;
      if (!m.inBounds(nr, nc) || m.at(nr, nc) == WALL) return OUT_BONK;
      if (m.at(nr, nc) == PIT) return OUT_FELL;
      p.row = (int8_t)nr; p.col = (int8_t)nc;
      return m.isGoal(nr, nc) ? OUT_WIN : OUT_OK;
    }
    case CMD_JUMP: {
      int mr = p.row + dr, mc = p.col + dc, lr = p.row + 2 * dr, lc = p.col + 2 * dc;
      if (!m.inBounds(lr, lc) || m.at(mr, mc) == WALL || m.at(lr, lc) == WALL) return OUT_BONK;
      if (m.at(lr, lc) == PIT) return OUT_FELL;
      p.row = (int8_t)lr; p.col = (int8_t)lc;
      return m.isGoal(lr, lc) ? OUT_WIN : OUT_OK;
    }
    default: return OUT_OK;
  }
}

// Greedy memoryless navigator toward target (tr,tc): jump pits when the landing is safe,
// steer around walls, otherwise face and advance toward the target's straight-line bearing.
inline Cmd reactiveActionTo(const Maze& m, const Pose& p, int tr, int tc) {
  int fdr, fdc; facingDelta(p.facing, fdr, fdc);
  int rdr, rdc; facingDelta(turnRight(p.facing), rdr, rdc);
  int gr = tr - p.row, gc = tc - p.col;
  int fwd = gr * fdr + gc * fdc;       // how much the target is straight ahead
  int rgt = gr * rdr + gc * rdc;       // how much the target is to the right
  int ar = p.row + fdr, ac = p.col + fdc;
  bool wallAhead = !m.inBounds(ar, ac) || m.at(ar, ac) == WALL;
  bool pitAhead  = m.inBounds(ar, ac) && m.at(ar, ac) == PIT;

  if (pitAhead) {
    int lr = p.row + 2 * fdr, lc = p.col + 2 * fdc;
    if (fwd > 0 && m.inBounds(lr, lc) && m.at(lr, lc) != WALL && m.at(lr, lc) != PIT)
      return CMD_JUMP;
    return rgt >= 0 ? CMD_TURN_R : CMD_TURN_L;
  }
  if (wallAhead) return rgt >= 0 ? CMD_TURN_R : CMD_TURN_L;
  if (fwd > 0) {
    if (rgt > fwd)  return CMD_TURN_R;
    if (-rgt > fwd) return CMD_TURN_L;
    return CMD_FWD;
  }
  if (rgt != 0) return rgt > 0 ? CMD_TURN_R : CMD_TURN_L;
  return CMD_TURN_R;
}

inline int reactiveCmdToAction(Cmd c) {
  switch (c) { case CMD_FWD: return 0; case CMD_TURN_L: return 1;
               case CMD_TURN_R: return 2; case CMD_JUMP: return 3; default: return 0; }
}

// Memory-using explorer: like reactiveActionTo, but it remembers visited tiles and prefers
// UNVISITED ground, so it backs out of dead-ends instead of oscillating. This is the
// stateful teacher a RECURRENT brain must learn to imitate from senses alone (the RNN has
// no "visited" input — it has to build the memory internally). `visited` is MAZE_MAX_CELLS.
inline Cmd exploreActionTo(const Maze& m, const Pose& p, const uint8_t* visited, int tr, int tc) {
  int fdr, fdc; facingDelta(p.facing, fdr, fdc);
  int rdr, rdc; facingDelta(turnRight(p.facing), rdr, rdc);
  int ldr, ldc; facingDelta(turnLeft(p.facing), ldr, ldc);
  int ar = p.row + fdr, ac = p.col + fdc;
  int rr = p.row + rdr, rc = p.col + rdc;
  int lr = p.row + ldr, lc = p.col + ldc;
  int gr = tr - p.row, gc = tc - p.col;
  int fwd = gr * fdr + gc * fdc, rgt = gr * rdr + gc * rdc;

  if (m.inBounds(ar, ac) && m.at(ar, ac) == PIT) {
    int jr = p.row + 2 * fdr, jc = p.col + 2 * fdc;
    if (fwd > 0 && m.inBounds(jr, jc) && m.at(jr, jc) != WALL && m.at(jr, jc) != PIT)
      return CMD_JUMP;
    return rgt >= 0 ? CMD_TURN_R : CMD_TURN_L;
  }
  auto open  = [&](int r, int c) { return m.inBounds(r, c) && m.at(r, c) != WALL && m.at(r, c) != PIT; };
  auto fresh = [&](int r, int c) { return open(r, c) && !(visited && visited[r * m.cols() + c]); };
  bool fN = fresh(ar, ac), rN = fresh(rr, rc), lN = fresh(lr, lc);
  if (fN && fwd >= 0) return CMD_FWD;
  if (rN && rgt > 0)  return CMD_TURN_R;
  if (lN && rgt < 0)  return CMD_TURN_L;
  if (fN) return CMD_FWD;
  if (rN) return CMD_TURN_R;
  if (lN) return CMD_TURN_L;
  if (open(ar, ac)) return CMD_FWD;     // all neighbours visited -> backtrack
  if (open(rr, rc)) return CMD_TURN_R;
  if (open(lr, lc)) return CMD_TURN_L;
  return CMD_TURN_R;
}

}  // namespace gb
