#include "screens/TuneLessonScreen.h"
#include "hal/Audio.h"
#include "assets/Assets.h"
#include "game/MazeGen.h"

using namespace ui;
using namespace gb;

namespace screens {

// knob steppers (in the band, above the maze): explore (eps) and step (alpha)
static const Rect R_EPS_DN = {86,  26, 26, 20};
static const Rect R_EPS_UP = {150, 26, 26, 20};
static const Rect R_ALP_DN = {86,  50, 26, 20};
static const Rect R_ALP_UP = {150, 50, 26, 20};
// bottom bar
static const Rect R_LEARN = {6,   (int16_t)(BOTBAR_Y + 2), 120, 26};
static const Rect R_RESET = {132, (int16_t)(BOTBAR_Y + 2), 80, 26};
static const Rect R_BACK  = {244, (int16_t)(BOTBAR_Y + 2), 70, 26};

void TuneLessonScreen::begin() {
  MazeGen::generate(_maze, 5u, 5);   // the same small reward maze as the Q-learning lesson
  Pose st = _maze.startPose();
  int floors[MAZE_MAX_CELLS], nf = 0, cols = _maze.cols();
  for (int r = 0; r < _maze.rows(); r++)
    for (int c = 0; c < cols; c++)
      if (_maze.at(r, c) == FLOOR && !(r == st.row && c == st.col) && !_maze.isGoal(r, c))
        if (nf < MAZE_MAX_CELLS) floors[nf++] = r * cols + c;
  if (nf >= 1) _maze.set(floors[nf / 2] / cols, floors[nf / 2] % cols, STAR);  // a gem to reward
  _eps = 0.0f; _alpha = 0.5f;
  reset();
}

void TuneLessonScreen::reset() {
  _q.init(&_maze, 7);
  _q.epsilon = _eps; _q.alpha = _alpha;   // apply the current knobs
  _solved = false;
}

void TuneLessonScreen::enter() { draw(); }

void TuneLessonScreen::mazeGeom(int& tile, int& ox, int& oy) const {
  int cols = _maze.cols(), rows = _maze.rows();
  tile = (SCREEN_W - 20) / cols;
  int top = BAND_Y + 50;                  // leave room for the two knob rows
  int th = (SCREEN_H - BOTBAR_H - top - 6) / rows;
  if (th < tile) tile = th;
  ox = (SCREEN_W - tile * cols) / 2;
  oy = top + (SCREEN_H - BOTBAR_H - top - tile * rows) / 2;
}

static uint16_t valColor(float v) {
  if (v <= 0.01f) return C_FLOOR;
  float t = v; if (t > 1.0f) t = 1.0f;
  return ui::rgb(20, (int)(60 + 180 * t), (int)(50 + 60 * t));
}

// one knob row: label, [-] value [+]
static void knob(LGFX& g, int y, const char* name, float val, const Rect& dn, const Rect& up, uint16_t col) {
  label(g, 6, y + 4, name, col);
  button(g, dn, "-", C_INK, C_PANEL);
  char v[8]; snprintf(v, sizeof(v), "%.1f", val);
  label(g, 131, y + 4, v, C_ACCENT, textdatum_t::top_center);
  button(g, up, "+", C_INK, C_PANEL);
}

void TuneLessonScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Tuning the grid", C_ACCENT, textdatum_t::top_left, 2);
  char hd[20]; snprintf(hd, sizeof(hd), "tries %d", _q.episodes);
  label(g, SCREEN_W - 6, 6, hd, C_DIM, textdatum_t::top_right);

  knob(g, 24, "explore", _eps, R_EPS_DN, R_EPS_UP, C_MOVE);
  knob(g, 48, "step",    _alpha, R_ALP_DN, R_ALP_UP, C_LOOP);
  label(g, 190, 28, _eps < 0.05f ? "0 = never tries new moves!" : "", C_BAD);

  int tile, ox, oy; mazeGeom(tile, ox, oy);
  for (int r = 0; r < _maze.rows(); r++)
    for (int c = 0; c < _maze.cols(); c++) {
      int x = ox + c * tile, y = oy + r * tile;
      Tile t = _maze.at(r, c);
      uint16_t col = (t == WALL) ? C_WALL : (t == PIT) ? C_BG : valColor(_q.maxQ(r, c));
      g.fillRect(x, y, tile - 1, tile - 1, col);
      int cx = x + tile / 2, cy = y + tile / 2;
      if (_maze.isGoal(r, c)) assets::drawGoalToken(g, cx, cy, tile, 0);
      else if (t == STAR) g.fillCircle(cx, cy, tile / 5, ui::rgb(120, 230, 245));
      else if (t != WALL && t != PIT && _q.maxQ(r, c) > 0.01f) {
        int a = _q.bestAction(r, c), s = tile / 4;
        if (a == 0) g.fillTriangle(cx, cy - s, cx - s, cy + s, cx + s, cy + s, C_INK);
        else if (a == 2) g.fillTriangle(cx, cy + s, cx - s, cy - s, cx + s, cy - s, C_INK);
        else if (a == 1) g.fillTriangle(cx + s, cy, cx - s, cy - s, cx - s, cy + s, C_INK);
        else g.fillTriangle(cx - s, cy, cx + s, cy - s, cx + s, cy + s, C_INK);
      }
      Pose st = _maze.startPose();
      if (r == st.row && c == st.col) g.drawRect(x, y, tile - 1, tile - 1, C_GO);
    }

  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_LEARN, "Learn x20", C_GO, C_PANEL);
  button(g, R_RESET, "Reset", C_ACCENT, C_PANEL);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
  const char* msg;
  if (_solved && _q.episodes <= _par * 20) msg = "Tuned it! solved fast - great knobs";
  else if (_solved) msg = "solved! now try to do it in fewer tries";
  else msg = "turn EXPLORE up, then Learn, until it solves";
  label(g, SCREEN_W / 2, BOTBAR_Y - 10, msg, _solved ? C_GO : C_DIM, textdatum_t::bottom_center);
}

app::Signal TuneLessonScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  if (R_LEARN.contains(tx, ty)) {
    for (int e = 0; e < 20; e++) _q.runEpisode();
    _solved = _q.greedySolves();
    hal::audio.blip(); draw();
  } else if (R_EPS_DN.contains(tx, ty)) { if (_eps > 0.001f) _eps -= 0.1f; if (_eps < 0) _eps = 0; reset(); hal::audio.blip(); draw(); }
  else if (R_EPS_UP.contains(tx, ty)) { if (_eps < 0.5f) _eps += 0.1f; reset(); hal::audio.blip(); draw(); }
  else if (R_ALP_DN.contains(tx, ty)) { if (_alpha > 0.15f) _alpha -= 0.2f; reset(); hal::audio.blip(); draw(); }
  else if (R_ALP_UP.contains(tx, ty)) { if (_alpha < 0.85f) _alpha += 0.2f; reset(); hal::audio.blip(); draw(); }
  else if (R_RESET.contains(tx, ty)) { reset(); hal::audio.blip(); draw(); }
  else if (R_BACK.contains(tx, ty)) return app::Signal::BACK;
  return app::Signal::NONE;
}

}  // namespace screens
