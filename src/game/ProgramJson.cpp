#include "game/ProgramJson.h"

namespace gb {

// Compact keys: t=type, c=cmd, n=count, cd=cond, fn=func, b=body.
void nodeListToJson(const NodeList& list, JsonArray out) {
  for (const Node& n : list) {
    JsonObject o = out.add<JsonObject>();
    o["t"] = (int)n.type;
    switch (n.type) {
      case N_CMD: o["c"] = (int)n.cmd; break;
      case N_REPEAT: o["n"] = (int)n.count; break;
      case N_REPEAT_UNTIL:
      case N_IF: o["cd"] = (int)n.cond; break;
      case N_CALL: o["fn"] = (int)n.func; break;
    }
    if (!n.body.empty()) {
      JsonArray b = o["b"].to<JsonArray>();
      nodeListToJson(n.body, b);
    }
  }
}

void nodeListFromJson(JsonArrayConst in, NodeList& out) {
  out.clear();
  for (JsonObjectConst o : in) {
    Node n;
    n.type = (NodeType)(int)(o["t"] | 0);
    switch (n.type) {
      case N_CMD: n.cmd = (Cmd)(int)(o["c"] | 0); break;
      case N_REPEAT: n.count = (uint8_t)(int)(o["n"] | 2); break;
      case N_REPEAT_UNTIL:
      case N_IF: n.cond = (Cond)(int)(o["cd"] | 0); break;
      case N_CALL: n.func = (uint8_t)(int)(o["fn"] | 1); break;
    }
    if (o["b"].is<JsonArrayConst>()) nodeListFromJson(o["b"].as<JsonArrayConst>(), n.body);
    out.push_back(n);
  }
}

void programToJson(const Program& p, JsonObject out) {
  nodeListToJson(p.main, out["main"].to<JsonArray>());
  if (!p.f1.empty()) nodeListToJson(p.f1, out["f1"].to<JsonArray>());
  if (!p.f2.empty()) nodeListToJson(p.f2, out["f2"].to<JsonArray>());
}

void programFromJson(JsonObjectConst in, Program& p) {
  p.clear();
  if (in["main"].is<JsonArrayConst>()) nodeListFromJson(in["main"].as<JsonArrayConst>(), p.main);
  if (in["f1"].is<JsonArrayConst>()) nodeListFromJson(in["f1"].as<JsonArrayConst>(), p.f1);
  if (in["f2"].is<JsonArrayConst>()) nodeListFromJson(in["f2"].as<JsonArrayConst>(), p.f2);
}

}  // namespace gb
