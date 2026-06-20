#include "screens/EvoLessonScreen.h"
#include "hal/Audio.h"
#include "assets/Assets.h"
#include "game/MazeGen.h"
#include "game/Program.h"
#include "game/Interpreter.h"

using namespace ui;
using namespace gb;

namespace screens {

static const Rect R_EVOLVE = {6,   (int16_t)(BOTBAR_Y + 2), 140, 26};
static const Rect R_RESET  = {152, (int16_t)(BOTBAR_Y + 2), 70, 26};
static const Rect R_BACK   = {244, (int16_t)(BOTBAR_Y + 2), 70, 26};

void EvoLessonScreen::begin() {
  MazeGen::generate(_maze, 9u, 6);
  _evo.init(SENSOR_COUNT_FOR_BRAIN, 8, 5, 3);
  _evo.evaluate(_maze, nullptr, 100);
  _histN = 0;
  _hist[_histN++] = _evo.bestFit();
  tracePath();
}

void EvoLessonScreen::enter() { draw(); }

// Run the current best brain and record the cells it visits (its trajectory).
void EvoLessonScreen::tracePath() {
  Program prog; prog.brains.push_back(_evo.best());
  Node loop = Node::repeatUntil(AT_GOAL);
  loop.body.push_back(Node::neuro(0));
  prog.main.push_back(loop);
  Interpreter it; it.load(&prog, &_maze, _maze.startPose(), 64);
  _pathLen = 0; _won = false;
  for (int s = 0; s < 64; s++) {
    Pose p = it.pose();
    if (_pathLen < 64) _path[_pathLen++] = (uint8_t)(p.row * _maze.cols() + p.col);
    if (it.finished()) { _won = (it.lastOutcome() == OUT_WIN); break; }
    it.step();
  }
}

void EvoLessonScreen::mazeGeom(int& tile, int& ox, int& oy) const {
  int cols = _maze.cols(), rows = _maze.rows();
  int band = BAND_H - 34;  // leave room for the fitness chart at the bottom of the band
  tile = (SCREEN_W - 20) / cols;
  int th = band / rows;
  if (th < tile) tile = th;
  ox = (SCREEN_W - tile * cols) / 2;
  oy = BAND_Y + 4;
}

void EvoLessonScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Evolution", C_ACCENT, textdatum_t::top_left, 2);
  char hd[28]; snprintf(hd, sizeof(hd), "gen %d  fit %.0f", _evo.gen, _evo.bestFit());
  label(g, SCREEN_W - 6, 6, hd, C_DIM, textdatum_t::top_right);

  int tile, ox, oy; mazeGeom(tile, ox, oy);
  for (int r = 0; r < _maze.rows(); r++)
    for (int c = 0; c < _maze.cols(); c++) {
      int x = ox + c * tile, y = oy + r * tile;
      Tile t = _maze.at(r, c);
      uint16_t col = ((r + c) & 1) ? C_FLOOR : C_FLOOR2;
      if (t == WALL) col = C_WALL; else if (t == PIT) col = C_BG;
      g.fillRect(x, y, tile - 1, tile - 1, col);
      if (_maze.isGoal(r, c)) assets::drawGoalToken(g, x + tile / 2, y + tile / 2, tile, 0);
    }
  // the best brain's path
  for (int i = 0; i < _pathLen; i++) {
    int r = _path[i] / _maze.cols(), c = _path[i] % _maze.cols();
    int cx = ox + c * tile + tile / 2, cy = oy + r * tile + tile / 2;
    g.fillCircle(cx, cy, tile / 6 + 1, _won ? C_GO : C_MOVE);
  }

  // fitness chart at the bottom of the band
  int chTop = oy + tile * _maze.rows() + 4, chH = 24, chW = SCREEN_W - 16;
  g.drawRect(8, chTop, chW, chH, C_PANEL_HI);
  float lo = 1e9f, hi = -1e9f;
  for (int i = 0; i < _histN; i++) { if (_hist[i] < lo) lo = _hist[i]; if (_hist[i] > hi) hi = _hist[i]; }
  if (hi <= lo) hi = lo + 1;
  for (int i = 1; i < _histN; i++) {
    int x0 = 8 + (i - 1) * chW / (HIST - 1), x1 = 8 + i * chW / (HIST - 1);
    int y0 = chTop + chH - 2 - (int)((_hist[i - 1] - lo) / (hi - lo) * (chH - 4));
    int y1 = chTop + chH - 2 - (int)((_hist[i] - lo) / (hi - lo) * (chH - 4));
    g.drawLine(x0, y0, x1, y1, C_GO);
  }

  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_EVOLVE, "Evolve x3", C_GO, C_PANEL);
  button(g, R_RESET, "Reset", C_ACCENT, C_PANEL);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
}

app::Signal EvoLessonScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  if (R_EVOLVE.contains(tx, ty)) {
    for (int g = 0; g < 3; g++) {
      _evo.breed();
      _evo.evaluate(_maze, nullptr, 100);
      if (_histN < HIST) _hist[_histN++] = _evo.bestFit();
      else { for (int i = 1; i < HIST; i++) _hist[i - 1] = _hist[i]; _hist[HIST - 1] = _evo.bestFit(); }
    }
    tracePath();
    hal::audio.blip(); draw();
  } else if (R_RESET.contains(tx, ty)) {
    begin(); draw();
  } else if (R_BACK.contains(tx, ty)) {
    return app::Signal::BACK;
  }
  return app::Signal::NONE;
}

}  // namespace screens
