// GridBot — firmware entry point.
//   normal build  : boots the game (App state machine).
//   -DSELFTEST    : boots the headless engine self-test harness (serial PASS/FAIL).
#include <Arduino.h>
#include <LittleFS.h>

#include "hal/Display.h"
#include "hal/Touch.h"

#ifdef SELFTEST
#include "selftest/SelfTest.h"
#else
#include "app/App.h"
#include "app/Log.h"
static app::App gApp;

// USB-serial screenshot for the PIO_DEBUG loop (no WiFi needed): host sends 'S',
// the device streams the panel readback as raw little-endian RGB565, framed by a
// "GBSHOT <w> <h>\n" header and a trailing "GBEND". scripts/shot.py rebuilds a PNG.
static void serialShot() {
  auto& g = hal::display.gfx();
  int w = g.width(), h = g.height();
  Serial.printf("GBSHOT %d %d\n", w, h);
  static uint16_t row[320];
  for (int y = 0; y < h; y++) {
    g.readRect(0, y, w, 1, row);
    Serial.write((const uint8_t*)row, w * 2);
  }
  Serial.print("\nGBEND\n");
}

// Line-oriented serial control for the PIO_DEBUG loop:
//   S            -> screenshot
//   T <x> <y>    -> inject a synthetic tap at screen px (like /api/tap)
//   L            -> toggle touch coordinate logging
//   X            -> clear touch calibration + reboot (force re-calibration)
static void handleSerialLine(const String& line) {
  if (line.length() == 0) return;
  char c = line[0];
  if (c == 'S') {
    serialShot();
  } else if (c == 'T') {
    int x = 0, y = 0, frames = 1;
    int n = sscanf(line.c_str() + 1, "%d %d %d", &x, &y, &frames);
    if (n >= 2) {
      hal::touch.inject(x, y, n >= 3 ? frames : 1);   // 3rd arg = hold N frames (test long-press)
      Serial.printf("TAP %d %d %d\n", x, y, n >= 3 ? frames : 1);
    }
  } else if (c == 'L') {
    static bool on = false; on = !on; hal::touch.setLog(on);
    Serial.printf("LOG %s\n", on ? "on" : "off");
  } else if (c == 'X') {
    hal::touch.clearCalibration();
    Serial.println("calib cleared, rebooting");
    delay(100); ESP.restart();
  } else if (c == 'G') {
    int lvl = 1;
    if (sscanf(line.c_str() + 1, "%d", &lvl) == 1) {
      gApp.debugGoToLevel((uint32_t)lvl);
      Serial.printf("GOTO level %d\n", lvl);
    }
  } else if (c == 'P') {
    int lvl = 23;
    if (sscanf(line.c_str() + 1, "%d", &lvl) == 1) {
      gApp.debugFastPlay((uint32_t)lvl);
      Serial.printf("PLAYED to level %d\n", lvl);
    }
  } else if (c == 'H') {
    gApp.debugHome();
    Serial.println("HOME");
  } else if (c == 'C') {
    int n = 100; sscanf(line.c_str() + 1, "%d", &n);
    gApp.debugGrantCoins((uint32_t)n);
    Serial.printf("COINS +%d\n", n);
  } else if (c == 'A') {
    gApp.debugAutoRun();
    Serial.println("AUTORUN");
  } else if (c == 'N') {
    gApp.debugStep();
    Serial.println("STEP");
  } else if (c == 'B') {
    gApp.debugNeuroLesson();
    Serial.println("NEURO");
  } else if (c == 'D') {
    int n = 0; sscanf(line.c_str() + 1, "%d", &n);
    gApp.debugLoadProg((uint32_t)n);
  } else if (c == 'K') {
    gApp.debugZapDemo();
  } else if (c == 'F') {
    int a = -1, b = -1; sscanf(line.c_str() + 1, "%d %d", &a, &b);
    gApp.debugFightLib(a, b);                 // 'F' lists the library; 'F a b' fields lib a vs b in soccer
  } else if (c == 'M') {
    gApp.debugDumpMaze();
  } else if (c == '?' || c == 'h') {
    applog::help();
  } else if (c == 'E') {
    applog::dump();
  }
}
#endif

static void mountFS() {
  if (!LittleFS.begin(true)) {  // format-on-fail: a fresh board has no image yet
    Serial.println("LittleFS mount FAILED");
  } else {
    Serial.println("LittleFS mounted");
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== GridBot boot ===");

  mountFS();
  hal::display.begin();

#ifdef SELFTEST
  selftest::runAll();
#else
  gApp.begin();
  Serial.println("GridBot running.  (type ? over serial for the command list)");
  applog::event("boot");
#endif
}

void loop() {
#ifdef SELFTEST
  delay(1000);
#else
  static String line;
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == '\n' || ch == '\r') { handleSerialLine(line); line = ""; }
    else if (line.length() < 40) line += ch;
  }
  gApp.tick(millis());
  delay(6);
#endif
}
