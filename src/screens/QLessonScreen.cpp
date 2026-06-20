#include "screens/QLessonScreen.h"
#include "hal/Audio.h"
#include "game/MazeGen.h"

using namespace ui;
using namespace gb;

namespace screens {

static const Rect R_LEARN = {6,   (int16_t)(BOTBAR_Y + 2), 130, 26};
static const Rect R_RESET = {142, (int16_t)(BOTBAR_Y + 2), 80, 26};
static const Rect R_BACK  = {244, (int16_t)(BOTBAR_Y + 2), 70, 26};

void QLessonScreen::begin() {
  MazeGen::generate(_maze, 5u, 5);   // a small fixed maze (few cells -> fast, clear Q)
  _q.init(&_maze, 7);
  _solved = false;
}

void QLessonScreen::enter() { draw(); }

void QLessonScreen::mazeGeom(int& tile, int& ox, int& oy) const {
  int cols = _maze.cols(), rows = _maze.rows();
  tile = (SCREEN_W - 20) / cols;
  int th = (BAND_H - 8) / rows;
  if (th < tile) tile = th;
  ox = (SCREEN_W - tile * cols) / 2;
  oy = BAND_Y + 6 + (BAND_H - 8 - tile * rows) / 2;
}

// value -> colour: positive = green glow (stronger = brighter), <=0 = dim floor
static uint16_t valColor(float v) {
  if (v <= 0.01f) return C_FLOOR;
  float t = v; if (t > 1.0f) t = 1.0f;
  return ui::rgb(20, (int)(60 + 180 * t), (int)(50 + 60 * t));
}

void QLessonScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Q-learning: reward", C_ACCENT, textdatum_t::top_left, 2);
  char hd[28]; snprintf(hd, sizeof(hd), "try %d", _q.episodes);
  label(g, SCREEN_W - 6, 6, hd, C_DIM, textdatum_t::top_right);

  int tile, ox, oy; mazeGeom(tile, ox, oy);
  for (int r = 0; r < _maze.rows(); r++) {
    for (int c = 0; c < _maze.cols(); c++) {
      int x = ox + c * tile, y = oy + r * tile;
      Tile t = _maze.at(r, c);
      uint16_t col;
      if (t == WALL) col = C_WALL;
      else if (t == PIT) col = C_BG;
      else col = valColor(_q.maxQ(r, c));         // value heatmap
      g.fillRect(x, y, tile - 1, tile - 1, col);
      if (_maze.isGoal(r, c)) {
        g.fillCircle(x + tile / 2, y + tile / 2, tile / 3, C_ACCENT);
      } else if (t != WALL && t != PIT && _q.maxQ(r, c) > 0.01f) {
        // policy arrow: best move learned for this cell (0=N,1=E,2=S,3=W)
        int a = _q.bestAction(r, c), cx = x + tile / 2, cy = y + tile / 2, s = tile / 4;
        uint16_t ac = C_INK;
        if (a == 0) g.fillTriangle(cx, cy - s, cx - s, cy + s, cx + s, cy + s, ac);
        else if (a == 2) g.fillTriangle(cx, cy + s, cx - s, cy - s, cx + s, cy - s, ac);
        else if (a == 1) g.fillTriangle(cx + s, cy, cx - s, cy - s, cx - s, cy + s, ac);
        else g.fillTriangle(cx - s, cy, cx + s, cy - s, cx + s, cy + s, ac);
      }
      Pose st = _maze.startPose();
      if (r == st.row && c == st.col) g.drawRect(x, y, tile - 1, tile - 1, C_GO);  // start
    }
  }

  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_LEARN, "Learn x50", C_GO, C_PANEL);
  button(g, R_RESET, "Reset", C_ACCENT, C_PANEL);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
  if (_solved) label(g, SCREEN_W / 2, BOTBAR_Y - 10, "it learned the way!", C_GO,
                     textdatum_t::bottom_center);
}

app::Signal QLessonScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  if (R_LEARN.contains(tx, ty)) {
    for (int e = 0; e < 50; e++) _q.runEpisode();
    _solved = _q.greedySolves();
    hal::audio.blip(); draw();
  } else if (R_RESET.contains(tx, ty)) {
    _q.init(&_maze, 7); _solved = false; hal::audio.blip(); draw();
  } else if (R_BACK.contains(tx, ty)) {
    return app::Signal::BACK;
  }
  return app::Signal::NONE;
}

}  // namespace screens
