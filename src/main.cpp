// GridBot — firmware entry point.
//   normal build  : boots the game (App state machine).
//   -DSELFTEST    : boots the headless engine self-test harness (serial PASS/FAIL).
#include <Arduino.h>
#include <LittleFS.h>

#include "hal/Display.h"
#include "hal/Touch.h"

#ifdef SELFTEST
#include "selftest/SelfTest.h"
#endif

static void mountFS() {
  // format-on-fail = true: a fresh board has no LittleFS image yet.
  if (!LittleFS.begin(true)) {
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
  return;  // idle in loop()
#else
  // Phase 0 smoke test: splash + live touch echo. Replaced by App in Phase 1.
  auto& g = hal::display.gfx();
  g.fillScreen(TFT_BLACK);
  g.setTextColor(TFT_WHITE);
  g.setTextDatum(textdatum_t::middle_center);
  g.setTextSize(2);
  g.drawString("GridBot", g.width() / 2, g.height() / 2 - 12);
  g.setTextSize(1);
  g.drawString("program your way out", g.width() / 2, g.height() / 2 + 14);

  hal::touch.begin();
  Serial.println("Phase 0: touch the screen; coords print here.");
#endif
}

void loop() {
#ifdef SELFTEST
  delay(1000);
  return;
#else
  hal::TouchPoint p = hal::touch.read();
  if (p.pressed) {
    Serial.printf("touch x=%d y=%d\n", p.x, p.y);
    auto& g = hal::display.gfx();
    g.fillCircle(p.x, p.y, 3, TFT_GREEN);
    delay(40);  // debounce
  }
  delay(8);
#endif
}
