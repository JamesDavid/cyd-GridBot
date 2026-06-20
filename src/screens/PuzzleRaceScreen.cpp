#include "screens/PuzzleRaceScreen.h"
#include "hal/Audio.h"
#include "assets/Assets.h"
#include "game/MazeGen.h"
#include "game/Interpreter.h"
#include "game/Score.h"

using namespace ui;
using namespace gb;

namespace screens {

static constexpr uint32_t AUTHOR_MS = 45000;  // 45s per player
// command pad: 6 buttons, 2 cols x 3 rows (index 5 is CLEAR, drawn as a label)
static const Glyph CMD_GLYPH[5] = {Glyph::ARROW_UP, Glyph::ARROW_DOWN, Glyph::TURN_L,
                                   Glyph::TURN_R, Glyph::JUMP};
static const Rect R_DONE = {6, (int16_t)(BOTBAR_Y + 2), 150, 26};
static const Rect R_READY = {84, 150, 150, 34};
static const Rect R_AGAIN = {40, 150, 100, 30};
static const Rect R_EXIT  = {180, 150, 100, 30};
static constexpr int LIST_X = 216;

void PuzzleRaceScreen::begin(Profile* profile) {
  _profile = profile;
  // one shared maze (a meaty mid-size board); both players solve the same one
  MazeGen::generate(_maze, profile ? profile->seedBase + 999u : 999u, 9);
  _prog[0].clear(); _prog[1].clear();
  _dist[0] = _dist[1] = -1;
  _player = 0;
  _phase = Phase::AUTHOR;
}

ui::Rect PuzzleRaceScreen::cmdRect(int i) const {
  int col = i % 2, row = i / 2;
  return {(int16_t)(120 + col * 50), (int16_t)(BAND_Y + 2 + row * 40), 46, 36};
}

void PuzzleRaceScreen::enter() {
  _deadline = 0;  // armed on first tick
  drawAuthor();
}

void PuzzleRaceScreen::drawThumb() {
  auto& g = hal::display.gfx();
  int cols = _maze.cols(), rows = _maze.rows();
  int tile = 104 / cols; if (130 / rows < tile) tile = 130 / rows;
  int ox = 6, oy = BAND_Y + 4;
  for (int r = 0; r < rows; r++)
    for (int c = 0; c < cols; c++) {
      int x = ox + c * tile, y = oy + r * tile;
      Tile t = _maze.at(r, c);
      uint16_t col = ((r + c) & 1) ? C_FLOOR : C_FLOOR2;
      if (t == WALL) col = C_WALL; else if (t == PIT) col = C_BG;
      g.fillRect(x, y, tile - 1, tile - 1, col);
      if (_maze.isGoal(r, c)) g.fillCircle(x + tile / 2, y + tile / 2, tile / 3, C_GO);
    }
  Pose s = _maze.startPose();
  assets::drawCharacter(g, ox + s.col * tile + tile / 2, oy + s.row * tile + tile / 2,
                        tile, _profile ? _profile->avatar : 0, s.facing);
}

void PuzzleRaceScreen::drawList() {
  auto& g = hal::display.gfx();
  g.fillRect(LIST_X, BAND_Y, SCREEN_W - LIST_X, BAND_H, C_PANEL);
  NodeList& list = _prog[_player].main;
  char hdr[16]; snprintf(hdr, sizeof(hdr), "%d steps", (int)list.size());
  label(g, LIST_X + 6, BAND_Y + 4, hdr, C_DIM);
  int visible = (BAND_H - 20) / 16;
  int start = (int)list.size() > visible ? (int)list.size() - visible : 0;
  for (int i = start; i < (int)list.size(); i++) {
    int y = BAND_Y + 20 + (i - start) * 16;
    const char* nm = "?";
    switch (list[i].cmd) {
      case CMD_FWD: nm = "fwd"; break; case CMD_BACK: nm = "back"; break;
      case CMD_TURN_L: nm = "turnL"; break; case CMD_TURN_R: nm = "turnR"; break;
      case CMD_JUMP: nm = "jump"; break; default: break;
    }
    label(g, LIST_X + 6, y, nm, C_INK);
  }
}

void PuzzleRaceScreen::drawAuthor() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  char t[24]; snprintf(t, sizeof(t), "Player %d - code it!", _player + 1);
  label(g, 6, 4, t, _player == 0 ? C_GO : C_FUNC);
  drawThumb();
  for (int i = 0; i < 6; i++) {
    Rect r = cmdRect(i);
    uint16_t fg = (i == 5) ? C_BAD : C_MOVE;
    if (i == 5) {
      button(g, r, "CLR", fg, C_PANEL);  // glyph set has no eraser; a clear label reads better
    } else {
      button(g, r, "", fg, C_PANEL);
      drawGlyph(g, CMD_GLYPH[i], r.cx(), r.cy(), 18, fg);
    }
  }
  drawList();
  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_DONE, "DONE >", C_GO, C_PANEL);
  // timer drawn each tick over the top bar
}

void PuzzleRaceScreen::drawHandoff() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  label(g, SCREEN_W / 2, 60, "Player 1 locked in!", C_GO, textdatum_t::top_center, 2);
  label(g, SCREEN_W / 2, 100, "Pass the device to Player 2", C_INK, textdatum_t::top_center);
  button(g, R_READY, "Ready >", C_ACCENT, C_PANEL);
}

int PuzzleRaceScreen::scoreProgram(const Program& p) {
  Interpreter it;
  it.load(&p, &_maze, _maze.startPose());
  it.runToEnd();
  return distanceToGoal(_maze, it.pose().row, it.pose().col);
}

void PuzzleRaceScreen::drawResult() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Puzzle Race", C_ACCENT, textdatum_t::top_left, 2);
  // lower distance wins (0 = reached the goal); -1 (unreachable, fell) treated as worst
  int d0 = _dist[0] < 0 ? 999 : _dist[0];
  int d1 = _dist[1] < 0 ? 999 : _dist[1];
  char b[40];
  snprintf(b, sizeof(b), "Player 1: %s", _dist[0] == 0 ? "reached the goal!" :
           _dist[0] < 0 ? "fell / stuck" : "");
  label(g, 30, 50, b, C_GO);
  if (_dist[0] > 0) { snprintf(b, sizeof(b), "%d tiles away", _dist[0]); label(g, 200, 50, b, C_INK); }
  snprintf(b, sizeof(b), "Player 2: %s", _dist[1] == 0 ? "reached the goal!" :
           _dist[1] < 0 ? "fell / stuck" : "");
  label(g, 30, 70, b, C_FUNC);
  if (_dist[1] > 0) { snprintf(b, sizeof(b), "%d tiles away", _dist[1]); label(g, 200, 70, b, C_INK); }

  const char* msg = (d0 == d1) ? "It's a tie!" : (d0 < d1 ? "Player 1 wins!" : "Player 2 wins!");
  uint16_t col = (d0 == d1) ? C_ACCENT : (d0 < d1 ? C_GO : C_FUNC);
  label(g, SCREEN_W / 2, 110, msg, col, textdatum_t::top_center, 2);
  button(g, R_AGAIN, "Again", C_GO, C_PANEL);
  button(g, R_EXIT, "Exit", C_INK, C_PANEL);
}

void PuzzleRaceScreen::lockIn(uint32_t now) {
  (void)now;
  _dist[_player] = scoreProgram(_prog[_player]);
  hal::audio.win();
  if (_player == 0) { _phase = Phase::HANDOFF; drawHandoff(); }
  else { _phase = Phase::RESULT; drawResult(); }
}

app::Signal PuzzleRaceScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  bool tap = _tap.tapped(tp, now, tx, ty);

  if (_phase == Phase::AUTHOR) {
    if (_deadline == 0) { _deadline = now + AUTHOR_MS; _shownSec = -1; }
    // countdown in the top bar (only repaint when the second changes)
    int rem = (int)((_deadline - now) / 1000);
    if (rem < 0) rem = 0;
    if (rem != _shownSec) {
      _shownSec = rem;
      auto& g = hal::display.gfx();
      g.fillRect(220, 2, 96, 18, C_PANEL);
      char tm[12]; snprintf(tm, sizeof(tm), "0:%02d", rem);
      label(g, SCREEN_W - 6, 4, tm, rem <= 10 ? C_BAD : C_INK, textdatum_t::top_right);
    }
    if (now >= _deadline) { lockIn(now); return app::Signal::NONE; }

    if (tap) {
      if (R_DONE.contains(tx, ty)) { lockIn(now); return app::Signal::NONE; }
      for (int i = 0; i < 6; i++) {
        if (!cmdRect(i).contains(tx, ty)) continue;
        if (i == 5) _prog[_player].main.clear();
        else {
          const Cmd cmds[5] = {CMD_FWD, CMD_BACK, CMD_TURN_L, CMD_TURN_R, CMD_JUMP};
          _prog[_player].main.push_back(Node::command(cmds[i]));
        }
        hal::audio.blip();
        drawList();
        return app::Signal::NONE;
      }
    }
    return app::Signal::NONE;
  }

  if (_phase == Phase::HANDOFF) {
    if (tap && R_READY.contains(tx, ty)) {
      _player = 1; _deadline = 0; _phase = Phase::AUTHOR; drawAuthor();
    }
    return app::Signal::NONE;
  }

  // RESULT
  if (tap) {
    if (R_AGAIN.contains(tx, ty)) { begin(_profile); enter(); }
    else if (R_EXIT.contains(tx, ty)) return app::Signal::BACK;
  }
  return app::Signal::NONE;
}

}  // namespace screens
