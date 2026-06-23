// Phase 3 tests: AST <-> JSON round-trips on the host (the LittleFS round-trip is
// confirmed on-device, since the filesystem is board-side).
#include <unity.h>
#include <ArduinoJson.h>
#include "game/Program.h"
#include "game/ProgramJson.h"

using namespace gb;

void setUp() {}
void tearDown() {}

static bool sameList(const NodeList& a, const NodeList& b);
static bool sameNode(const Node& a, const Node& b) {
  if (a.type != b.type) return false;
  switch (a.type) {
    case N_CMD: if (a.cmd != b.cmd) return false; break;
    case N_REPEAT: if (a.count != b.count) return false; break;
    case N_REPEAT_UNTIL:
    case N_IF: if (a.cond != b.cond) return false; break;
    case N_CALL: if (a.func != b.func) return false; break;
    case N_NEURO: if (a.brainIdx != b.brainIdx || a.pilot != b.pilot || a.rnn != b.rnn) return false; break;
    default: break;
  }
  return sameList(a.body, b.body);
}
static bool sameList(const NodeList& a, const NodeList& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); i++) if (!sameNode(a[i], b[i])) return false;
  return true;
}

void test_program_roundtrip() {
  Program p;
  p.main.push_back(Node::command(CMD_FWD));
  Node rep = Node::repeat(4);
  rep.body.push_back(Node::command(CMD_TURN_R));
  Node iff = Node::ifCond(WALL_AHEAD);
  iff.body.push_back(Node::command(CMD_JUMP));
  rep.body.push_back(iff);
  p.main.push_back(rep);
  p.main.push_back(Node::call(1));
  p.f1.push_back(Node::command(CMD_FWD));
  p.f1.push_back(Node::command(CMD_FWD));

  JsonDocument doc;
  programToJson(p, doc.to<JsonObject>());
  std::string s;
  serializeJson(doc, s);

  JsonDocument doc2;
  deserializeJson(doc2, s);
  Program q;
  programFromJson(doc2.as<JsonObjectConst>(), q);

  TEST_ASSERT_TRUE(sameList(p.main, q.main));
  TEST_ASSERT_TRUE(sameList(p.f1, q.f1));
  TEST_ASSERT_TRUE(sameList(p.f2, q.f2));
  TEST_ASSERT_EQUAL(programWrittenCount(p), programWrittenCount(q));
}

void test_empty_program_roundtrip() {
  Program p;
  JsonDocument doc;
  programToJson(p, doc.to<JsonObject>());
  std::string s;
  serializeJson(doc, s);
  JsonDocument doc2;
  deserializeJson(doc2, s);
  Program q;
  q.main.push_back(Node::command(CMD_FWD));  // ensure clear() works
  programFromJson(doc2.as<JsonObjectConst>(), q);
  TEST_ASSERT_TRUE(q.empty());
}

// A NeuroBot program with a trained brain must round-trip — the brain travels with it
// (so it survives save/load and trades over the radio).
void test_neuro_program_roundtrip() {
  Program p;
  uint8_t idx = p.addBrain(1);
  p.brains[idx].w1[0][0] = 1.25f; p.brains[idx].b2[3] = -0.5f;  // distinctive weights
  Node loop = Node::repeatUntil(AT_GOAL);
  loop.body.push_back(Node::pilotBrain(idx));   // a PILOT brain: the pilot flag must survive
  p.main.push_back(loop);

  JsonDocument doc; programToJson(p, doc.to<JsonObject>());
  std::string s; serializeJson(doc, s);
  JsonDocument doc2; deserializeJson(doc2, s);
  Program q; programFromJson(doc2.as<JsonObjectConst>(), q);

  TEST_ASSERT_TRUE(sameList(p.main, q.main));         // the NEURO node + brainIdx
  TEST_ASSERT_EQUAL(1, (int)q.brains.size());
  TEST_ASSERT_EQUAL_FLOAT(1.25f, q.brains[0].w1[0][0]);  // the trained weights survived
  TEST_ASSERT_EQUAL_FLOAT(-0.5f, q.brains[0].b2[3]);
  TEST_ASSERT_EQUAL(5, q.brains[0].nOut);
}

// A trained RECURRENT brain (rnn node) round-trips: the node.rnn flag and the recurrent
// feedback weights survive save/load, and an untrained rbrain is NOT bloating the JSON.
void test_rnn_program_roundtrip() {
  Program p;
  uint8_t idx = p.addBrain(1);
  p.rbrains[idx].trained = true;                 // mark trained so it serializes
  p.rbrains[idx].whh[0][0] = 0.77f;              // distinctive feedback weight
  p.rbrains[idx].who[2][1] = -0.4f;
  Node loop = Node::repeatUntil(AT_GOAL);
  loop.body.push_back(Node::rnnBrain(idx));      // an RNN brain node
  p.main.push_back(loop);

  JsonDocument doc; programToJson(p, doc.to<JsonObject>());
  std::string s; serializeJson(doc, s);
  JsonDocument doc2; deserializeJson(doc2, s);
  Program q; programFromJson(doc2.as<JsonObjectConst>(), q);

  TEST_ASSERT_TRUE(sameList(p.main, q.main));    // rnn flag survives
  TEST_ASSERT_EQUAL(1, (int)q.rbrains.size());   // rebuilt parallel to brains
  TEST_ASSERT_TRUE(q.rbrains[0].trained);
  TEST_ASSERT_EQUAL_FLOAT(0.77f, q.rbrains[0].whh[0][0]);  // feedback weights survived
  TEST_ASSERT_EQUAL_FLOAT(-0.4f, q.rbrains[0].who[2][1]);
}

// Hand-placed pilot waypoints must survive save/load (and radio trade), so the kid's route sticks.
void test_pilot_waypoints_roundtrip() {
  Program p;
  uint8_t idx = p.addBrain(1);
  Node loop = Node::repeatUntil(AT_GOAL);
  loop.body.push_back(Node::pilotBrain(idx));
  p.main.push_back(loop);
  p.waypoints = {12, 13, 23, 33, 34};            // a hand-drawn route (tile indices)

  JsonDocument doc; programToJson(p, doc.to<JsonObject>());
  std::string s; serializeJson(doc, s);
  JsonDocument doc2; deserializeJson(doc2, s);
  Program q; programFromJson(doc2.as<JsonObjectConst>(), q);

  TEST_ASSERT_EQUAL(5, (int)q.waypoints.size());
  for (int i = 0; i < 5; i++) TEST_ASSERT_EQUAL(p.waypoints[i], q.waypoints[i]);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_program_roundtrip);
  RUN_TEST(test_empty_program_roundtrip);
  RUN_TEST(test_neuro_program_roundtrip);
  RUN_TEST(test_rnn_program_roundtrip);
  RUN_TEST(test_pilot_waypoints_roundtrip);
  return UNITY_END();
}
