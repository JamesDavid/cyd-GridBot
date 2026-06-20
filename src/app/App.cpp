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
  if (hal::audio.enabled())  // respect the menu mute toggle
    hal::audio.startMusic(hal::kTitleMusic, hal::kTitleMusicLen, true);  // menu theme
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
  if (hal::audio.enabled() && !hal::audio.playing())
    hal::audio.startMusic(hal::kTitleMusic, hal::kTitleMusicLen, true);
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
  if (now.jump && !prev.jump) newText = "New: Jump!";
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
  hal::audio.stopMusic();  // quiet for gameplay (step ticks would clash)
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

void App::debugGrantCoins(uint32_t n) {
  if (_profile.id.empty()) {
    std::vector<store::ProfileMeta> metas;
    store::profiles.listProfiles(metas);
    if (!metas.empty()) loadProfileInto(metas[0].id);
  }
  _profile.coins += n;
  saveProfile();
}

void App::debugAutoRun() {
  if (_state == State::GAME) _game.beginAutoRun();
}

void App::debugStep() {
  if (_state == State::GAME) _game.debugStep();
  else if (_state == State::ARENA) _arena.debugStep();
}

void App::debugNeuroLesson() {
  hal::audio.stopMusic();
  _lessonsMenu.enter();
  _state = State::LESSONS_MENU;
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
  hal::audio.update(now);  // advance the background melody (no-op if not playing)
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
          if (hal::audio.enabled())  // battle theme on the arena menus
            hal::audio.startMusic(hal::kArenaMusic, hal::kArenaMusicLen, true);
        } else {
          gotoGame();
        }
      }
      break;
    }
    case State::GAME: {
      Signal s = _game.tick(now, tp);
      if (s == Signal::GOTO_NEURO_TRAIN) {  // train a NEURO block's brain on this maze
        hal::audio.stopMusic();
        _neuroTrain.begin(&_game.program(), _game.pendingNeuro(), &_game.maze());
        _game.clearPendingNeuro();
        _neuroTrain.enter();
        _state = State::NEURO_TRAIN;
        break;
      }
      // Seed Challenge: a one-off board — return to the picker, never touch the campaign.
      if (_inChallenge && (s == Signal::BACK || s == Signal::WON)) {
        _inChallenge = false;
        _challenge.begin(&_profile); _challenge.enter(); _state = State::CHALLENGE;
        break;
      }
      if (s == Signal::BACK) { saveProfile(); gotoSelect(); break; }
      if (s == Signal::WON) {
        _profile.stats.levelsCompleted++;
        if (_game.lastStars() == 3) _profile.stats.threeStarWins++;
        if (!_game.program().brains.empty()) _profile.stats.neuroWins++;  // cleared with a brain
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
          hal::audio.badge();  // achievement chime
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
      else if (s == Signal::GOTO_SHOP) {
        _shop.begin(&_profile); _shop.enter(); _state = State::SHOP;
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
        hal::audio.stopMusic();  // drop the battle theme so the intro restarts its own
        gotoIntro(_profile.level);
      }
      else if (s == Signal::GOTO_RADIO) {
        _radio.begin(&_profile); _radio.enter(); _state = State::RADIO;
      }
      else if (s == Signal::GOTO_PUZZLE) {
        _puzzle.begin(&_profile); _puzzle.enter(); _state = State::PUZZLE;
      }
      else if (s == Signal::GOTO_ARENA_TRAIN) {
        _arenaTrain.begin(&_profile); _arenaTrain.enter(); _state = State::ARENA_TRAIN;
      }
      else if (s == Signal::GOTO_CHALLENGE) {
        _challenge.begin(&_profile); _challenge.enter(); _state = State::CHALLENGE;
      }
      break;
    }
    case State::PUZZLE: {
      Signal s = _puzzle.tick(now, tp);
      if (s == Signal::BACK) { _arena.begin(&_profile); _arena.enter(); _state = State::ARENA; }
      break;
    }
    case State::LESSONS_MENU: {
      Signal s = _lessonsMenu.tick(now, tp);
      if (s == Signal::BACK) gotoSelect();
      else if (s == Signal::PLAY) {
        if (_lessonsMenu.pick() == 0) { _codeLab.enter(); _state = State::CODE_LAB; }
        else { _lessonHub.enter(); _state = State::NEURO_HUB; }
      }
      break;
    }
    case State::CODE_LAB: {
      Signal s = _codeLab.tick(now, tp);
      if (s == Signal::BACK) { _lessonsMenu.enter(); _state = State::LESSONS_MENU; }
      else if (s == Signal::PLAY) { _codeLesson.begin(_codeLab.pick()); _codeLesson.enter(); _state = State::CODE_LESSON; }
      break;
    }
    case State::CODE_LESSON: {
      if (_codeLesson.tick(now, tp) == Signal::BACK) { _codeLab.enter(); _state = State::CODE_LAB; }
      break;
    }
    case State::NEURO_HUB: {
      Signal s = _lessonHub.tick(now, tp);
      if (s == Signal::BACK) { _lessonsMenu.enter(); _state = State::LESSONS_MENU; }
      else if (s == Signal::PLAY) {
        switch (_lessonHub.pick()) {
          case 0: case 1: case 2:
            _neuro.begin(_lessonHub.pick()); _neuro.enter(); _state = State::NEURO_LESSON; break;
          case 3: _brainMap.enter(); _state = State::BRAIN_MAP; break;
          case 4: _qLesson.begin(); _qLesson.enter(); _state = State::Q_LESSON; break;
          case 5: _evoLesson.begin(); _evoLesson.enter(); _state = State::EVO_LESSON; break;
          case 6: _transferLesson.begin(); _transferLesson.enter(); _state = State::TRANSFER_LESSON; break;
          case 7: _brainView.begin(); _brainView.enter(); _state = State::BRAIN_VIEW; break;
        }
      }
      break;
    }
    case State::NEURO_LESSON: {
      if (_neuro.tick(now, tp) == Signal::BACK) { _lessonHub.enter(); _state = State::NEURO_HUB; }
      break;
    }
    case State::Q_LESSON: {
      if (_qLesson.tick(now, tp) == Signal::BACK) { _lessonHub.enter(); _state = State::NEURO_HUB; }
      break;
    }
    case State::EVO_LESSON: {
      if (_evoLesson.tick(now, tp) == Signal::BACK) { _lessonHub.enter(); _state = State::NEURO_HUB; }
      break;
    }
    case State::TRANSFER_LESSON: {
      if (_transferLesson.tick(now, tp) == Signal::BACK) { _lessonHub.enter(); _state = State::NEURO_HUB; }
      break;
    }
    case State::BRAIN_VIEW: {
      if (_brainView.tick(now, tp) == Signal::BACK) { _lessonHub.enter(); _state = State::NEURO_HUB; }
      break;
    }
    case State::BRAIN_MAP: {
      if (_brainMap.tick(now, tp) == Signal::BACK) { _lessonHub.enter(); _state = State::NEURO_HUB; }
      break;
    }
    case State::NEURO_TRAIN: {  // came from the editor's brain block; return to the game
      if (_neuroTrain.tick(now, tp) == Signal::BACK) {
        if (_neuroTrain.usedBrain()) _profile.stats.brainsTrained++;
        _profile.achievements |= gb::evaluateAchievements(_profile);
        saveProfile();
        _game.enter(); _state = State::GAME;
      }
      break;
    }
    case State::ARENA_TRAIN: {  // came from the Arena menu; return there (library may have grown)
      if (_arenaTrain.tick(now, tp) == Signal::BACK) {
        if (_arenaTrain.savedFighter()) { _profile.stats.fightersSaved++; _profile.stats.brainsTrained++; }
        _profile.achievements |= gb::evaluateAchievements(_profile);
        saveProfile(); _arena.begin(&_profile); _arena.enter(); _state = State::ARENA;
      }
      break;
    }
    case State::CHALLENGE: {
      Signal s = _challenge.tick(now, tp);
      if (s == Signal::PLAY) {
        hal::audio.stopMusic();  // the game uses step-tick SFX
        _inChallenge = true;
        _game.beginChallenge(&_profile, _challenge.code());
        _game.enter();
        _state = State::GAME;
      } else if (s == Signal::BACK) {
        _arena.begin(&_profile); _arena.enter(); _state = State::ARENA;
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
    case State::SHOP: {
      Signal s = _shop.tick(now, tp);
      if (s == Signal::BACK) { saveProfile(); _stats.begin(&_profile); _stats.enter(); _state = State::STATS; }
      break;
    }
  }
}

}  // namespace app
