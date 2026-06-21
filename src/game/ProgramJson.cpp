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
      case N_NEURO: o["bi"] = (int)n.brainIdx; if (n.pilot) o["pl"] = true; if (n.rnn) o["rn"] = true; break;
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
      case N_NEURO: n.brainIdx = (uint8_t)(int)(o["bi"] | 0); n.pilot = (o["pl"] | false); n.rnn = (o["rn"] | false); break;
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

// A trained recurrent brain. Only TRAINED rbrains are stored, tagged with their index
// (most brains are feedforward, so this keeps the JSON small).
static void rnetToJson(const RNet& n, int idx, JsonObject o) {
  o["x"] = idx; o["i"] = n.nIn; o["h"] = n.nHid; o["o"] = n.nOut; o["lr"] = n.lr;
  JsonArray wih = o["wih"].to<JsonArray>();
  for (int j = 0; j < n.nHid; j++) for (int i = 0; i < n.nIn; i++) wih.add(n.wih[j][i]);
  JsonArray whh = o["whh"].to<JsonArray>();
  for (int j = 0; j < n.nHid; j++) for (int k = 0; k < n.nHid; k++) whh.add(n.whh[j][k]);
  JsonArray bh = o["bh"].to<JsonArray>();
  for (int j = 0; j < n.nHid; j++) bh.add(n.bh[j]);
  JsonArray who = o["who"].to<JsonArray>();
  for (int m = 0; m < n.nOut; m++) for (int j = 0; j < n.nHid; j++) who.add(n.who[m][j]);
  JsonArray bo = o["bo"].to<JsonArray>();
  for (int m = 0; m < n.nOut; m++) bo.add(n.bo[m]);
}

static void rnetFromJson(JsonObjectConst o, RNet& n) {
  n.nIn = (int)(o["i"] | 10); n.nHid = (int)(o["h"] | 8); n.nOut = (int)(o["o"] | 5);
  n.lr = (float)(o["lr"] | 0.1f); n.trained = true;
  JsonArrayConst wih = o["wih"].as<JsonArrayConst>(); int t = 0;
  for (int j = 0; j < n.nHid; j++) for (int i = 0; i < n.nIn; i++) n.wih[j][i] = (float)(wih[t++] | 0.0f);
  JsonArrayConst whh = o["whh"].as<JsonArrayConst>(); t = 0;
  for (int j = 0; j < n.nHid; j++) for (int k = 0; k < n.nHid; k++) n.whh[j][k] = (float)(whh[t++] | 0.0f);
  JsonArrayConst bh = o["bh"].as<JsonArrayConst>();
  for (int j = 0; j < n.nHid; j++) n.bh[j] = (float)(bh[j] | 0.0f);
  JsonArrayConst who = o["who"].as<JsonArrayConst>(); t = 0;
  for (int m = 0; m < n.nOut; m++) for (int j = 0; j < n.nHid; j++) n.who[m][j] = (float)(who[t++] | 0.0f);
  JsonArrayConst bo = o["bo"].as<JsonArrayConst>();
  for (int m = 0; m < n.nOut; m++) n.bo[m] = (float)(bo[m] | 0.0f);
}

void programToJson(const Program& p, JsonObject out) {
  nodeListToJson(p.main, out["main"].to<JsonArray>());
  if (!p.f1.empty()) nodeListToJson(p.f1, out["f1"].to<JsonArray>());
  if (!p.f2.empty()) nodeListToJson(p.f2, out["f2"].to<JsonArray>());
  if (!p.brains.empty()) {
    JsonArray ba = out["br"].to<JsonArray>();
    for (const Net& b : p.brains) netToJson(b, ba.add<JsonObject>());
  }
  bool anyRnn = false;
  for (const RNet& r : p.rbrains) if (r.trained) anyRnn = true;
  if (anyRnn) {                                  // store only trained recurrent brains, by index
    JsonArray ra = out["rbr"].to<JsonArray>();
    for (size_t i = 0; i < p.rbrains.size(); i++)
      if (p.rbrains[i].trained) rnetToJson(p.rbrains[i], (int)i, ra.add<JsonObject>());
  }
}

void programFromJson(JsonObjectConst in, Program& p) {
  p.clear();
  if (in["main"].is<JsonArrayConst>()) nodeListFromJson(in["main"].as<JsonArrayConst>(), p.main);
  if (in["f1"].is<JsonArrayConst>()) nodeListFromJson(in["f1"].as<JsonArrayConst>(), p.f1);
  if (in["f2"].is<JsonArrayConst>()) nodeListFromJson(in["f2"].as<JsonArrayConst>(), p.f2);
  if (in["br"].is<JsonArrayConst>())
    for (JsonObjectConst o : in["br"].as<JsonArrayConst>()) { Net b; netFromJson(o, b); p.brains.push_back(b); }
  // rebuild rbrains PARALLEL to brains (untrained placeholders), then overlay saved ones
  for (size_t i = 0; i < p.brains.size(); i++) {
    RNet r; r.config(p.brains[i].nIn, p.brains[i].nHid, p.brains[i].nOut, (uint32_t)(i + 1));
    p.rbrains.push_back(r);
  }
  if (in["rbr"].is<JsonArrayConst>())
    for (JsonObjectConst o : in["rbr"].as<JsonArrayConst>()) {
      int idx = (int)(o["x"] | 0);
      if (idx >= 0 && idx < (int)p.rbrains.size()) rnetFromJson(o, p.rbrains[idx]);
    }
}

}  // namespace gb
