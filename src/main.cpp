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
  gApp.tick(millis());
  delay(6);
#endif
}
