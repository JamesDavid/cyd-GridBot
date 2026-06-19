#include "game/Program.h"

namespace gb {

int writtenCount(const NodeList& list) {
  int n = 0;
  for (const Node& node : list) {
    n += 1;  // the node itself (a CMD, or the REPEAT/IF/CALL header) counts as one
    if (!node.body.empty()) n += writtenCount(node.body);
  }
  return n;
}

int programWrittenCount(const Program& p) {
  // Functions are written once and called cheaply, so their bodies count once each.
  return writtenCount(p.main) + writtenCount(p.f1) + writtenCount(p.f2);
}

}  // namespace gb
