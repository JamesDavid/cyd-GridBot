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

// A dumb opponent for the arena: just walks forward (falls/bonks quickly).
inline Program alwaysForwardProgram() {
  Program p;
  Node loop = Node::repeatUntil(AT_GOAL);
  loop.body.push_back(Node::command(CMD_FWD));
  p.main.push_back(loop);
  return p;
}

// Sumo hunter: push when the enemy is right ahead, otherwise advance (SPEC §18.4).
inline Program hunterProgram() {
  Program p;
  Node loop = Node::repeatUntil(AT_GOAL);  // AT_GOAL never true in Sumo -> runs to cap
  Node iff = Node::ifCond(ENEMY_AHEAD);
  iff.body.push_back(Node::command(CMD_PUSH));
  loop.body.push_back(iff);
  loop.body.push_back(Node::command(CMD_FWD));
  p.main.push_back(loop);
  return p;
}

}  // namespace gb
