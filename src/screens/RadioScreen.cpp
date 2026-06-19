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
static const Rect R_BATTLE  = {40, 70, 240, 34};
static const Rect R_TRADE   = {40, 120, 240, 34};

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
  // My bot: latest library entry, else the wall-follower.
  if (profile && !profile->library.empty()) _mine = profile->library.back().program;
  else _mine = wallFollowerProgram();
}

void RadioScreen::enter() { drawChoice(); }

void RadioScreen::drawChoice() {
  _phase = Phase::CHOICE;
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 4, "Radio - find a friend", C_ACCENT);
  button(g, R_BATTLE, "Battle a friend", C_GO, C_PANEL);
  button(g, R_TRADE, "Trade a bot", C_FUNC, C_PANEL);
  label(g, SCREEN_W / 2, 160, "(needs a friend's CYD nearby)", C_DIM, textdatum_t::top_center);
  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
}

void RadioScreen::startLink(bool trade) {
  _trade = trade;
  _phase = Phase::LINKING;
  net::BotCard mine;
  mine.name = _profile ? String(_profile->name.c_str()) : String("Player");
  mine.avatar = _myAvatar;
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
      e.name = std::string(tc.name.c_str()) + "'s bot";
      e.program = _theirs;
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
  _arena.setup(&_maze, p0, p1, _s0, _s1, MatchType::RACE);
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
                        tile, avatar, p.facing);
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
    drawCell(b0.row, b0.col); drawCell(b1.row, b1.col);
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
    net::radio.stop(); hal::led.off();
    return app::Signal::BACK;
  }
  if (tap && _phase == Phase::CHOICE) {
    if (R_BATTLE.contains(tx, ty)) startLink(false);
    else if (R_TRADE.contains(tx, ty)) startLink(true);
  }
  return app::Signal::NONE;
}

}  // namespace screens
