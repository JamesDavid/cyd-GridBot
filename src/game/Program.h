// GridBot — the kid's authored code as an AST (SPEC §5.4). One shape is used
// everywhere: player program, function bodies, library entries, and AI bots (§11.1,
// §18) — so nothing special-cases "a program". Tier 1 only emits CMD nodes, but the
// tree supports loops/functions/sensing from the start.
#pragma once
#include <vector>
#include "game/Types.h"

namespace gb {

struct Node {
  NodeType type = N_CMD;
  Cmd  cmd  = CMD_FWD;     // N_CMD
  Cond cond = WALL_AHEAD;  // N_IF, N_REPEAT_UNTIL
  uint8_t count = 2;       // N_REPEAT (2..5)
  uint8_t func  = 1;       // N_CALL  (1 or 2)
  std::vector<Node> body;  // N_REPEAT / N_REPEAT_UNTIL / N_IF

  static Node command(Cmd c)            { Node n; n.type = N_CMD; n.cmd = c; return n; }
  static Node repeat(uint8_t c)         { Node n; n.type = N_REPEAT; n.count = c; return n; }
  static Node repeatUntil(Cond c)       { Node n; n.type = N_REPEAT_UNTIL; n.cond = c; return n; }
  static Node ifCond(Cond c)            { Node n; n.type = N_IF; n.cond = c; return n; }
  static Node call(uint8_t f)           { Node n; n.type = N_CALL; n.func = f; return n; }
};

using NodeList = std::vector<Node>;

struct Program {
  NodeList main;
  NodeList f1, f2;

  void clear() { main.clear(); f1.clear(); f2.clear(); }
  bool empty() const { return main.empty() && f1.empty() && f2.empty(); }
};

// "Written" instruction count for star scoring (SPEC §9): a loop counts as its
// written size (the REPEAT block + the nodes inside it), NOT its runtime expansion —
// this is what rewards loops/functions. Counts main + any defined function bodies.
int writtenCount(const NodeList& list);
int programWrittenCount(const Program& p);

}  // namespace gb
