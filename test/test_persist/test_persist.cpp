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

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_program_roundtrip);
  RUN_TEST(test_empty_program_roundtrip);
  return UNITY_END();
}
