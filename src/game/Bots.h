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

// Sumo hunter (SPEC §18.4): a real SEEKER, so the match is an actual brawl. The step-VM runs
// one primitive per tick and skips false IFs without spending the tick, so this body is a
// priority each tick: zap if the foe is right ahead -> turn TOWARD the foe (it's off to a
// side) -> turn off a wall/pit -> otherwise close in. The bots home in on each other and
// trade zaps + shoves instead of wandering. (A trained NeuroBot learns the same from foeF/R/D.)
inline Program hunterProgram() {
  Program p;
  Node loop = Node::repeatUntil(AT_GOAL);  // AT_GOAL never true in Sumo -> runs to cap
  auto rule = [&](Cond c, Cmd cmd) {
    Node n = Node::ifCond(c); n.body.push_back(Node::command(cmd)); loop.body.push_back(n);
  };
  rule(ENEMY_AHEAD, CMD_FIRE);    // foe in the tile ahead -> ZAP (shoves it)
  rule(PIT_AHEAD,   CMD_TURN_R);  // never charge into a pit, even while chasing (no suicide)
  rule(ENEMY_RIGHT, CMD_TURN_R);  // foe off to the right -> turn to face it
  rule(ENEMY_LEFT,  CMD_TURN_L);  // foe off to the left  -> turn to face it
  rule(WALL_AHEAD,  CMD_TURN_R);  // wall straight ahead -> turn (don't jam)
  loop.body.push_back(Node::command(CMD_FWD));  // else charge in (ram = shove)
  p.main.push_back(loop);
  return p;
}

}  // namespace gb
