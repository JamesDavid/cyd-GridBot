#include "app/App.h"
#include "hal/Display.h"
#include "hal/Touch.h"

namespace app {

void App::begin() {
  // Phase 1 hardcoded profile (real profiles + persistence arrive in Phase 3).
  _profile.id = "u01";
  _profile.name = "Theo";
  _profile.avatar = 0;
  _profile.level = 1;
  _profile.seedBase = 73219;
  _profile.unlocks = gb::computeUnlocks(_profile.level);

  hal::touch.begin();
  startLevel(_profile.level);
}

void App::startLevel(uint32_t level) {
  _profile.level = level;
  _profile.unlocks = gb::computeUnlocks(level);
  _game.begin(&_profile, level);
  _game.enter();
}

void App::tick(uint32_t now) {
  hal::TouchPoint tp = hal::touch.read();
  Signal s = _game.tick(now, tp);
  if (s == Signal::WON) {
    _profile.stats.levelsCompleted++;
    // clear the resume slot on advance (SPEC §11.1) and move on.
    _profile.workLevel = 0;
    _profile.work.clear();
    startLevel(_profile.level + 1);
  }
}

}  // namespace app
