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
      case N_NEURO: o["bi"] = (int)n.brainIdx; if (n.pilot) o["pl"] = true; break;
      default: break;
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
      case N_NEURO: n.brainIdx = (uint8_t)(int)(o["bi"] | 0); n.pilot = (o["pl"] | false); break;
      default: break;
    }
    if (o["b"].is<JsonArrayConst>()) nodeListFromJson(o["b"].as<JsonArrayConst>(), n.body);
    out.push_back(n);
  }
}

// A trained brain (NeuroBot): dims + the used weight ranges. Compact enough to persist
// and to trade (it travels with the program over LittleFS / ESP-NOW).
static void netToJson(const Net& n, JsonObject o) {
  o["i"] = n.nIn; o["h"] = n.nHid; o["o"] = n.nOut; o["lr"] = n.lr;
  JsonArray w1 = o["w1"].to<JsonArray>();
  for (int j = 0; j < n.nHid; j++) for (int i = 0; i < n.nIn; i++) w1.add(n.w1[j][i]);
  JsonArray b1 = o["b1"].to<JsonArray>();
  for (int j = 0; j < n.nHid; j++) b1.add(n.b1[j]);
  JsonArray w2 = o["w2"].to<JsonArray>();
  for (int k = 0; k < n.nOut; k++) for (int j = 0; j < n.nHid; j++) w2.add(n.w2[k][j]);
  JsonArray b2 = o["b2"].to<JsonArray>();
  for (int k = 0; k < n.nOut; k++) b2.add(n.b2[k]);
}

static void netFromJson(JsonObjectConst o, Net& n) {
  n.nIn = (int)(o["i"] | 10); n.nHid = (int)(o["h"] | 8); n.nOut = (int)(o["o"] | 5);
  n.lr = (float)(o["lr"] | 0.3f);
  JsonArrayConst w1 = o["w1"].as<JsonArrayConst>(); int t = 0;
  for (int j = 0; j < n.nHid; j++) for (int i = 0; i < n.nIn; i++) n.w1[j][i] = (float)(w1[t++] | 0.0f);
  JsonArrayConst b1 = o["b1"].as<JsonArrayConst>();
  for (int j = 0; j < n.nHid; j++) n.b1[j] = (float)(b1[j] | 0.0f);
  JsonArrayConst w2 = o["w2"].as<JsonArrayConst>(); t = 0;
  for (int k = 0; k < n.nOut; k++) for (int j = 0; j < n.nHid; j++) n.w2[k][j] = (float)(w2[t++] | 0.0f);
  JsonArrayConst b2 = o["b2"].as<JsonArrayConst>();
  for (int k = 0; k < n.nOut; k++) n.b2[k] = (float)(b2[k] | 0.0f);
}

void programToJson(const Program& p, JsonObject out) {
  nodeListToJson(p.main, out["main"].to<JsonArray>());
  if (!p.f1.empty()) nodeListToJson(p.f1, out["f1"].to<JsonArray>());
  if (!p.f2.empty()) nodeListToJson(p.f2, out["f2"].to<JsonArray>());
  if (!p.brains.empty()) {
    JsonArray ba = out["br"].to<JsonArray>();
    for (const Net& b : p.brains) netToJson(b, ba.add<JsonObject>());
  }
}

void programFromJson(JsonObjectConst in, Program& p) {
  p.clear();
  if (in["main"].is<JsonArrayConst>()) nodeListFromJson(in["main"].as<JsonArrayConst>(), p.main);
  if (in["f1"].is<JsonArrayConst>()) nodeListFromJson(in["f1"].as<JsonArrayConst>(), p.f1);
  if (in["f2"].is<JsonArrayConst>()) nodeListFromJson(in["f2"].as<JsonArrayConst>(), p.f2);
  if (in["br"].is<JsonArrayConst>())
    for (JsonObjectConst o : in["br"].as<JsonArrayConst>()) { Net b; netFromJson(o, b); p.brains.push_back(b); }
}

}  // namespace gb
