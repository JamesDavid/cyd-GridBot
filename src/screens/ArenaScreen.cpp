#include "screens/ArenaScreen.h"
#include "hal/Audio.h"
#include "hal/Led.h"
#include "assets/Assets.h"
#include "game/MazeGen.h"
#include "game/Bots.h"

using namespace ui;
using namespace gb;

namespace screens {

static const Rect R_BACK = {6, (int16_t)(BOTBAR_Y + 2), 100, 26};
static const Rect R_TYPE = {110, (int16_t)(BOTBAR_Y + 2), 90, 26};
static const Rect R_AI   = {6,  60, 150, 30};
static const Rect R_HOT  = {164, 60, 150, 30};
static const Rect R_RADIO= {6, 100, 308, 26};
static const Rect R_READY= {84, 150, 150, 34};

static Program dashProgram() {
  Program p;
  Node loop = Node::repeatUntil(AT_GOAL);
  loop.body.push_back(Node::command(CMD_FWD));
  p.main.push_back(loop);
  return p;
}

void ArenaScreen::begin(Profile* profile) {
  _profile = profile;
  _type = MatchType::RACE;
  _hotseat = false;
  _pick0 = _pick1 = -1;
  _phase = Phase::MENU;
  _running = false;
  buildCandidates();
}

void ArenaScreen::buildCandidates() {
  _cands.clear();
  _cands.push_back({"Wall Hugger", wallFollowerProgram(), 0});
  _cands.push_back({"Dasher", dashProgram(), 3});
  _cands.push_back({"Hunter", hunterProgram(), 5});
  if (_profile) {
    for (auto& e : _profile->library) {
      if (_cands.size() >= 7) break;
      _cands.push_back({e.name, e.program, _profile->avatar});
    }
  }
}

void ArenaScreen::enter() { drawMenu(); }

// ---- menus ----------------------------------------------------------------
void ArenaScreen::drawMenu() {
  _phase = Phase::MENU;
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 4, "Arena", C_ACCENT);
  label(g, SCREEN_W / 2, 36, "Pick an opponent", C_INK, textdatum_t::top_center);
  button(g, R_AI, "vs House AI", C_GO, C_PANEL);
  button(g, R_HOT, "Hotseat 2P", C_FUNC, C_PANEL);
  button(g, R_RADIO, "Radio battle (needs 2 boards)", C_DIM, C_PANEL);
  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
  button(g, R_TYPE, _type == MatchType::RACE ? "Race" : "Sumo", C_ACCENT, C_PANEL);
}

void ArenaScreen::drawPick(int player) {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  char t[28]; snprintf(t, sizeof(t), "Player %d: pick a bot", player + 1);
  label(g, 6, 4, t, C_ACCENT);
  for (int i = 0; i < (int)_cands.size(); i++) {
    Rect r = {6, (int16_t)(BAND_Y + 4 + i * 24), 308, 22};
    button(g, r, _cands[i].name.c_str(), C_INK, C_PANEL);
  }
}

void ArenaScreen::drawHandoff() {
  _phase = Phase::HANDOFF;
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  label(g, SCREEN_W / 2, 60, "Player 1 locked in!", C_GO, textdatum_t::top_center, 2);
  label(g, SCREEN_W / 2, 100, "Pass the device to Player 2", C_INK, textdatum_t::top_center);
  button(g, R_READY, "Ready >", C_ACCENT, C_PANEL);
}

// ---- match ----------------------------------------------------------------
void ArenaScreen::startMatch() {
  MazeGen::generateArena(_maze, _profile ? _profile->seedBase + 7u : 7u, _s0, _s1);
  const Program& p0 = _cands[_pick0].prog;
  const Program& p1 = _cands[_pick1].prog;
  _arena.setup(&_maze, &p0, &p1, _s0, _s1, _type);
  _phase = Phase::BOARD;
  _running = true;
  _last = 0;
  drawBoard();
}

void ArenaScreen::mazeGeometry(int& tile, int& ox, int& oy) {
  int cols = _maze.cols(), rows = _maze.rows();
  tile = SCREEN_W / cols;
  int th = BAND_H / rows;
  if (th < tile) tile = th;
  ox = (SCREEN_W - tile * cols) / 2;
  oy = BAND_Y + (BAND_H - tile * rows) / 2;
}

void ArenaScreen::drawCell(int r, int c) {
  auto& g = hal::display.gfx();
  int tile, ox, oy; mazeGeometry(tile, ox, oy);
  int x = ox + c * tile, y = oy + r * tile;
  Tile t = _maze.at(r, c);
  uint16_t bg = ((r + c) & 1) ? C_FLOOR : C_FLOOR2;
  if (t == WALL) bg = C_WALL; else if (t == PIT) bg = C_PIT;
  g.fillRect(x, y, tile - 1, tile - 1, bg);
  if (t == PIT) g.fillCircle(x + tile / 2, y + tile / 2, tile / 3, C_BG);
  if (_type == MatchType::RACE && _maze.isGoal(r, c)) {
    g.fillCircle(x + tile / 2, y + tile / 2, tile / 3, C_ACCENT);
    label(g, x + tile / 2, y + tile / 2, "G", C_BG, textdatum_t::middle_center);
  }
}

void ArenaScreen::drawBot(int i, const Pose& p, int avatar) {
  auto& g = hal::display.gfx();
  int tile, ox, oy; mazeGeometry(tile, ox, oy);
  int cx = ox + p.col * tile + tile / 2, cy = oy + p.row * tile + tile / 2;
  assets::drawCharacter(g, cx, cy, tile, avatar, p.facing);
}

void ArenaScreen::drawBoard() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  char hdr[40];
  snprintf(hdr, sizeof(hdr), "%s: %s vs %s", _type == MatchType::RACE ? "Race" : "Sumo",
           _cands[_pick0].name.c_str(), _cands[_pick1].name.c_str());
  label(g, 6, 4, hdr, C_ACCENT);
  for (int r = 0; r < _maze.rows(); r++)
    for (int c = 0; c < _maze.cols(); c++) drawCell(r, c);
  drawBot(0, _arena.pose(0), _cands[_pick0].avatar);
  drawBot(1, _arena.pose(1), _cands[_pick1].avatar == _cands[_pick0].avatar
                                ? (_cands[_pick1].avatar + 4) % 8 : _cands[_pick1].avatar);
  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
}

void ArenaScreen::finishOverlay() {
  auto& g = hal::display.gfx();
  const char* msg = "Draw!"; uint16_t col = C_ACCENT;
  switch (_arena.outcome()) {
    case ArenaOutcome::BOT0: msg = "Player 1 wins!"; col = C_GO; hal::led.green(); hal::audio.win(); break;
    case ArenaOutcome::BOT1: msg = "Player 2 wins!"; col = C_FUNC; hal::led.red(); hal::audio.fail(); break;
    default: break;
  }
  int w = 220, h = 60, x = (SCREEN_W - w) / 2, y = (SCREEN_H - h) / 2;
  g.fillRoundRect(x, y, w, h, 10, C_PANEL);
  g.drawRoundRect(x, y, w, h, 10, col);
  label(g, SCREEN_W / 2, y + 22, msg, col, textdatum_t::middle_center, 2);
  label(g, SCREEN_W / 2, y + 44, "tap Back to exit", C_DIM, textdatum_t::middle_center);
}

// ---- input ----------------------------------------------------------------
app::Signal ArenaScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  bool tap = _tap.tapped(tp, now, tx, ty);

  if (_phase == Phase::BOARD && _running && now - _last >= 250) {
    _last = now;
    Pose b0 = _arena.pose(0), b1 = _arena.pose(1);
    ArenaOutcome o = _arena.tick();
    drawCell(b0.row, b0.col); drawCell(b1.row, b1.col);
    if (_arena.alive(0)) drawBot(0, _arena.pose(0), _cands[_pick0].avatar);
    if (_arena.alive(1)) drawBot(1, _arena.pose(1), _cands[_pick1].avatar);
    hal::audio.tick();
    if (o != ArenaOutcome::RUNNING) { _running = false; _phase = Phase::DONE; finishOverlay(); }
  }

  if (!tap) return app::Signal::NONE;

  switch (_phase) {
    case Phase::MENU:
      if (R_BACK.contains(tx, ty)) return app::Signal::BACK;
      if (R_TYPE.contains(tx, ty)) {
        _type = (_type == MatchType::RACE) ? MatchType::SUMO : MatchType::RACE;
        drawMenu();
      } else if (R_AI.contains(tx, ty)) {
        _hotseat = false; _phase = Phase::PICK1; drawPick(0);  // you pick your bot; house auto
      } else if (R_HOT.contains(tx, ty)) {
        _hotseat = true; _phase = Phase::PICK1; drawPick(0);
      }
      break;
    case Phase::PICK1:
      for (int i = 0; i < (int)_cands.size(); i++) {
        Rect r = {6, (int16_t)(BAND_Y + 4 + i * 24), 308, 22};
        if (r.contains(tx, ty)) {
          _pick0 = i; hal::audio.blip();
          if (_hotseat) { drawHandoff(); }
          else { _pick1 = (_type == MatchType::SUMO) ? 2 : 0; startMatch(); }  // house bot
          return app::Signal::NONE;
        }
      }
      break;
    case Phase::HANDOFF:
      if (R_READY.contains(tx, ty)) { _phase = Phase::PICK2; drawPick(1); }
      break;
    case Phase::PICK2:
      for (int i = 0; i < (int)_cands.size(); i++) {
        Rect r = {6, (int16_t)(BAND_Y + 4 + i * 24), 308, 22};
        if (r.contains(tx, ty)) { _pick1 = i; hal::audio.blip(); startMatch(); return app::Signal::NONE; }
      }
      break;
    case Phase::BOARD:
    case Phase::DONE:
      if (R_BACK.contains(tx, ty)) { hal::led.off(); return app::Signal::BACK; }
      break;
  }
  return app::Signal::NONE;
}

}  // namespace screens
