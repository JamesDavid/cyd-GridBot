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
    int x = 0, y = 0;
    if (sscanf(line.c_str() + 1, "%d %d", &x, &y) == 2) {
      hal::touch.inject(x, y);
      Serial.printf("TAP %d %d\n", x, y);
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
  Serial.println("GridBot running.");
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
