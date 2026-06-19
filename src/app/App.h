// GridBot — top-level app / state machine (SPEC §3). Phase 1: boots a hardcoded
// profile straight into the GAME screen. Phases 3+ add PROFILE_SELECT / CREATE /
// LEVEL_INTRO / STATS around it.
#pragma once
#include <stdint.h>
#include "game/Profile.h"
#include "screens/GameScreen.h"

namespace app {

class App {
 public:
  void begin();
  void tick(uint32_t now);

 private:
  void startLevel(uint32_t level);

  gb::Profile _profile;
  screens::GameScreen _game;
};

}  // namespace app
