// GridBot — on-device self-test harness (-DSELFTEST). Boots the board into a
// headless harness that runs the deterministic engine against fixtures and prints
// PASS/FAIL lines the PIO_DEBUG serial loop reads. Mirrors the env:native tests so
// the same logic is confirmed under real ESP32 memory/types.
//
// Each phase appends fixtures here. Phase 0: just the core determinism check.
#pragma once
#include <Arduino.h>
#include "core/Util.h"
#include "game/Maze.h"
#include "game/Program.h"
#include "game/Interpreter.h"
#include "game/MazeGen.h"
#include "game/Score.h"
#include "store/ProfileStore.h"

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

  // Interpreter fixtures under real ESP32 types (mirror the native tests).
  {
    gb::Maze m; m.reset(1, 4); m.fill(gb::FLOOR);
    gb::Pose s; s.row = 0; s.col = 0; s.facing = gb::EAST;
    m.setStart(s); m.set(0, 0, gb::START); m.setGoal(0, 3);
    gb::Program p;
    gb::Node rep = gb::Node::repeat(3);
    rep.body.push_back(gb::Node::command(gb::CMD_FWD));
    p.main.push_back(rep);
    gb::Interpreter it; it.load(&p, &m, m.startPose());
    check(it.runToEnd() == gb::OUT_WIN, "interp_repeat_win");
    check(it.primCount() == 3, "interp_repeat_count");
  }
  {
    gb::Maze m; m.reset(1, 2); m.fill(gb::FLOOR);
    gb::Pose s; s.row = 0; s.col = 0; s.facing = gb::EAST;
    m.setStart(s); m.set(0, 1, gb::WALL);
    m.setGoal(0, 1);  // goal under a wall is unreachable, but FWD bonks first
    gb::Program p; p.main.push_back(gb::Node::command(gb::CMD_FWD));
    gb::Interpreter it; it.load(&p, &m, m.startPose());
    check(it.runToEnd() == gb::OUT_BONK, "interp_bonk");
  }
  {
    // Solvability sweep under real ESP32 RNG/types (mirrors the native sweep).
    bool allSolvable = true;
    int n = 0;
    for (uint32_t sb = 1; sb <= 8; sb++) {
      for (int lvl = 1; lvl <= 60; lvl += 3) {
        gb::Maze m; gb::MazeGen::generate(m, sb, lvl);
        gb::Difficulty d = gb::difficultyFor(lvl);
        if (gb::shortestSolutionLen(m, d.allowPitGaps) <= 0) allSolvable = false;
        n++;
      }
    }
    Serial.printf("  (swept %d generated mazes)\n", n);
    check(allSolvable, "gen_solvable_sweep");
  }

  // LittleFS profile round-trip (the filesystem is board-side, SPEC §14).
  {
    store::profiles.begin();
    gb::Profile p;
    p.id = "uTEST"; p.name = "Theo"; p.avatar = 3; p.level = 14;
    p.seedBase = 73219; p.unlocks = gb::computeUnlocks(14);
    p.stats.totalWins = 7; p.stats.starsTotal = 15;
    p.stats.commandsUsed[gb::CS_FWD] = 42;
    p.workLevel = 14;
    p.work.main.push_back(gb::Node::command(gb::CMD_FWD));
    gb::Node rep = gb::Node::repeat(3);
    rep.body.push_back(gb::Node::command(gb::CMD_TURN_R));
    p.work.main.push_back(rep);
    bool saved = store::profiles.save(p);
    gb::Profile q;
    bool loaded = store::profiles.load("uTEST", q);
    bool eq = loaded && q.name == "Theo" && q.avatar == 3 && q.level == 14 &&
              q.seedBase == 73219 && q.unlocks.jump == true &&
              q.stats.totalWins == 7 && q.stats.commandsUsed[gb::CS_FWD] == 42 &&
              q.workLevel == 14 &&
              gb::programWrittenCount(q.work) == gb::programWrittenCount(p.work);
    check(saved, "profile_save");
    check(eq, "profile_roundtrip");
    store::profiles.remove("uTEST");
  }

  Serial.printf("=== SELFTEST DONE: %d passed, %d failed ===\n", _pass, _fail);
  Serial.println(_fail == 0 ? "SELFTEST RESULT: PASS" : "SELFTEST RESULT: FAIL");
}

}  // namespace selftest
