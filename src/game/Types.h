// GridBot — shared engine types (platform-agnostic). SPEC §5.3, §5.4, §8.
#pragma once
#include <stdint.h>

namespace gb {

// Tile types (SPEC §5.3). COLOR_*/COIN/STAR reserved for later tiers.
enum Tile : uint8_t { FLOOR, WALL, PIT, START, GOAL, COLOR_A, COLOR_B, COIN, STAR };

// Heading. Order matters: turnR = (f+1)&3, turnL = (f+3)&3.
enum Facing : uint8_t { NORTH = 0, EAST = 1, SOUTH = 2, WEST = 3 };

// Primitive commands (SPEC §8.1).
enum Cmd : uint8_t { CMD_FWD, CMD_BACK, CMD_TURN_L, CMD_TURN_R, CMD_JUMP,
                     CMD_FIRE };  // FIRE ("zap") is the arena attack — shoves a rival (§18.2)

// Conditions for IF / REPEAT_UNTIL (SPEC §8.1). ENEMY_* are arena-only (§18.3).
// BLOCKED_AHEAD = wall OR pit (the kid's "can't go straight" — handy for wall-followers).
// ENEMY_LEFT/RIGHT = the foe is off to that side (ego-relative) — used by the Sumo seeker so
// a code bot can TURN toward the enemy and line up a zap. Appended to keep byte values stable.
// SOCCER conditions (appended, byte-stable) let a kid HAND-CODE a soccer bot with the same senses
// the brain trains on: BALL_* = where the ball is, NET_* = which way your goal is, and the RIVAL is
// the existing ENEMY_* (in soccer the "enemy" view points at the opposing bot). WALL_LEFT/RIGHT fill
// the old gap (the brain always sensed side walls; now the blocks can too).
enum Cond : uint8_t { WALL_AHEAD, PIT_AHEAD, AT_GOAL, ENEMY_AHEAD, ENEMY_NEAR, BLOCKED_AHEAD,
                      ENEMY_LEFT, ENEMY_RIGHT,
                      BALL_AHEAD, BALL_LEFT, BALL_RIGHT, BALL_NEAR, NET_LEFT, NET_RIGHT,
                      WALL_LEFT, WALL_RIGHT };

// The editor's IF/UNTIL condition cycle (tap a condition to advance to the next). Shared by both
// editors so the order is consistent: obstacles -> goal -> foe -> SOCCER (ball, net) -> side walls.
inline Cond nextCond(Cond c) {
  switch (c) {
    case WALL_AHEAD:    return PIT_AHEAD;
    case PIT_AHEAD:     return BLOCKED_AHEAD;
    case BLOCKED_AHEAD: return AT_GOAL;
    case AT_GOAL:       return ENEMY_AHEAD;
    case ENEMY_AHEAD:   return ENEMY_NEAR;
    case ENEMY_NEAR:    return ENEMY_LEFT;
    case ENEMY_LEFT:    return ENEMY_RIGHT;
    case ENEMY_RIGHT:   return BALL_AHEAD;
    case BALL_AHEAD:    return BALL_LEFT;
    case BALL_LEFT:     return BALL_RIGHT;
    case BALL_RIGHT:    return BALL_NEAR;
    case BALL_NEAR:     return NET_LEFT;
    case NET_LEFT:      return NET_RIGHT;
    case NET_RIGHT:     return WALL_LEFT;
    case WALL_LEFT:     return WALL_RIGHT;
    case WALL_RIGHT:    return WALL_AHEAD;
  }
  return WALL_AHEAD;
}

// AST node kinds (SPEC §5.4). N_NEURO (NeuroBot) runs a trained brain: it senses, picks
// an action via argmax, and executes it — a learned reactive policy embedded in code.
enum NodeType : uint8_t { N_CMD, N_REPEAT, N_REPEAT_UNTIL, N_IF, N_CALL, N_NEURO };

// step() outcome (SPEC §8.3).
enum Outcome : uint8_t { OUT_OK, OUT_WIN, OUT_BONK, OUT_FELL, OUT_DONE_NO_WIN };

struct Pose {
  int8_t row = 0, col = 0;
  Facing facing = EAST;
  bool operator==(const Pose& o) const {
    return row == o.row && col == o.col && facing == o.facing;
  }
};

// Direction delta for a facing (row, col).
inline void facingDelta(Facing f, int& dr, int& dc) {
  switch (f) {
    case NORTH: dr = -1; dc = 0; break;
    case EAST:  dr = 0;  dc = 1; break;
    case SOUTH: dr = 1;  dc = 0; break;
    case WEST:  dr = 0;  dc = -1; break;
  }
}

inline Facing turnRight(Facing f) { return (Facing)((f + 1) & 3); }
inline Facing turnLeft(Facing f)  { return (Facing)((f + 3) & 3); }

}  // namespace gb
