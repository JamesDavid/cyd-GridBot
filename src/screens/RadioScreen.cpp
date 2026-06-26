#include "screens/RadioScreen.h"
#include <ArduinoJson.h>
#include "hal/Audio.h"
#include "hal/Led.h"
#include "assets/Assets.h"
#include "net/Radio.h"
#include "game/MazeGen.h"
#include "game/Bots.h"
#include "game/ProgramJson.h"

using namespace ui;
using namespace gb;

namespace screens {

static const Rect R_BACK    = {6, (int16_t)(BOTBAR_Y + 2), 100, 26};
static const Rect R_BATTLE  = {40, 58,  240, 34};
static const Rect R_TRADE   = {40, 100, 240, 34};
static const Rect R_ROOM    = {40, 142, 240, 34};  // multi-device tournament Room (ESP-NOW)

static String programToJsonString(const Program& p) {
  JsonDocument doc;
  programToJson(p, doc.to<JsonObject>());
  String s;
  serializeJson(doc, s);
  return s;
}
static Program programFromJsonString(const String& s) {
  Program p;
  JsonDocument doc;
  if (deserializeJson(doc, s) == DeserializationError::Ok)
    programFromJson(doc.as<JsonObjectConst>(), p);
  return p;
}

void RadioScreen::begin(Profile* profile) {
  _profile = profile;
  _phase = Phase::CHOICE;
  _myAvatar = profile ? profile->avatar : 0;
  _pickIdx = -1;   // default to the most recent fighter until the picker chooses
}

void RadioScreen::enter() { drawChoice(); }

void RadioScreen::drawChoice() {
  _phase = Phase::CHOICE;
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Radio Friend", C_ACCENT, textdatum_t::top_left, 2);
  button(g, R_BATTLE, "Battle a friend", C_GO, C_PANEL);
  button(g, R_TRADE, "Trade a bot", C_FUNC, C_PANEL);
  button(g, R_ROOM, "Room - tournament w/ friends", C_ACCENT, C_PANEL);
  label(g, SCREEN_W / 2, 182, "(needs a friend's CYD nearby)", C_DIM, textdatum_t::top_center);
  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
}

// One library row's rect -- identical maths in draw and hit-test so taps land where they look.
ui::Rect RadioScreen::pickRowRect(int i, int n) const {
  int top = TOPBAR_H + 24, bottom = BOTBAR_Y - 4;
  int rowh = (bottom - top) / (n < 1 ? 1 : n);
  if (rowh > 30) rowh = 30; if (rowh < 14) rowh = 14;
  return ui::Rect{12, (int16_t)(top + i * rowh), (int16_t)(SCREEN_W - 24), (int16_t)(rowh - 3)};
}

void RadioScreen::drawPick() {
  _phase = Phase::PICK;
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, _trade ? "Share which bot?" : "Pick your fighter", C_ACCENT, textdatum_t::top_left, 2);
  int n = _profile ? (int)_profile->library.size() : 0;
  if (n == 0) {
    label(g, SCREEN_W / 2, 100, "No saved bots - using default", C_DIM, textdatum_t::middle_center);
  } else {
    label(g, SCREEN_W / 2, TOPBAR_H + 8, _trade ? "tap a bot to send it" : "tap a bot to send into battle",
          C_DIM, textdatum_t::top_center);
    for (int i = 0; i < n; i++) {
      const auto& e = _profile->library[i];
      bool isLast = (i == n - 1);   // the default (most recently saved)
      button(g, pickRowRect(i, n), e.name.c_str(), isLast ? C_GO : C_INK, C_PANEL);
    }
  }
  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
}

void RadioScreen::startLink(bool trade) {
  _trade = trade;
  _phase = Phase::LINKING;
  // Resolve the chosen fighter (picker index, else most-recent, else the wall-follower default).
  String botName;
  if (_profile && !_profile->library.empty()) {
    int n = (int)_profile->library.size();
    int idx = (_pickIdx >= 0 && _pickIdx < n) ? _pickIdx : n - 1;
    _mine = _profile->library[idx].program;
    botName = String(_profile->library[idx].name.c_str());
  } else {
    _mine = wallFollowerProgram();
  }
  net::BotCard mine;
  mine.name = _profile ? String(_profile->name.c_str()) : String("Player");
  mine.avatar = _myAvatar;
  mine.uuid = _profile ? String(_profile->uuid.c_str()) : String("");
  mine.botName = botName;   // the fighter the picker chose (sent to my friend)
  mine.progJson = programToJsonString(_mine);
  net::radio.begin();
  net::radio.startSession(mine);
  drawLinking();
}

void RadioScreen::drawLinking() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 4, _trade ? "Trade - linking..." : "Battle - linking...", C_ACCENT);
  const char* st = "Searching for a friend...";
  switch (net::radio.status()) {
    case net::RadioStatus::SEARCHING:  st = "Searching for a friend..."; break;
    case net::RadioStatus::EXCHANGING: st = "Friend found! sharing..."; break;
    case net::RadioStatus::READY:      st = "Ready!"; break;
    case net::RadioStatus::FAILED:     st = "Radio error"; break;
    default: break;
  }
  label(g, SCREEN_W / 2, 110, st, C_INK, textdatum_t::middle_center);
  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Cancel", C_INK, C_PANEL);
}

void RadioScreen::onReady() {
  const net::BotCard& tc = net::radio.theirCard();
  _theirs = programFromJsonString(tc.progJson);
  _theirAvatar = tc.avatar;

  if (_trade) {
    // Pokemon-style: drop the friend's bot into the library, bounded (SPEC §11.1).
    if (_profile && (int)_profile->library.size() < gb::LIBRARY_MAX) {
      gb::LibEntry e;
      // keep the name the friend gave the bot; fall back to "<friend>'s bot" for old senders
      e.name = tc.botName.length() ? std::string(tc.botName.c_str())
                                    : std::string(tc.name.c_str()) + "'s bot";
      e.program = _theirs;
      e.source = gb::LIB_RADIO;
      _profile->library.push_back(e);
    }
    _phase = Phase::TRADED;
    auto& g = hal::display.gfx();
    g.fillScreen(C_BG);
    label(g, SCREEN_W / 2, 90, "Traded!", C_GO, textdatum_t::middle_center, 2);
    char buf[40]; snprintf(buf, sizeof(buf), "Got %s's bot", tc.name.c_str());
    label(g, SCREEN_W / 2, 130, buf, C_INK, textdatum_t::middle_center);
    assets::drawCharacter(g, SCREEN_W / 2, 170, 28, _theirAvatar, gb::SOUTH);
    button(g, R_BACK, "< Back", C_INK, C_PANEL);
    hal::audio.win();
    return;
  }

  // Battle: deterministic match from the shared seed; role from MAC order so both
  // devices render the identical match (SPEC §18.1).
  MazeGen::generateArena(_maze, net::radio.sharedSeed(), _s0, _s1);
  const Program* p0 = net::radio.iAmBot0() ? &_mine : &_theirs;
  const Program* p1 = net::radio.iAmBot0() ? &_theirs : &_mine;
  _arena.setup(&_maze, p0, p1, _s0, _s1, MatchType::SUMO);   // "Battle a friend" = a real sumo fight
  _phase = Phase::BATTLE;
  _last = 0;
  drawBoard();
}

void RadioScreen::mazeGeometry(int& tile, int& ox, int& oy) {
  int cols = _maze.cols(), rows = _maze.rows();
  tile = SCREEN_W / cols;
  int th = BAND_H / rows; if (th < tile) tile = th;
  ox = (SCREEN_W - tile * cols) / 2;
  oy = BAND_Y + (BAND_H - tile * rows) / 2;
}
void RadioScreen::drawCell(int r, int c) {
  auto& g = hal::display.gfx();
  int tile, ox, oy; mazeGeometry(tile, ox, oy);
  int x = ox + c * tile, y = oy + r * tile;
  Tile t = _maze.at(r, c);
  uint16_t bg = ((r + c) & 1) ? C_FLOOR : C_FLOOR2;
  if (t == WALL) bg = C_WALL; else if (t == PIT) bg = C_PIT;
  g.fillRect(x, y, tile - 1, tile - 1, bg);
  if (_maze.isGoal(r, c)) { g.fillCircle(x + tile / 2, y + tile / 2, tile / 3, C_ACCENT);
    label(g, x + tile / 2, y + tile / 2, "G", C_BG, textdatum_t::middle_center); }
}
void RadioScreen::drawBot(int i, const Pose& p, int avatar) {
  auto& g = hal::display.gfx();
  int tile, ox, oy; mazeGeometry(tile, ox, oy);
  assets::drawCharacter(g, ox + p.col * tile + tile / 2, oy + p.row * tile + tile / 2,
                        tile, avatar, p.facing, i == 0 ? C_GO : C_BAD);  // bot 0 green / bot 1 red
}
void RadioScreen::drawBoard() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 4, "Radio Battle!", C_ACCENT);
  for (int r = 0; r < _maze.rows(); r++)
    for (int c = 0; c < _maze.cols(); c++) drawCell(r, c);
  bool me0 = net::radio.iAmBot0();
  drawBot(0, _arena.pose(0), me0 ? _myAvatar : _theirAvatar);
  drawBot(1, _arena.pose(1), me0 ? _theirAvatar : _myAvatar);
  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
}

app::Signal RadioScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  bool tap = _tap.tapped(tp, now, tx, ty);

  if (_phase == Phase::LINKING) {
    net::radio.loop(now);
    static net::RadioStatus prev = net::RadioStatus::IDLE;
    if (net::radio.status() != prev) { prev = net::radio.status(); drawLinking(); }
    if (net::radio.ready()) onReady();
  } else if (_phase == Phase::BATTLE && now - _last >= 250) {
    _last = now;
    Pose b0 = _arena.pose(0), b1 = _arena.pose(1);
    ArenaOutcome o = _arena.tick();
    // redraw each vacated cell + its neighbours: the robot's facing arrow overhangs the next tile,
    // so clearing only the one cell leaves yellow specks as it moves.
    auto erase = [&](int r, int c) {
      for (int dr = -1; dr <= 1; dr++)
        for (int dc = -1; dc <= 1; dc++) {
          int rr = r + dr, cc = c + dc;
          if (rr >= 0 && rr < _maze.rows() && cc >= 0 && cc < _maze.cols()) drawCell(rr, cc);
        }
    };
    erase(b0.row, b0.col); erase(b1.row, b1.col);
    bool me0 = net::radio.iAmBot0();
    drawBot(0, _arena.pose(0), me0 ? _myAvatar : _theirAvatar);
    drawBot(1, _arena.pose(1), me0 ? _theirAvatar : _myAvatar);
    if (o != ArenaOutcome::RUNNING) {
      _phase = Phase::DONE;
      bool iWon = (me0 && o == ArenaOutcome::BOT0) || (!me0 && o == ArenaOutcome::BOT1);
      const char* msg = (o == ArenaOutcome::DRAW) ? "Draw!" : (iWon ? "You win!" : "Friend wins");
      auto& g = hal::display.gfx();
      int w = 200, h = 50, x = (SCREEN_W - w) / 2, y = (SCREEN_H - h) / 2;
      g.fillRoundRect(x, y, w, h, 8, C_PANEL);
      label(g, SCREEN_W / 2, y + h / 2, msg, iWon ? C_GO : C_ACCENT, textdatum_t::middle_center, 2);
      if (iWon) { hal::led.green(); hal::audio.win(); } else hal::audio.fail();
    }
  }

  if (tap && R_BACK.contains(tx, ty)) {
    if (_phase == Phase::PICK) { drawChoice(); return app::Signal::NONE; }  // back to Battle/Trade/Room
    net::radio.stop(); hal::led.off();
    return app::Signal::BACK;
  }
  if (tap && _phase == Phase::CHOICE) {
    if (R_ROOM.contains(tx, ty)) return app::Signal::GOTO_ROOM;   // hand off to the Arena's Room lobby
    if (R_BATTLE.contains(tx, ty)) { _trade = false; drawPick(); }   // pick a fighter, then link
    else if (R_TRADE.contains(tx, ty)) { _trade = true; drawPick(); }
  } else if (tap && _phase == Phase::PICK) {
    int n = _profile ? (int)_profile->library.size() : 0;
    if (n == 0) { startLink(_trade); }                           // no library -> default bot
    else for (int i = 0; i < n; i++)
      if (pickRowRect(i, n).contains(tx, ty)) {
        _pickIdx = (int8_t)i;
        hal::audio.blip();
        startLink(_trade);
        break;
      }
  }
  return app::Signal::NONE;
}

}  // namespace screens
