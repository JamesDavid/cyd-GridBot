// GridBot — AST <-> JSON (SPEC §5.4). Arduino-free (ArduinoJson works on the host),
// so program serialization is unit-tested on native. ProfileStore (LittleFS) reuses
// these for the resume slot + library.
#pragma once
#include <ArduinoJson.h>
#include "game/Program.h"

namespace gb {

void nodeListToJson(const NodeList& list, JsonArray out);
void nodeListFromJson(JsonArrayConst in, NodeList& out);

void programToJson(const Program& p, JsonObject out);
void programFromJson(JsonObjectConst in, Program& p);

}  // namespace gb
