#include "screens/ArenaScreen.h"
#include "hal/Audio.h"
#include "hal/Led.h"
#include "assets/Assets.h"
#include "game/MazeGen.h"
#include "game/Bots.h"

using namespace ui;
using namespace gb;

namespace screens {

static const Rect R_BACK = {6, (int16_t)(BOTBAR_Y + 2), 120, 26};
static const Rect R_GO   = {164, (int16_t)(BOTBAR_Y + 2), 150, 26};
static constexpr int AI_AVATAR = 5;  // Robot opponent

void ArenaScreen::begin(Profile* profile) {
  _profile = profile;
  MazeGen::generateArena(_maze, profile ? profile->seedBase + 7u : 7u, _s0, _s1);
  // Player's bot: their most recent library entry, else the wall-follower.
  if (profile && !profile->library.empty()) _p0 = profile->library.back().program;
  else _p0 = wallFollowerProgram();
  _p1 = wallFollowerProgram();  // house bot
  _arena.setup(&_maze, &_p0, &_p1, _s0, _s1, MatchType::RACE);
  _drawn[0] = _s0; _drawn[1] = _s1;
  _running = false; _done = false;
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
  if (_maze.isGoal(r, c)) {
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
  label(g, 6, 4, "Arena - Race", C_ACCENT);
  label(g, SCREEN_W - 6, 4, "you vs House", C_DIM, textdatum_t::top_right);
  for (int r = 0; r < _maze.rows(); r++)
    for (int c = 0; c < _maze.cols(); c++) drawCell(r, c);
  drawBot(0, _arena.pose(0), _profile ? _profile->avatar : 0);
  drawBot(1, _arena.pose(1), AI_AVATAR);
  _drawn[0] = _arena.pose(0); _drawn[1] = _arena.pose(1);
  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
  button(g, R_GO, _running ? "..." : "FIGHT!", C_GO, C_PANEL);
}

void ArenaScreen::enter() { drawBoard(); }

void ArenaScreen::finishOverlay() {
  auto& g = hal::display.gfx();
  const char* msg = "Draw!";
  uint16_t col = C_ACCENT;
  switch (_arena.outcome()) {
    case ArenaOutcome::BOT0: msg = "You win!"; col = C_GO; hal::led.green(); break;
    case ArenaOutcome::BOT1: msg = "House wins"; col = C_BAD; hal::led.red(); break;
    default: msg = "Draw!"; break;
  }
  int w = 200, h = 60, x = (SCREEN_W - w) / 2, y = (SCREEN_H - h) / 2;
  g.fillRoundRect(x, y, w, h, 10, C_PANEL);
  g.drawRoundRect(x, y, w, h, 10, col);
  label(g, SCREEN_W / 2, y + 22, msg, col, textdatum_t::middle_center, 2);
  label(g, SCREEN_W / 2, y + 44, "tap Back to exit", C_DIM, textdatum_t::middle_center);
  if (_arena.outcome() == ArenaOutcome::BOT0) hal::audio.win();
  else if (_arena.outcome() == ArenaOutcome::BOT1) hal::audio.fail();
}

app::Signal ArenaScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  bool tap = _tap.tapped(tp, now, tx, ty);

  if (_running && !_done && now - _last >= 250) {
    _last = now;
    Pose b0 = _arena.pose(0), b1 = _arena.pose(1);
    ArenaOutcome o = _arena.tick();
    // dirty-rect: clear old bot cells, redraw both bots
    drawCell(b0.row, b0.col); drawCell(b1.row, b1.col);
    drawBot(0, _arena.pose(0), _profile ? _profile->avatar : 0);
    drawBot(1, _arena.pose(1), AI_AVATAR);
    hal::audio.tick();
    if (o != ArenaOutcome::RUNNING) { _done = true; _running = false; finishOverlay(); }
  }

  if (tap) {
    if (R_BACK.contains(tx, ty)) { hal::led.off(); return app::Signal::BACK; }
    if (R_GO.contains(tx, ty) && !_running && !_done) {
      _running = true; _last = 0;
      drawBoard();
    }
  }
  return app::Signal::NONE;
}

}  // namespace screens
