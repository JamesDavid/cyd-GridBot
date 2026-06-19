// GridBot — on-device self-test harness (-DSELFTEST). Boots the board into a
// headless harness that runs the deterministic engine against fixtures and prints
// PASS/FAIL lines the PIO_DEBUG serial loop reads. Mirrors the env:native tests so
// the same logic is confirmed under real ESP32 memory/types.
//
// Each phase appends fixtures here. Phase 0: just the core determinism check.
#pragma once
#include <Arduino.h>
#include "core/Util.h"

namespace selftest {

inline int _pass = 0, _fail = 0;

inline void check(bool cond, const char* name) {
  if (cond) { _pass++; Serial.printf("PASS %s\n", name); }
  else      { _fail++; Serial.printf("FAIL %s\n", name); }
}

inline void runAll() {
  _pass = _fail = 0;
  Serial.println("=== GridBot SELFTEST ===");

  check(gb::seedFor(73219, 14) == gb::seedFor(73219, 14), "seed_deterministic");
  check(gb::seedFor(73219, 14) != gb::seedFor(73219, 15), "seed_varies_level");
  {
    gb::Rng a(gb::seedFor(1, 1)), b(gb::seedFor(1, 1));
    bool same = true;
    for (int i = 0; i < 100; i++) if (a.next() != b.next()) same = false;
    check(same, "rng_deterministic");
  }

  Serial.printf("=== SELFTEST DONE: %d passed, %d failed ===\n", _pass, _fail);
  Serial.println(_fail == 0 ? "SELFTEST RESULT: PASS" : "SELFTEST RESULT: FAIL");
}

}  // namespace selftest
