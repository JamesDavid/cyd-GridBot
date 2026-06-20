#include "screens/PuzzleRaceScreen.h"
#include "hal/Audio.h"
#include "assets/Assets.h"
#include "game/MazeGen.h"
#include "game/Interpreter.h"
#include "game/Score.h"

using namespace ui;
using namespace gb;

namespace screens {

static constexpr uint32_t AUTHOR_MS = 90000;  // 90s per player (programming takes longer)
static const Rect R_DONE = {6,   (int16_t)(BOTBAR_Y + 2), 150, 26};
static const Rect R_PEEK = {164, (int16_t)(BOTBAR_Y + 2), 150, 26};
static const Rect R_READY = {84, 150, 150, 34};
static const Rect R_AGAIN = {40, 150, 100, 30};
static const Rect R_EXIT  = {180, 150, 100, 30};

void PuzzleRaceScreen::begin(Profile* profile) {
  _profile = profile;
  // one shared maze (a meaty mid-size board); both players solve the same one. The
  // game counter persists across "Again" / re-entry so every round is a fresh board.
  uint32_t base = (profile ? profile->seedBase : 0) + 999u;
  MazeGen::generate(_maze, base + _gameNo * 1009u, 9);
  _gameNo++;
  _prog[0].clear(); _prog[1].clear();
  _dist[0] = _dist[1] = -1;
  _player = 0;
  _par = shortestSolutionLen(_maze, profile && profile->unlocks.jump);
  if (_par <= 0) _par = 1;
  _phase = Phase::AUTHOR;
  _peeking = false;
}

void PuzzleRaceScreen::attachEditor() {
  EditorConfig cfg;
  if (_profile) {
    cfg.jump   = _profile->unlocks.jump;
    cfg.repeat = _profile->unlocks.repeat;
    cfg.call   = _profile->unlocks.func;
    cfg.sense  = _profile->unlocks.sense;
    cfg.avatar = _profile->avatar;
    cfg.customChar = &_profile->customChar;
  }
  cfg.func = _profile && _profile->unlocks.func;  // SAME editor: real F1/F2 function tabs
  cfg.library = false;  // ...but no library Save/Load mid-race (would be a pre-canned-bot cheat)
  cfg.neuro = false;  // a brain would need training; not in a timed race
  cfg.facing = _maze.startPose().facing;
  _editor.attach(&_prog[_player], cfg, _par, _profile);
}

void PuzzleRaceScreen::enter() {
  _deadline = 0;  // armed on first tick
  _peeking = false;
  attachEditor();
  drawAuthor();
}

void PuzzleRaceScreen::drawAuthor() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  char t[28]; snprintf(t, sizeof(t), "Player %d - program it!", _player + 1);
  label(g, 6, 4, t, _player == 0 ? C_GO : C_FUNC);
  _editor.draw();  // the same control pad + program list as the campaign
  drawButtons();
  // timer drawn each tick over the top bar
}

// The editor's program pane fills the full right side down to the screen bottom, so it
// repaints over this bottom bar when you tap the pad — redraw the buttons after editor taps.
void PuzzleRaceScreen::drawButtons() {
  auto& g = hal::display.gfx();
  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_DONE, "DONE >", C_GO, C_PANEL);
  button(g, R_PEEK, "see the maze", C_ACCENT, C_PANEL);
}

void PuzzleRaceScreen::drawBoard() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 4, "The maze - tap to go back", C_ACCENT);
  int cols = _maze.cols(), rows = _maze.rows();
  int tile = (SCREEN_W - 16) / cols; int th = (BAND_H - 8) / rows;
  if (th < tile) tile = th;
  int ox = (SCREEN_W - tile * cols) / 2, oy = BAND_Y + (BAND_H - tile * rows) / 2;
  for (int r = 0; r < rows; r++)
    for (int c = 0; c < cols; c++) {
      int x = ox + c * tile, y = oy + r * tile;
      Tile t = _maze.at(r, c);
      uint16_t col = ((r + c) & 1) ? C_FLOOR : C_FLOOR2;
      if (t == WALL) col = C_WALL; else if (t == PIT) col = C_BG;
      g.fillRect(x, y, tile - 1, tile - 1, col);
      if (_maze.isGoal(r, c)) assets::drawGoalToken(g, x + tile / 2, y + tile / 2, tile, 0);
    }
  Pose s = _maze.startPose();
  assets::drawCharacter(g, ox + s.col * tile + tile / 2, oy + s.row * tile + tile / 2,
                        tile, _profile ? _profile->avatar : 0, s.facing);
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
  _peeking = false;
  if (_player == 0) { _phase = Phase::HANDOFF; drawHandoff(); }
  else { _phase = Phase::RESULT; drawResult(); }
}

app::Signal PuzzleRaceScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  bool tap = _tap.tapped(tp, now, tx, ty);

  if (_phase == Phase::AUTHOR) {
    if (_deadline == 0) { _deadline = now + AUTHOR_MS; _shownSec = -1; }
    // countdown in the top bar (only repaint when the second changes, and not while peeking)
    int rem = (int)((_deadline - now) / 1000);
    if (rem < 0) rem = 0;
    if (!_peeking && rem != _shownSec) {
      _shownSec = rem;
      auto& g = hal::display.gfx();
      g.fillRect(250, 2, 66, 18, C_PANEL);
      char tm[12]; snprintf(tm, sizeof(tm), "0:%02d", rem);
      label(g, SCREEN_W - 6, 4, tm, rem <= 10 ? C_BAD : C_INK, textdatum_t::top_right);
    }
    if (now >= _deadline) { lockIn(now); return app::Signal::NONE; }

    if (tap) {
      if (_peeking) { _peeking = false; drawAuthor(); return app::Signal::NONE; }
      if (R_DONE.contains(tx, ty)) { lockIn(now); return app::Signal::NONE; }
      if (R_PEEK.contains(tx, ty)) { _peeking = true; drawBoard(); return app::Signal::NONE; }
      // everything else goes to the shared editor; RUN there = run it now (lock in + score)
      if (_editor.handleTap(tx, ty) == ProgramEditor::Action::RUN) { lockIn(now); }
      else drawButtons();  // the editor may have repainted its pane over our bottom bar
    }
    return app::Signal::NONE;
  }

  if (_phase == Phase::HANDOFF) {
    if (tap && R_READY.contains(tx, ty)) {
      _player = 1; _deadline = 0; _peeking = false; _phase = Phase::AUTHOR;
      attachEditor(); drawAuthor();
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
