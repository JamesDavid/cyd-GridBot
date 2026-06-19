#include "app/App.h"
#include "hal/Display.h"
#include "hal/Touch.h"
#include "hal/Audio.h"
#include "hal/Led.h"
#include "ui/UI.h"
#include "store/ProfileStore.h"
#include "game/MazeGen.h"

using namespace ui;

namespace app {

void App::begin() {
  hal::touch.begin();
  hal::audio.begin();
  hal::led.begin();
  store::profiles.begin();
  gotoSelect();
}

void App::gotoSelect() {
  _state = State::SELECT;
  _select.begin(&store::profiles);
  _select.enter();
}

void App::loadProfileInto(const std::string& id) {
  if (!store::profiles.load(id, _profile)) {
    _profile = gb::Profile{};
    _profile.id = id;
  }
  // Sticky unlocks: level only rises, so derive from level (SPEC §7).
  _profile.unlocks = gb::computeUnlocks(_profile.level);
}

void App::saveProfile() { store::profiles.save(_profile); }

void App::gotoIntro(uint32_t level) {
  _introLevel = level;
  _state = State::INTRO;
  drawIntro();
}

void App::drawIntro() {
  hal::led.off();
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  int w = 240, h = 120, x = (SCREEN_W - w) / 2, y = (SCREEN_H - h) / 2;
  g.fillRoundRect(x, y, w, h, 10, C_PANEL);
  g.drawRoundRect(x, y, w, h, 10, C_ACCENT);
  char buf[24];
  snprintf(buf, sizeof(buf), "Level %u", (unsigned)_introLevel);
  label(g, SCREEN_W / 2, y + 20, buf, C_ACCENT, textdatum_t::middle_center, 2);

  // newly-unlocked mechanic (compare this level's unlocks to the previous level's)
  gb::Unlocks now = gb::computeUnlocks(_introLevel);
  gb::Unlocks prev = gb::computeUnlocks(_introLevel ? _introLevel - 1 : 0);
  const char* newText = nullptr;
  if (now.backward && !prev.backward) newText = "New: Backward!";
  else if (now.jump && !prev.jump) newText = "New: Jump!";
  else if (now.repeat && !prev.repeat) newText = "New: Repeat loops!";
  else if (now.func && !prev.func) newText = "New: Functions!";
  else if (now.sense && !prev.sense) newText = "New: Sensing!";
  if (newText) label(g, SCREEN_W / 2, y + 52, newText, C_GO, textdatum_t::middle_center);

  label(g, SCREEN_W / 2, y + h - 22, "tap to start", C_DIM, textdatum_t::middle_center);

  // Arena unlocks after the sensing tier (SPEC §18).
  if (_profile.unlocks.sense) {
    button(g, _arenaBtn, "ARENA >", C_FUNC, C_PANEL);
  }
}

void App::gotoGame() {
  _state = State::GAME;
  _game.begin(&_profile, _introLevel);
  _game.enter();
}

void App::tick(uint32_t now) {
  hal::TouchPoint tp = hal::touch.read();

  switch (_state) {
    case State::SELECT: {
      Signal s = _select.tick(now, tp);
      if (s == Signal::PLAY) { loadProfileInto(_select.chosenId()); gotoIntro(_profile.level); }
      else if (s == Signal::NEW_PROFILE) { _create.begin(); _create.enter(); _state = State::CREATE; }
      else if (s == Signal::GOTO_STATS) { loadProfileInto(_select.chosenId()); _stats.begin(&_profile); _stats.enter(); _state = State::STATS; }
      break;
    }
    case State::CREATE: {
      Signal s = _create.tick(now, tp);
      if (s == Signal::CREATED) {
        std::string id = store::profiles.createProfile(_create.name(), _create.avatar());
        loadProfileInto(id);
        gotoIntro(_profile.level);
      }
      break;
    }
    case State::INTRO: {
      int x, y;
      if (_introTap.tapped(tp, now, x, y)) {
        if (_profile.unlocks.sense && _arenaBtn.contains(x, y)) {
          _arena.begin(&_profile); _arena.enter(); _state = State::ARENA;
        } else {
          gotoGame();
        }
      }
      break;
    }
    case State::GAME: {
      Signal s = _game.tick(now, tp);
      if (s == Signal::WON) {
        _profile.stats.levelsCompleted++;
        _profile.workLevel = 0;
        _profile.work.clear();
        _profile.level = _introLevel + 1;
        _profile.unlocks = gb::computeUnlocks(_profile.level);
        saveProfile();  // autosave on WIN (SPEC §11)
        gotoIntro(_profile.level);
      }
      break;
    }
    case State::STATS: {
      Signal s = _stats.tick(now, tp);
      if (s == Signal::BACK) gotoSelect();
      break;
    }
    case State::ARENA: {
      Signal s = _arena.tick(now, tp);
      if (s == Signal::BACK) gotoIntro(_profile.level);
      else if (s == Signal::GOTO_RADIO) {
        _radio.begin(&_profile); _radio.enter(); _state = State::RADIO;
      }
      break;
    }
    case State::RADIO: {
      Signal s = _radio.tick(now, tp);
      if (s == Signal::BACK) { saveProfile(); _arena.begin(&_profile); _arena.enter(); _state = State::ARENA; }
      break;
    }
  }
}

}  // namespace app
