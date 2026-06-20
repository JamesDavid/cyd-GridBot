#include "app/App.h"
#include "hal/Display.h"
#include "hal/Touch.h"
#include "hal/Audio.h"
#include "hal/Led.h"
#include "ui/UI.h"
#include "store/ProfileStore.h"
#include "game/MazeGen.h"
#include "game/Score.h"
#include "game/Bots.h"
#include "game/Interpreter.h"
#include "game/Achievements.h"

using namespace ui;

namespace app {

// Count a solved program's commands into the histogram (so stats look real).
static void tallyCommands(const gb::Program& p, gb::Stats& st) {
  for (const gb::Node& n : p.main) {
    if (n.type != gb::N_CMD) continue;
    switch (n.cmd) {
      case gb::CMD_FWD:    st.commandsUsed[gb::CS_FWD]++; break;
      case gb::CMD_BACK:   st.commandsUsed[gb::CS_BACK]++; break;
      case gb::CMD_JUMP:   st.commandsUsed[gb::CS_JUMP]++; break;
      case gb::CMD_TURN_L:
      case gb::CMD_TURN_R: st.commandsUsed[gb::CS_TURN]++; break;
      default: break;
    }
  }
}

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
  char buf[32];
  ui::Biome bm = ui::biomeFor((int)_introLevel);
  snprintf(buf, sizeof(buf), "Level %u", (unsigned)_introLevel);
  label(g, SCREEN_W / 2, y + 18, buf, C_ACCENT, textdatum_t::middle_center, 2);
  label(g, SCREEN_W / 2, y + 36, bm.name, C_DIM, textdatum_t::middle_center);

  // newly-unlocked mechanic (compare this level's unlocks to the previous level's)
  gb::Unlocks now = gb::computeUnlocks(_introLevel);
  gb::Unlocks prev = gb::computeUnlocks(_introLevel ? _introLevel - 1 : 0);
  const char* newText = nullptr;
  if (now.backward && !prev.backward) newText = "New: Backward!";
  else if (now.jump && !prev.jump) newText = "New: Jump!";
  else if (now.repeat && !prev.repeat) newText = "New: Repeat loops!";
  else if (now.func && !prev.func) newText = "New: Functions!";
  else if (now.sense && !prev.sense) newText = "New: Sensing!";
  if (newText) label(g, SCREEN_W / 2, y + 50, newText, C_GO, textdatum_t::middle_center);
  if (_newBadge) {
    char b[40]; snprintf(b, sizeof(b), "Badge: %s!", _newBadge);
    label(g, SCREEN_W / 2, y + 68, b, C_FUNC, textdatum_t::middle_center);
  }

  label(g, SCREEN_W / 2, y + h - 18, "tap to start", C_DIM, textdatum_t::middle_center);

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

void App::debugGoToLevel(uint32_t level) {
  if (_profile.id.empty()) {
    std::vector<store::ProfileMeta> metas;
    store::profiles.listProfiles(metas);
    if (!metas.empty()) loadProfileInto(metas[0].id);
    else loadProfileInto(store::profiles.createProfile("Dev", 0));
  }
  _profile.level = level;
  _profile.unlocks = gb::computeUnlocks(level);
  saveProfile();
  gotoIntro(level);
}

void App::debugHome() { gotoSelect(); }

void App::debugAutoRun() {
  if (_state == State::GAME) _game.beginAutoRun();
}

void App::debugStep() {
  if (_state == State::GAME) _game.debugStep();
  else if (_state == State::ARENA) _arena.debugStep();
}

void App::debugFastPlay(uint32_t target) {
  if (_profile.id.empty()) {
    std::vector<store::ProfileMeta> metas;
    store::profiles.listProfiles(metas);
    if (!metas.empty()) loadProfileInto(metas[0].id);
    else loadProfileInto(store::profiles.createProfile("Robo", 0));
  }
  int guard = 0;
  while (_profile.level < target && guard++ < 300) {
    uint32_t lvl = _profile.level;
    gb::Program p;
    bool won = false;
    int par = 1;
    if (gb::isMultiLevel((int)lvl)) {
      gb::Maze boards[gb::MAX_BOARDS];
      int nb = gb::MazeGen::generateBoards(boards, gb::MAX_BOARDS, _profile.seedBase, (int)lvl);
      p = gb::wallFollowerProgram();
      won = true;
      for (int i = 0; i < nb; i++) {
        gb::Interpreter it; it.load(&p, &boards[i], boards[i].startPose(), 2000);
        if (it.runToEnd() != gb::OUT_WIN) won = false;
      }
      par = gb::shortestSolutionLen(boards[0], true);
      _profile.stats.commandsUsed[gb::CS_SENSE]++;
      _profile.stats.commandsUsed[gb::CS_FWD]++;
    } else {
      gb::Maze m; gb::MazeGen::generate(m, _profile.seedBase, (int)lvl);
      par = gb::shortestSolutionLen(m, _profile.unlocks.jump);
      if (gb::solveMaze(m, _profile.unlocks.jump, p)) {
        gb::Interpreter it; it.load(&p, &m, m.startPose());
        won = (it.runToEnd() == gb::OUT_WIN);
      }
      tallyCommands(p, _profile.stats);
    }
    if (!won) break;
    int stars = gb::starsFor(gb::programWrittenCount(p), par > 0 ? par : 1);
    _profile.stats.totalRuns++;
    _profile.stats.totalWins++;
    _profile.stats.levelsCompleted++;
    _profile.stats.starsTotal += stars;
    _profile.stats.currentStreak++;
    _profile.level = lvl + 1;
    _profile.unlocks = gb::computeUnlocks(_profile.level);
  }
  _profile.achievements |= gb::evaluateAchievements(_profile);
  saveProfile();
  gotoIntro(_profile.level);
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
        if (_create.isEdit()) {
          // tweak name/avatar in place — keep all stats/progress
          _profile.name = _create.name();
          _profile.avatar = _create.avatar();
          saveProfile();
          gotoSelect();
        } else {
          std::string id = store::profiles.createProfile(_create.name(), _create.avatar());
          loadProfileInto(id);
          gotoIntro(_profile.level);
        }
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
      if (s == Signal::BACK) { saveProfile(); gotoSelect(); break; }
      if (s == Signal::WON) {
        _profile.stats.levelsCompleted++;
        if (_game.lastStars() == 3) _profile.stats.threeStarWins++;
        _profile.workLevel = 0;
        _profile.work.clear();
        _profile.level = _introLevel + 1;
        _profile.unlocks = gb::computeUnlocks(_profile.level);
        // unlock + celebrate any newly-earned achievements
        uint32_t want = gb::evaluateAchievements(_profile);
        uint32_t isNew = want & ~_profile.achievements;
        _profile.achievements = want;
        _newBadge = nullptr;
        if (isNew) {
          for (int i = 0; i < gb::ACH_COUNT; i++)
            if (isNew & (1u << i)) { _newBadge = gb::achievementName(i); break; }
        }
        saveProfile();  // autosave on WIN (SPEC §11)
        gotoIntro(_profile.level);
      }
      break;
    }
    case State::STATS: {
      Signal s = _stats.tick(now, tp);
      if (s == Signal::BACK) gotoSelect();
      else if (s == Signal::EDIT_PROFILE) {
        _create.beginEdit(_profile.name, _profile.avatar);
        _create.enter();
        _state = State::CREATE;
      }
      else if (s == Signal::GOTO_BADGES) {
        _badges.begin(&_profile); _badges.enter(); _state = State::BADGES;
      }
      else if (s == Signal::GOTO_DRAW) {
        _pixed.begin(&_profile); _pixed.enter(); _state = State::DRAW;
      }
      else if (s == Signal::DELETE_PROFILE) {
        store::profiles.remove(_profile.id);
        _profile = gb::Profile{};
        gotoSelect();
      }
      break;
    }
    case State::DRAW: {
      Signal s = _pixed.tick(now, tp);
      if (s == Signal::BACK) {
        // drop all-empty sprites so they fall back to the roster art
        auto allZero = [](const std::vector<uint8_t>& v) {
          for (uint8_t b : v) if (b) return false; return true; };
        if (allZero(_profile.customChar)) _profile.customChar.clear();
        if (allZero(_profile.customGoal)) _profile.customGoal.clear();
        saveProfile();
        _stats.begin(&_profile); _stats.enter(); _state = State::STATS;
      }
      break;
    }
    case State::ARENA: {
      Signal s = _arena.tick(now, tp);
      if (s == Signal::BACK) {
        // an arena win may have earned the Champion badge
        _profile.achievements |= gb::evaluateAchievements(_profile);
        saveProfile();
        gotoIntro(_profile.level);
      }
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
    case State::BADGES: {
      Signal s = _badges.tick(now, tp);
      if (s == Signal::BACK) { _stats.begin(&_profile); _stats.enter(); _state = State::STATS; }
      break;
    }
  }
}

}  // namespace app
