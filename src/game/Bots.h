// GridBot — canned programs as plain ASTs (SPEC §7.1, §18.4). One AST shape for
// player / library / AI, so these double as the wall-follower the sensing levels
// teach and as arena opponents later.
#pragma once
#include "game/Program.h"

namespace gb {

// The wall-follower the §7.1 generalization levels are designed around:
//   REPEAT_UNTIL AT_GOAL { IF WALL_AHEAD { TURN_LEFT } FORWARD }
inline Program wallFollowerProgram() {
  Program p;
  Node loop = Node::repeatUntil(AT_GOAL);
  Node iff = Node::ifCond(WALL_AHEAD);
  iff.body.push_back(Node::command(CMD_TURN_L));
  loop.body.push_back(iff);
  loop.body.push_back(Node::command(CMD_FWD));
  p.main.push_back(loop);
  return p;
}

// Mirror of the wall-follower: hugs the RIGHT wall instead of the left, so it
// takes a visibly different route through the same maze — a distinct sparring
// partner alongside the left-hugger.
inline Program wallFollowerRightProgram() {
  Program p;
  Node loop = Node::repeatUntil(AT_GOAL);
  Node iff = Node::ifCond(WALL_AHEAD);
  iff.body.push_back(Node::command(CMD_TURN_R));
  loop.body.push_back(iff);
  loop.body.push_back(Node::command(CMD_FWD));
  p.main.push_back(loop);
  return p;
}

// A dumb opponent for the arena: just walks forward (falls/bonks quickly).
inline Program alwaysForwardProgram() {
  Program p;
  Node loop = Node::repeatUntil(AT_GOAL);
  loop.body.push_back(Node::command(CMD_FWD));
  p.main.push_back(loop);
  return p;
}

// Sumo hunter (SPEC §18.4): zap when the enemy is right ahead; otherwise PATROL (turn at
// walls) instead of walking straight into one. Patrolling keeps it roaming the ring so it
// actually runs into the opponent -- a straight-walker just jams against a wall and the
// match stalls to a draw. A trained NeuroBot learns true seeking; this is the code stand-in.
inline Program hunterProgram() {
  Program p;
  Node loop = Node::repeatUntil(AT_GOAL);  // AT_GOAL never true in Sumo -> runs to cap
  Node zap = Node::ifCond(ENEMY_AHEAD);
  zap.body.push_back(Node::command(CMD_FIRE));
  loop.body.push_back(zap);
  Node turn = Node::ifCond(BLOCKED_AHEAD);   // wall or pit ahead -> turn to keep moving
  turn.body.push_back(Node::command(CMD_TURN_L));
  loop.body.push_back(turn);
  loop.body.push_back(Node::command(CMD_FWD));
  p.main.push_back(loop);
  return p;
}

}  // namespace gb
