#include "app/App.h"
#include "app/Log.h"
#include <Arduino.h>   // delay() for the boot splash
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
#include "assets/Assets.h"

using namespace ui;

#define GB_VERSION "1.0"   // firmware version (shown on the boot splash)
#define GB_REPO "github.com/JamesDavid/cyd-GridBot"

namespace app {

// Count a solved program's commands into the histogram (so stats look real).
static void tallyCommands(const gb::Program& p, gb::Stats& st) {
  for (const gb::Node& n : p.main) {
    if (n.type != gb::N_CMD) continue;
    switch (n.cmd) {
      case gb::CMD_FWD:    st.commandsUsed[gb::CS_FWD]++; break;
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
  drawSplash();
  delay(1900);            // a moment to read the splash, then into player select
  gotoSelect();
}

// Boot splash: the title + firmware version/build + repo URL (a tidy "what is this + where" card).
void App::drawSplash() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  label(g, SCREEN_W / 2, 64, "GridBot", C_ACCENT, textdatum_t::middle_center, 4);
  label(g, SCREEN_W / 2, 104, "learn to code + train AI", C_MOVE, textdatum_t::middle_center);
  char v[40]; snprintf(v, sizeof(v), "firmware %s  -  %s", GB_VERSION, __DATE__);
  label(g, SCREEN_W / 2, 158, v, C_DIM, textdatum_t::middle_center);
  label(g, SCREEN_W / 2, 178, GB_REPO, C_INK, textdatum_t::middle_center);
}

void App::gotoSelect() {
  _state = State::SELECT;
  _select.begin(&store::profiles);
  _select.enter();
  if (hal::audio.enabled())  // respect the menu mute toggle
    hal::audio.startMusic(hal::kTitleMusic, hal::kTitleMusicLen, true);  // menu theme
}

void App::gotoHome() {
  _state = State::HOME;
  _home.begin(&_profile);
  _home.enter();
  if (hal::audio.enabled())  // the hub keeps the menu theme
    hal::audio.startMusic(hal::kTitleMusic, hal::kTitleMusicLen, true);
}

void App::returnToSub() {
  if (_subFromHome) { gotoHome(); }
  else { _stats.begin(&_profile); _stats.enter(); _state = State::STATS; }
}

void App::loadProfileInto(const std::string& id) {
  if (!store::profiles.load(id, _profile)) {
    _profile = gb::Profile{};
    _profile.id = id;
  }
  // Sticky unlocks: level only rises, so derive from level (SPEC §7).
  _profile.unlocks = gb::computeUnlocks(_profile.level);
  applog::event("playing as %s (Lv %u)", _profile.name.c_str(), (unsigned)_profile.level);
  // apply this player's saved sound preferences
  hal::audio.setEnabled(_profile.settings.sound);
  hal::audio.setMusicOn(_profile.settings.music);
  hal::audio.setSfxOn(_profile.settings.sfx);
  hal::audio.setVolume(_profile.settings.volume);
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
  // a newly-unlocked power -> point straight at its lesson, at the teachable moment.
  const char* newText = nullptr; const char* subText = nullptr; _introLesson = -1;
  if (now.jump && !prev.jump)        { newText = "New: Jump!";         _introLesson = 1; }   // CodeLab idx
  else if (now.repeat && !prev.repeat){ newText = "New: Repeat loops!"; _introLesson = 2; }
  else if (now.sense && !prev.sense)  { newText = "New: Sensing!";      _introLesson = 3;
                                        subText = "+ Arena Battle - code a fighter!"; }  // foe senses + zap unlock Battle
  else if (now.func && !prev.func)    { newText = "New: Functions!";    _introLesson = 4; }
  else if (now.neuro && !prev.neuro)  { newText = "New: NeuroBot!";     _introLesson = 100; } // NeuroLab hub
  // the training tools arrive one at a time after the brain, each pointing at its lesson:
  else if (now.nDraw && !prev.nDraw)    { newText = "New: Draw & tag!";   _introLesson = 101; } // Data lesson
  else if (now.nEvolve && !prev.nEvolve){ newText = "New: Evolve!";       _introLesson = 102; } // Evolution
  else if (now.nPilot && !prev.nPilot)  { newText = "New: Pilot!";        _introLesson = 103; } // Pilot
  else if (now.nRnn && !prev.nRnn)      { newText = "New: Memory brain!"; _introLesson = 104; } // RNN
  if (newText) label(g, SCREEN_W / 2, y + 50, newText, C_GO, textdatum_t::middle_center);
  if (subText) label(g, SCREEN_W / 2, y + 64, subText, C_ACCENT, textdatum_t::middle_center);
  if (_newBadge) {
    char b[40]; snprintf(b, sizeof(b), "Badge: %s!", _newBadge);
    label(g, SCREEN_W / 2, y + 68, b, C_FUNC, textdatum_t::middle_center);
  }

  label(g, SCREEN_W / 2, y + h - 18, "tap to start", C_DIM, textdatum_t::middle_center);

  // tap the card to start; back out to the hub; or learn the new power right now.
  if (_introLesson >= 0) {
    _backBtn = {28, 196, 120, 28};
    _learnBtn = {172, 196, 120, 28};
    button(g, _backBtn, "< Back", C_INK, C_PANEL);
    button(g, _learnBtn, "Learn it >", ui::rgb(120, 230, 245), C_PANEL);
  } else {
    _backBtn = {90, 196, 140, 28};
    _learnBtn = {0, 0, 0, 0};
    button(g, _backBtn, "< Back", C_INK, C_PANEL);
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

void App::debugDumpMaze() {
  if (_state != State::GAME) { Serial.println("NOMAZE"); return; }
  gb::Maze& m = _game.maze();
  Serial.printf("MAZE %d %d\n", m.rows(), m.cols());
  for (int r = 0; r < m.rows(); r++) {
    for (int c = 0; c < m.cols(); c++) {
      gb::Tile t = m.at(r, c);
      char ch = '.';
      if (t == gb::WALL) ch = '#';
      else if (t == gb::PIT) ch = 'O';
      else if (t == gb::GOAL) ch = 'G';
      else if (t == gb::COIN) ch = 'c';
      else if (t == gb::STAR) ch = '*';
      Serial.write(ch);
    }
    Serial.write('\n');
  }
  gb::Pose p = m.startPose();
  Serial.printf("ROBOT %d %d %d\n", p.row, p.col, (int)p.facing);
  Serial.printf("GOAL %d %d\n", m.goalRow(), m.goalCol());
}

// Build a hand-coding-guide program (and its earlier build stages) so they can be screenshotted in
// the real editor for docs/HAND-CODING-GUIDE.md. Stages mirror the guide's step-by-step examples.
static gb::Program guideProgram(uint32_t n) {
  using namespace gb;
  auto ifDo = [](Cond c, Cmd cmd) { Node x = Node::ifCond(c); x.body.push_back(Node::command(cmd)); return x; };
  Program p;
  Node loop = Node::repeatUntil(AT_GOAL);
  switch (n) {
    case 0:  // MAZE step 1: just drive
      loop.body.push_back(Node::command(CMD_FWD));
      break;
    case 1:  // MAZE step 2: wall-follower
      loop.body.push_back(ifDo(WALL_AHEAD, CMD_TURN_L));
      loop.body.push_back(Node::command(CMD_FWD));
      break;
    case 2:  // MAZE step 3: wall-follower + jump pits (final)
      loop.body.push_back(ifDo(WALL_AHEAD, CMD_TURN_L));
      loop.body.push_back(ifDo(PIT_AHEAD,  CMD_JUMP));
      loop.body.push_back(Node::command(CMD_FWD));
      break;
    case 3:  // BATTLE: the hunter (priority rules)
      loop.body.push_back(ifDo(ENEMY_AHEAD, CMD_FIRE));
      loop.body.push_back(ifDo(PIT_AHEAD,   CMD_TURN_R));
      loop.body.push_back(ifDo(ENEMY_RIGHT, CMD_TURN_R));
      loop.body.push_back(ifDo(ENEMY_LEFT,  CMD_TURN_L));
      loop.body.push_back(ifDo(WALL_AHEAD,  CMD_TURN_R));
      loop.body.push_back(Node::command(CMD_FWD));
      break;
    case 4:  // SOCCER step 1: the naive chaser (first try)
      loop.body.push_back(ifDo(BALL_LEFT,  CMD_TURN_L));
      loop.body.push_back(ifDo(BALL_RIGHT, CMD_TURN_R));
      loop.body.push_back(Node::command(CMD_FWD));
      break;
    default: {  // SOCCER step 2: get behind the ball, then push (final)
      Node onBall = Node::ifCond(BALL_AHEAD);
      onBall.body.push_back(ifDo(NET_LEFT,  CMD_TURN_R));
      onBall.body.push_back(ifDo(NET_RIGHT, CMD_TURN_L));
      loop.body.push_back(onBall);
      loop.body.push_back(ifDo(BALL_LEFT,  CMD_TURN_L));
      loop.body.push_back(ifDo(BALL_RIGHT, CMD_TURN_R));
      loop.body.push_back(Node::command(CMD_FWD));
      break;
    }
  }
  p.main.push_back(loop);
  return p;
}

void App::debugLoadProg(uint32_t n) {
  if (_state != State::GAME) { Serial.println("NOEDIT"); return; }  // open a level's editor first
  _game.program() = guideProgram(n);
  _game.showCodeTop();   // show the Code view (scrolled to the top) with the loaded program
  Serial.printf("LOADPROG %u\n", n);
}

void App::debugZapDemo() {
  if (_profile.id.empty()) { Serial.println("NOPROFILE"); return; }
  using namespace gb;
  auto ifDo = [](Cond c, Cmd cmd) { Node x = Node::ifCond(c); x.body.push_back(Node::command(cmd)); return x; };
  Program p;                                            // a net-AWARE zap-swap dribbler. The plain
  Node loop = Node::repeatUntil(AT_GOAL);               // "always zap" version shoves the ball into its
  Node onBall = Node::ifCond(BALL_AHEAD);               // own net (the swap pops the ball OPPOSITE your
  onBall.body.push_back(ifDo(NET_LEFT,  CMD_FIRE));     // facing). Zapping ONLY when the net is off to a
  onBall.body.push_back(ifDo(NET_RIGHT, CMD_FIRE));     // side means you're facing vertically, so the
  loop.body.push_back(onBall);                          // ball flips up/down to re-line-up -- never into
  loop.body.push_back(ifDo(BALL_LEFT,  CMD_TURN_L));    // a goal. Lined up (net ahead) -> just push it in.
  loop.body.push_back(ifDo(BALL_RIGHT, CMD_TURN_R));
  loop.body.push_back(Node::command(CMD_FWD));
  p.main.push_back(loop);
  LibEntry cone; cone.name = "Cone"; cone.program = Program{}; cone.source = LIB_CODE;  // idle opponent
  _profile.library.push_back(cone);
  LibEntry e; e.name = "Zappy"; e.program = p; e.source = LIB_CODE;
  int idx = (int)_profile.library.size();
  _profile.library.push_back(e);                        // in-memory only (not saved -> gone on reboot)
  _arena.beginQuickBattle(&_profile, idx, "Cone", MatchType::SOCCER);    // vs the idle cone -> BOARD phase
  _state = State::ARENA;
  Serial.println("ZAPDEMO Zappy vs Cone");
}

void App::debugFightLib(int a, int b) {
  if (_profile.id.empty()) { Serial.println("NOPROFILE"); return; }
  int n = (int)_profile.library.size();
  static const char* kSrc[] = {"unknown", "code", "neuro", "braincam", "arena", "radio"};
  if (a < 0 || b < 0) {                                  // 'F' alone -> list saved fighters
    Serial.printf("LIB %d fighters in profile '%s'\n", n, _profile.name.c_str());
    for (int i = 0; i < n; i++) {
      const auto& e = _profile.library[i];
      Serial.printf("  [%d] %-16s src=%s brains=%d\n", i, e.name.c_str(),
                    e.source < 6 ? kSrc[e.source] : "?", (int)e.program.brains.size());
    }
    return;
  }
  if (a >= n || b >= n) { Serial.printf("BADIDX (have %d)\n", n); return; }
  // Field lib[a] as the player vs lib[b] by name -> deterministic soccer match, paused for stepping.
  _arena.beginQuickBattle(&_profile, a, _profile.library[b].name.c_str(), gb::MatchType::SOCCER);
  _state = State::ARENA;
  Serial.printf("FIGHTLIB %s vs %s\n", _profile.library[a].name.c_str(), _profile.library[b].name.c_str());
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
    if (stars == 3) _profile.stats.threeStarWins++;  // so debug fast-play earns Bright Spark too
    _profile.stats.currentStreak++;
    _profile.level = lvl + 1;
    _profile.unlocks = gb::computeUnlocks(_profile.level);
  }
  _profile.achievements |= gb::evaluateAchievements(_profile);
  saveProfile();
  gotoIntro(_profile.level);
}

// The idle nametag screensaver -- a big classroom name card. Only on calm menu screens, so it
// never interrupts a running game/battle/training animation (those redraw without a touch).
bool App::saverEligible() const {
  switch (_state) {
    case State::HOME: case State::STATS: case State::BADGES: case State::SHOP:
    case State::LIBRARY: case State::NEURO_HUB: case State::LESSONS_MENU:
      return true;
    default: return false;
  }
}

void App::drawNametag() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  // a big robot, with the player's full customization (custom drawing / tint / worn emoji)
  assets::drawAvatar(g, SCREEN_W / 2, 92, 92, _profile.customChar.data(), (int)_profile.customChar.size(),
                     _profile.shopColor, _profile.shopEmoji, _profile.avatar, gb::SOUTH);
  label(g, SCREEN_W / 2, 168, _profile.name.c_str(), assets::roster(_profile.avatar).bodyColor,
        textdatum_t::middle_center, 4);
  char s[24]; snprintf(s, sizeof(s), "*%u stars", (unsigned)_profile.stats.starsTotal);
  label(g, SCREEN_W / 2, 202, s, C_ACCENT, textdatum_t::middle_center);
  label(g, SCREEN_W / 2, 226, "tap to wake", C_DIM, textdatum_t::middle_center);
}

void App::wake() {
  switch (_state) {
    case State::HOME:         _home.enter(); break;
    case State::STATS:        _stats.enter(); break;
    case State::BADGES:       _badges.enter(); break;
    case State::SHOP:         _shop.enter(); break;
    case State::LIBRARY:      _library.enter(); break;
    case State::NEURO_HUB:    _lessonHub.enter(); break;
    case State::LESSONS_MENU: _lessonsMenu.enter(); break;
    default: break;
  }
}

// The "train brain >" chooser: pick which trainer to open for this brain.
void App::drawTrainPick() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Train this brain", C_ACCENT, textdatum_t::top_left, 2);
  label(g, SCREEN_W / 2, 50, "Where do you want to train it?", C_DIM, textdatum_t::middle_center);
  ui::Rect maze{30, 70, 260, 40}, arena{30, 120, 260, 40}, cancel{90, 178, 140, 30};
  button(g, maze, "Maze trainer", C_SENSE, C_PANEL);
  label(g, SCREEN_W / 2, 104, "learn to solve a level", C_DIM, textdatum_t::middle_center);
  button(g, arena, "Arena (fight)", C_FUNC, C_PANEL);
  label(g, SCREEN_W / 2, 154, "train to win battles", C_DIM, textdatum_t::middle_center);
  button(g, cancel, "Cancel", C_INK, C_PANEL);
}

void App::tick(uint32_t now) {
  hal::audio.update(now);  // advance the background melody (no-op if not playing)
  hal::TouchPoint tp = hal::touch.read();

  // Remember the screen we were on before the current one (for the Menu's Back button). A state
  // change made during the previous tick is visible here as _state != the snapshot.
  if (_state != _stateAtTickStart) { _prevState = _stateAtTickStart; _stateAtTickStart = _state; }

  // Idle nametag screensaver: track the last touch; arm after a quiet minute on a calm screen;
  // any touch wakes back to where you were (and is swallowed so it doesn't also act as a tap).
  if (tp.pressed) _lastInput = now;
  if (_saver) {
    if (tp.pressed) { _saver = false; wake(); }
    return;
  }
  if (saverEligible() && now - _lastInput > SAVER_MS) { _saver = true; drawNametag(); return; }

  // Global overlays sit ABOVE every screen: a long-press opens the Menu modal (Home/Back/Sound/
  // Close); the Sound modal opens from it. If either consumes the tap, the screen underneath is
  // frozen this frame.
  bool stap = tp.pressed && !_soundDown; _soundDown = tp.pressed;   // rising-edge tap
  if (handleMenuUi(now, tp, stap)) return;
  if (handleSoundUi(tp.x, tp.y, stap)) return;

  switch (_state) {
    case State::SELECT: {
      Signal s = _select.tick(now, tp);
      if (s == Signal::PLAY) { loadProfileInto(_select.chosenId()); gotoHome(); }
      else if (s == Signal::NEW_PROFILE) { _create.begin(); _create.enter(); _state = State::CREATE; }
      else if (s == Signal::GOTO_STATS) { loadProfileInto(_select.chosenId()); _stats.begin(&_profile); _stats.enter(); _state = State::STATS; }
      break;
    }
    case State::HOME: {
      Signal s = _home.tick(now, tp);
      if (s == Signal::GOTO_PLAY) { _levelSelect.begin(&_profile); _levelSelect.enter(); _state = State::LEVEL_SELECT; }
      else if (s == Signal::GOTO_ARENA) {
        _arena.begin(&_profile); _arena.enter(); _state = State::ARENA;
        if (hal::audio.enabled()) hal::audio.startMusic(hal::kArenaMusic, hal::kArenaMusicLen, true);
      }
      else if (s == Signal::GOTO_LEARN) { _lessonsMenu.enter(); _state = State::LESSONS_MENU; }
      else if (s == Signal::GOTO_DRAW) { _subFromHome = true; _pixed.begin(&_profile); _pixed.enter(); _state = State::DRAW; }
      else if (s == Signal::GOTO_BADGES) { _subFromHome = true; _badges.begin(&_profile); _badges.enter(); _state = State::BADGES; }
      else if (s == Signal::GOTO_SHOP) { _subFromHome = true; _shop.begin(&_profile); _shop.enter(); _state = State::SHOP; }
      else if (s == Signal::GOTO_STATS) { _stats.begin(&_profile); _stats.enter(); _state = State::STATS; }
      else if (s == Signal::GOTO_MYBOTS) { _library.begin(&_profile); _library.enter(); _state = State::LIBRARY; }
      else if (s == Signal::BACK) { saveProfile(); gotoSelect(); }
      break;
    }
    case State::LEVEL_SELECT: {
      Signal s = _levelSelect.tick(now, tp);
      if (s == Signal::GOTO_LEVEL) {
        uint32_t lvl = _levelSelect.pickedLevel();
        if (lvl >= 1) {
          // Reopening a COMPLETED level to revise: pre-load its saved best program into the work
          // slot so the editor opens with it (trim it for more stars). For the CURRENT level, leave
          // the work slot alone so an in-progress script still resumes.
          if (lvl != _profile.level) {
            gb::Program best;
            if (store::profiles.loadLevelProgram(_profile.id, lvl, best) && !best.empty()) {
              _profile.work = best;
              _profile.workLevel = lvl;
            }
          }
          gotoIntro(lvl);
        }
      } else if (s == Signal::BACK) { gotoHome(); }
      break;
    }
    case State::LIBRARY: {
      Signal s = _library.tick(now, tp);
      if (s == Signal::BACK) { saveProfile(); gotoHome(); }
      else if (s == Signal::RENAME_LIB) {
        _renameLibIdx = _library.renameIdx();
        if (_renameLibIdx >= 0 && _renameLibIdx < (int)_profile.library.size()) {
          _create.beginRename(_profile.library[_renameLibIdx].name);
          _create.enter(); _state = State::CREATE;
        }
      }
      else if (s == Signal::EDIT_LIB) {  // open this bot's program in the code editor
        int i = _library.editIdx();
        if (i >= 0 && i < (int)_profile.library.size()) {
          _editLibIdx = i;
          uint32_t lvl = _profile.library[i].srcLevel ? _profile.library[i].srcLevel : 15u;
          hal::audio.stopMusic();
          _game.beginEditLibrary(&_profile, _profile.library[i].program, lvl);
          _game.resumeCode();   // straight to the code view (no maze-study preview)
          _state = State::GAME;
        }
      }
      break;
    }
    case State::TRAIN_PICK: {   // chooser: train this brain in the maze trainer or the arena
      int x, y;
      if (_introTap.tapped(tp, now, x, y)) {
        ui::Rect maze{30, 70, 260, 40}, arena{30, 120, 260, 40}, cancel{90, 178, 140, 30};
        if (maze.contains(x, y)) {                 // maze / neuro trainer (solve a level)
          _neuroTrain.begin(&_profile, &_game.program(), _pendingTrainIdx, &_game.maze());
          _neuroTrain.enter(); _state = State::NEURO_TRAIN; hal::audio.blip();
        } else if (arena.contains(x, y)) {          // arena fight trainer (seed this brain)
          _arenaFromEditor = true;
          _arenaTrain.beginEditBrain(&_profile, &_game.program(), _pendingTrainIdx);
          _arenaTrain.enter(); _state = State::ARENA_TRAIN; hal::audio.blip();
        } else if (cancel.contains(x, y)) {         // back to the editor, no training
          _game.resumeCode(); _state = State::GAME; hal::audio.blip();
        }
      }
      break;
    }
    case State::CREATE: {
      Signal s = _create.tick(now, tp);
      if (s == Signal::BACK) {  // cancel
        if (_renameLibIdx >= 0) { _renameLibIdx = -1; _library.begin(&_profile); _library.enter(); _state = State::LIBRARY; }
        else gotoSelect();
        break;
      }
      if (s == Signal::CREATED) {
        if (_renameLibIdx >= 0) {  // renaming a library bot, not a profile
          if (_renameLibIdx < (int)_profile.library.size()) _profile.library[_renameLibIdx].name = _create.name();
          _renameLibIdx = -1; saveProfile();
          _library.begin(&_profile); _library.enter(); _state = State::LIBRARY;
        } else if (_create.isEdit()) {
          // tweak name/avatar in place — keep all stats/progress
          _profile.name = _create.name();
          _profile.avatar = _create.avatar();
          saveProfile();
          gotoSelect();
        } else {
          std::string id = store::profiles.createProfile(_create.name(), _create.avatar());
          loadProfileInto(id);
          gotoHome();   // land on the menu/hub (Play / Arena / Learn / ...), not straight in a level
        }
      }
      break;
    }
    case State::INTRO: {
      int x, y;
      if (_introTap.tapped(tp, now, x, y)) {
        if (_introLesson >= 0 && _learnBtn.contains(x, y)) {  // learn the new power right now
          _fromIntro = true;
          if (_introLesson == 100) { _lessonHub.enter(); _state = State::NEURO_HUB; }
          else if (_introLesson == 101) { _transferLesson.begin(1); _transferLesson.enter(); _state = State::TRANSFER_LESSON; }  // Data & labels
          else if (_introLesson == 102) { _evoLesson.begin(); _evoLesson.enter(); _state = State::EVO_LESSON; }                 // Evolution
          else if (_introLesson == 103) { _pilotLesson.begin(); _pilotLesson.enter(); _state = State::PILOT_LESSON; }           // Pilot
          else if (_introLesson == 104) { _rnnLesson.begin(); _rnnLesson.enter(); _state = State::RNN_LESSON; }                 // Memory
          else { _codeLesson.begin(_introLesson); _codeLesson.enter(); _state = State::CODE_LESSON; }
        } else if (_backBtn.contains(x, y)) {            // back to the level browser we came from
          _levelSelect.begin(&_profile); _levelSelect.enter(); _state = State::LEVEL_SELECT;
        } else gotoGame();                                // tap the card to start the level
      }
      break;
    }
    case State::GAME: {
      Signal s = _game.tick(now, tp);
      if (s == Signal::GOTO_NEURO_TRAIN) {  // "train brain >" -> pick which trainer (maze vs arena)
        hal::audio.stopMusic();
        _pendingTrainIdx = _game.pendingNeuro();
        _game.clearPendingNeuro();
        drawTrainPick();
        _state = State::TRAIN_PICK;
        break;
      }
      // Editing a saved library bot: write the edited program back to that entry and return to
      // My Bots. No campaign progress/coins. (Brain training above still works mid-edit.)
      if (_editLibIdx >= 0 && (s == Signal::BACK || s == Signal::WON)) {
        if (_editLibIdx < (int)_profile.library.size())
          _profile.library[_editLibIdx].program = _game.program();
        saveProfile();
        _editLibIdx = -1;
        _library.begin(&_profile); _library.enter(); _state = State::LIBRARY;
        break;
      }
      // Seed Challenge: a one-off board — return to the picker, never touch the campaign.
      if (_inChallenge && (s == Signal::BACK || s == Signal::WON)) {
        _inChallenge = false;
        _challenge.begin(&_profile); _challenge.enter(); _state = State::CHALLENGE;
        break;
      }
      if (s == Signal::BACK) {  // leave the level -> back to the browser we came from
        saveProfile();
        _levelSelect.begin(&_profile); _levelSelect.enter(); _state = State::LEVEL_SELECT;
        break;
      }
      if (s == Signal::WON) {
        // Replaying an already-cleared level ("go for gold") must NOT advance/regress progress --
        // only re-record its best (done in GameScreen) and return to the browser. Star/3-star tallies
        // are credited there too (improvements only). Real progression happens only on your frontier.
        bool replay = _introLevel < _profile.level;
        if (!replay) {
          _profile.stats.levelsCompleted++;
          if (!_game.program().brains.empty()) _profile.stats.neuroWins++;  // cleared with a brain
          _profile.level = _introLevel + 1;
          _profile.unlocks = gb::computeUnlocks(_profile.level);
          // Carry your winning program into the next level — the whole point of the game is
          // "write ONE program (e.g. a wall-follower) and watch it solve mazes it's never
          // seen." A hardcoded path won't fit the new maze (tweak it — that's what motivates
          // loops/sensing); a general solver just hits RUN and wins. (SPEC §7.1)
          _profile.work = _game.program();
          _profile.workLevel = _profile.level;
        }
        // unlock + celebrate any newly-earned achievements (revising an old level can earn one too)
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
        if (replay) { _levelSelect.begin(&_profile); _levelSelect.enter(); _state = State::LEVEL_SELECT; }
        else gotoIntro(_profile.level);
      }
      break;
    }
    case State::STATS: {
      Signal s = _stats.tick(now, tp);
      if (s == Signal::BACK) gotoHome();
      else if (s == Signal::EDIT_PROFILE) {
        _create.beginEdit(_profile.name, _profile.avatar);
        _create.enter();
        _state = State::CREATE;
      }
      else if (s == Signal::GOTO_BADGES) {
        _subFromHome = false; _badges.begin(&_profile); _badges.enter(); _state = State::BADGES;
      }
      else if (s == Signal::GOTO_SHOP) {
        _subFromHome = false; _shop.begin(&_profile); _shop.enter(); _state = State::SHOP;
      }
      else if (s == Signal::GOTO_DRAW) {
        _subFromHome = false; _pixed.begin(&_profile); _pixed.enter(); _state = State::DRAW;
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
        returnToSub();
      }
      break;
    }
    case State::ARENA: {
      Signal s = _arena.tick(now, tp);
      if (s == Signal::BACK) {
        // an arena win may have earned the Champion badge
        _profile.achievements |= gb::evaluateAchievements(_profile);
        saveProfile();
        hal::audio.stopMusic();  // drop the battle theme so the hub restarts its own
        gotoHome();
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
      if (s == Signal::BACK) gotoHome();
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
      if (_codeLesson.tick(now, tp) == Signal::BACK) {
        if (_fromIntro) { _fromIntro = false; gotoIntro(_introLevel); }  // back to the level we came from
        else { _codeLab.enter(); _state = State::CODE_LAB; }
      }
      break;
    }
    case State::NEURO_HUB: {
      Signal s = _lessonHub.tick(now, tp);
      if (s == Signal::BACK) {
        if (_fromIntro) { _fromIntro = false; gotoIntro(_introLevel); }
        else { _lessonsMenu.enter(); _state = State::LESSONS_MENU; }
      }
      else if (s == Signal::PLAY) {
        // Order: watch it learn -> how it learns -> what one neuron can't -> more outputs -> ...
        switch (_lessonHub.pick()) {
          case 0:  _neuro.begin(0); _neuro.enter(); _state = State::NEURO_LESSON; break;       // One neuron
          case 1:  _backpropLesson.begin(); _backpropLesson.enter(); _state = State::BACKPROP_LESSON; break;  // Backprop
          case 2:  _perceptionLesson.begin(); _perceptionLesson.enter(); _state = State::PERCEPTION_LESSON; break;  // Perception (sense)
          case 3:  _neuro.begin(2); _neuro.enter(); _state = State::NEURO_LESSON; break;       // Hidden layer (corners/xor)
          case 4:  _neuro.begin(1); _neuro.enter(); _state = State::NEURO_LESSON; break;       // Many actions
          case 5:  _brainMap.enter(); _state = State::BRAIN_MAP; break;                        // Robot brain (puts it together)
          case 6:  _transferLesson.begin(1); _transferLesson.enter(); _state = State::TRANSFER_LESSON; break;  // Data & labels (mode 1)
          case 7:  _qLesson.begin(); _qLesson.enter(); _state = State::Q_LESSON; break;        // Q-learning (reward)
          case 8:  if (!_tuneLesson) _tuneLesson = new screens::TuneLessonScreen();
                   _tuneLesson->begin(); _tuneLesson->enter(); _state = State::TUNE_LESSON; break;  // Tuning (knobs)
          case 9:  if (!_tuneNetLesson) _tuneNetLesson = new screens::TuneNetLessonScreen();
                   _tuneNetLesson->begin(); _tuneNetLesson->enter(); _state = State::TUNENET_LESSON; break;  // Tune a real brain
          case 10: _evoLesson.begin(); _evoLesson.enter(); _state = State::EVO_LESSON; break;  // Evolution (no gradient)
          case 11: _transferLesson.begin(0); _transferLesson.enter(); _state = State::TRANSFER_LESSON; break;  // Transfer
          case 12: _brainView.begin(&_profile); _brainView.enter(); _state = State::BRAIN_VIEW; break;  // Brain Cam
          case 13: _pilotLesson.begin(); _pilotLesson.enter(); _state = State::PILOT_LESSON; break;  // Pilot
          case 14: _rnnLesson.begin(); _rnnLesson.enter(); _state = State::RNN_LESSON; break;  // Memory
          case 15: if (!_selfPlayLesson) _selfPlayLesson = new screens::SelfPlayLessonScreen();
                   _selfPlayLesson->begin(); _selfPlayLesson->enter(); _state = State::SELFPLAY_LESSON; break;  // Self-play
          case 16: _methodLesson.begin(); _methodLesson.enter(); _state = State::METHOD_LESSON; break;  // Right tool
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
    case State::TUNE_LESSON: {
      if (_tuneLesson && _tuneLesson->tick(now, tp) == Signal::BACK) { _lessonHub.enter(); _state = State::NEURO_HUB; }
      break;
    }
    case State::TUNENET_LESSON: {
      if (_tuneNetLesson && _tuneNetLesson->tick(now, tp) == Signal::BACK) { _lessonHub.enter(); _state = State::NEURO_HUB; }
      break;
    }
    case State::SELFPLAY_LESSON: {
      if (_selfPlayLesson && _selfPlayLesson->tick(now, tp) == Signal::BACK) { _lessonHub.enter(); _state = State::NEURO_HUB; }
      break;
    }
    case State::METHOD_LESSON: {
      if (_methodLesson.tick(now, tp) == Signal::BACK) { _lessonHub.enter(); _state = State::NEURO_HUB; }
      break;
    }
    case State::EVO_LESSON: {
      if (_evoLesson.tick(now, tp) == Signal::BACK) {
        if (_fromIntro) { _fromIntro = false; gotoIntro(_introLevel); }
        else { _lessonHub.enter(); _state = State::NEURO_HUB; }
      }
      break;
    }
    case State::TRANSFER_LESSON: {
      if (_transferLesson.tick(now, tp) == Signal::BACK) {
        if (_fromIntro) { _fromIntro = false; gotoIntro(_introLevel); }
        else { _lessonHub.enter(); _state = State::NEURO_HUB; }
      }
      break;
    }
    case State::BRAIN_VIEW: {
      if (_brainView.tick(now, tp) == Signal::BACK) { _lessonHub.enter(); _state = State::NEURO_HUB; }
      break;
    }
    case State::PILOT_LESSON: {
      if (_pilotLesson.tick(now, tp) == Signal::BACK) {
        if (_fromIntro) { _fromIntro = false; gotoIntro(_introLevel); }
        else { _lessonHub.enter(); _state = State::NEURO_HUB; }
      }
      break;
    }
    case State::RNN_LESSON: {
      if (_rnnLesson.tick(now, tp) == Signal::BACK) {
        if (_fromIntro) { _fromIntro = false; gotoIntro(_introLevel); }
        else { _lessonHub.enter(); _state = State::NEURO_HUB; }
      }
      break;
    }
    case State::PERCEPTION_LESSON: {
      if (_perceptionLesson.tick(now, tp) == Signal::BACK) { _lessonHub.enter(); _state = State::NEURO_HUB; }
      break;
    }
    case State::BACKPROP_LESSON: {
      if (_backpropLesson.tick(now, tp) == Signal::BACK) { _lessonHub.enter(); _state = State::NEURO_HUB; }
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
        _game.resumeCode(); _state = State::GAME;  // back to the editor, not the level preview
      }
      break;
    }
    case State::ARENA_TRAIN: {  // came from the Arena menu; return there (library may have grown)
      if (_arenaTrain.tick(now, tp) == Signal::BACK) {
        if (_arenaFromEditor) {   // opened from the editor: brain was written back -> return to it
          _arenaFromEditor = false;
          _profile.stats.brainsTrained++;
          _profile.achievements |= gb::evaluateAchievements(_profile);
          saveProfile();
          _game.resumeCode(); _state = State::GAME;
          break;
        }
        if (_arenaTrain.savedFighter()) _profile.stats.fightersSaved++;
        if (_arenaTrain.trainedBrain()) _profile.stats.brainsTrained++;  // training counts even without a save
        _profile.achievements |= gb::evaluateAchievements(_profile);
        saveProfile();
        if (_arenaTrain.launchFight()) {   // "Fight! >": go straight into a battle vs the sparring foe
          _arena.beginQuickBattle(&_profile, _arenaTrain.fightLibIdx(), _arenaTrain.fightOppName(), _arenaTrain.fightType());
        } else {
          _arena.begin(&_profile); _arena.enter();
        }
        _state = State::ARENA;
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
      else if (s == Signal::GOTO_ROOM) { _arena.begin(&_profile); _arena.enterRoom(); _state = State::ARENA; }
      break;
    }
    case State::BADGES: {
      Signal s = _badges.tick(now, tp);
      if (s == Signal::BACK) returnToSub();
      break;
    }
    case State::SHOP: {
      Signal s = _shop.tick(now, tp);
      if (s == Signal::BACK) { saveProfile(); returnToSub(); }
      break;
    }
  }
}

// ----- long-press Menu modal + the Sound modal it opens -------------------------------------
static const ui::Rect SND_VOL_DN = {52, 78, 40, 32}, SND_VOL_UP = {228, 78, 40, 32};
static const ui::Rect SND_MUSIC = {52, 120, 216, 28};
static const ui::Rect SND_SFX   = {52, 154, 216, 28};
static const ui::Rect SND_DONE  = {110, 190, 100, 28};

// The Menu modal (held-anywhere): Home / Back on top, Sound + Close below.
static const ui::Rect MENU_HOME  = {54, 78, 96, 34};
static const ui::Rect MENU_BACK  = {170, 78, 96, 34};
static const ui::Rect MENU_SOUND = {54, 122, 212, 30};
static const ui::Rect MENU_CLOSE = {54, 160, 212, 30};
static constexpr uint32_t LONGPRESS_MS = 550;   // hold this long to open the menu
static constexpr int LONGPRESS_TOL = 14;        // ...without sliding more than this (so drags don't trigger)

void App::drawMenuModal() {
  auto& g = hal::display.gfx();
  g.fillRoundRect(40, 44, 240, 164, 12, C_PANEL);
  g.drawRoundRect(40, 44, 240, 164, 12, C_ACCENT);
  ui::label(g, SCREEN_W / 2, 54, "Menu", C_ACCENT, textdatum_t::top_center, 2);
  ui::button(g, MENU_HOME,  "Home",   C_GO,     C_PANEL_HI);
  ui::button(g, MENU_BACK,  "< Back", C_ACCENT, C_PANEL_HI);
  ui::button(g, MENU_SOUND, "Sound",  C_INK,    C_PANEL_HI);
  ui::button(g, MENU_CLOSE, "Close",  C_DIM,    C_PANEL_HI);
}

void App::drawSoundModal() {
  auto& g = hal::display.gfx();
  g.fillRoundRect(34, 38, 252, 174, 12, C_PANEL);
  g.drawRoundRect(34, 38, 252, 174, 12, C_ACCENT);
  ui::label(g, SCREEN_W / 2, 50, "Sound", C_ACCENT, textdatum_t::top_center, 2);
  // volume row: [-]  ||||  [+]
  ui::button(g, SND_VOL_DN, "-", C_INK, C_PANEL_HI);
  ui::button(g, SND_VOL_UP, "+", C_INK, C_PANEL_HI);
  ui::label(g, SCREEN_W / 2, 84, "Volume", C_DIM, textdatum_t::top_center);
  for (int i = 0; i < 3; i++) {
    bool on = hal::audio.volume() > i;
    g.fillRoundRect(120 + i * 24, 92, 18, 18, 3, on ? C_GO : C_PANEL_HI);
  }
  ui::button(g, SND_MUSIC, hal::audio.musicOn() ? "Music: ON" : "Music: muted",
             hal::audio.musicOn() ? C_GO : C_DIM, C_PANEL_HI);
  ui::button(g, SND_SFX, hal::audio.sfxOn() ? "Sound FX: ON" : "Sound FX: muted",
             hal::audio.sfxOn() ? C_GO : C_DIM, C_PANEL_HI);
  ui::button(g, SND_DONE, "Done", C_GO, C_PANEL_HI);
}

bool App::handleSoundUi(int tx, int ty, bool tapped) {
  if (_soundModal) {
    if (tapped) {
      if (SND_VOL_DN.contains(tx, ty))      { hal::audio.setVolume(hal::audio.volume() - 1); hal::audio.blip(); }
      else if (SND_VOL_UP.contains(tx, ty)) { hal::audio.setVolume(hal::audio.volume() + 1); hal::audio.blip(); }
      else if (SND_MUSIC.contains(tx, ty))  { hal::audio.setMusicOn(!hal::audio.musicOn());
        if (hal::audio.musicOn() && saverEligible()) hal::audio.startMusic(hal::kTitleMusic, hal::kTitleMusicLen, true); }
      else if (SND_SFX.contains(tx, ty))    { hal::audio.setSfxOn(!hal::audio.sfxOn()); hal::audio.blip(); }
      else if (SND_DONE.contains(tx, ty))   { _soundModal = false; applyAndSaveSound(); wake(); return true; }
      drawSoundModal();
    }
    return true;   // modal swallows every tap while open
  }
  return false;
}

// Long-press anywhere opens the Menu modal; while it's open it owns every tap. Returns true when it
// consumed the input (so the screen underneath is frozen this frame).
bool App::handleMenuUi(uint32_t now, const hal::TouchPoint& tp, bool tap) {
  if (_menuModal) {
    if (tap) {
      _pressStart = 0;
      if (MENU_HOME.contains(tp.x, tp.y))       { _menuModal = false; _profile.id.empty() ? gotoSelect() : gotoHome(); }
      else if (MENU_BACK.contains(tp.x, tp.y))  { _menuModal = false; goBack(); }
      else if (MENU_SOUND.contains(tp.x, tp.y)) { _menuModal = false; _soundModal = true; drawSoundModal(); }
      else if (MENU_CLOSE.contains(tp.x, tp.y)) { _menuModal = false; wake(); }
    }
    return true;
  }
  if (_soundModal) { _pressStart = 0; return false; }            // the Sound modal owns input; no hold over it
  if (_state == State::DRAW) { _pressStart = 0; return false; }   // pixel editor uses press-and-drag to paint
  if (tp.pressed) {
    if (_pressStart == 0) { _pressStart = now; _pressX = tp.x; _pressY = tp.y; _longFired = false; }
    else if (!_longFired && now - _pressStart >= LONGPRESS_MS) {
      int dx = tp.x - _pressX, dy = tp.y - _pressY;
      if (dx * dx + dy * dy <= LONGPRESS_TOL * LONGPRESS_TOL) {   // a hold, not a slide
        _longFired = true; _menuModal = true; drawMenuModal();
        return true;
      }
    }
  } else {
    _pressStart = 0;
  }
  return false;
}

// Menu "Back": re-enter the screen we were on before this one (transient screens fall back to Home).
void App::goBack() { enterState(_prevState); }

void App::enterState(State s) {
  if (_profile.id.empty()) { gotoSelect(); return; }   // no profile loaded -> only player-select is valid
  switch (s) {
    case State::SELECT:       gotoSelect(); break;
    case State::HOME:         gotoHome();   break;
    case State::ARENA:        _arena.begin(&_profile); _arena.enter(); _state = State::ARENA; break;
    case State::LIBRARY:      _library.begin(&_profile); _library.enter(); _state = State::LIBRARY; break;
    case State::STATS:        _stats.begin(&_profile); _stats.enter(); _state = State::STATS; break;
    case State::LESSONS_MENU: _lessonsMenu.enter(); _state = State::LESSONS_MENU; break;
    case State::NEURO_HUB:    _lessonHub.enter(); _state = State::NEURO_HUB; break;
    case State::BADGES:       _subFromHome = true; _badges.begin(&_profile); _badges.enter(); _state = State::BADGES; break;
    case State::SHOP:         _subFromHome = true; _shop.begin(&_profile); _shop.enter(); _state = State::SHOP; break;
    default:                  gotoHome(); break;   // GAME/INTRO/matches/training/etc. -> the hub
  }
}

void App::applyAndSaveSound() {
  if (_profile.id.empty()) return;   // nothing to persist (e.g. at the player-select screen)
  _profile.settings.music = hal::audio.musicOn();
  _profile.settings.sfx = hal::audio.sfxOn();
  _profile.settings.volume = (uint8_t)hal::audio.volume();
  _profile.settings.sound = hal::audio.enabled();
  saveProfile();
}

}  // namespace app
