#include "app/Log.h"
#include <Arduino.h>
#include <vector>
#include <cstdarg>

namespace applog {

static std::vector<String> g_lines;   // heap-backed ring (kept off the static DRAM budget)
static const size_t MAX_LINES = 50;

void event(const char* fmt, ...) {
  char buf[80];
  va_list ap; va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  String line = String((unsigned long)(millis() / 1000)) + "s\t" + buf;
  g_lines.push_back(line);
  if (g_lines.size() > MAX_LINES) g_lines.erase(g_lines.begin());
  Serial.print("LOG "); Serial.println(line);   // also stream it live for a watching tool
}

void dump() {
  Serial.printf("GBLOG %d events (oldest first)\n", (int)g_lines.size());
  for (auto& l : g_lines) Serial.println(l);
  Serial.println("GBLOGEND");
}

void help() {
  Serial.println(F("=== GridBot serial console ==="));
  Serial.println(F("GridBot is a kid-facing coding + AI game on the CYD (ESP32 320x240 touchscreen)."));
  Serial.println(F("Kids snap together command blocks (forward/turn/jump/repeat/if/function/brain),"));
  Serial.println(F("solve mazes, then graduate to TRAINING little neural nets (Teach/Q-Learn/Evolve)"));
  Serial.println(F("and battling them in a Race/Soccer/Battle arena. You are connected over serial as"));
  Serial.println(F("an assistant: read what the kid did with E, watch the screen with S, and help."));
  Serial.println(F("Commands (one per line):"));
  Serial.println(F("  ?        this help"));
  Serial.println(F("  E        dump the event log: what the kid has been doing this session"));
  Serial.println(F("  S        screenshot: emits 'GBSHOT w h', then w*h RGB565 big-endian bytes, then 'GBEND'"));
  Serial.println(F("  T x y    tap the screen at pixel (x,y), 0..319 x 0..239"));
  Serial.println(F("  H        jump to the Home screen"));
  Serial.println(F("  M        print the current maze as ASCII tiles"));
  Serial.println(F("  G n      jump to level n        P n   fast-play up to level n"));
  Serial.println(F("  C n      grant n coins          N     advance a paused match one tick"));
  Serial.println(F("  L        toggle touch logging   X     clear touch calibration (reboots)"));
  Serial.println(F("Events also stream live as 'LOG <seconds>s <text>' lines as they happen."));
  Serial.println(F("GBHELPEND"));
}

}  // namespace applog
