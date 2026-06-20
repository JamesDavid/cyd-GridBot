#include "screens/NeuroTrainScreen.h"
#include "hal/Audio.h"
#include "game/Interpreter.h"

using namespace ui;
using namespace gb;

namespace screens {

static const Rect R_TRAIN = {6,   (int16_t)(BOTBAR_Y + 2), 120, 26};
static const Rect R_USE   = {132, (int16_t)(BOTBAR_Y + 2), 110, 26};
static const Rect R_BACK  = {248, (int16_t)(BOTBAR_Y + 2), 66, 26};

void NeuroTrainScreen::begin(Program* prog, int brainIdx, Maze* maze) {
  _prog = prog; _idx = brainIdx; _maze = maze;
  _evo.init(SENSOR_COUNT_FOR_BRAIN, 8, 5, 17);
  // seed the population with the block's current brain so re-training improves it
  if (prog && brainIdx < (int)prog->brains.size()) _evo.pop[0] = prog->brains[brainIdx];
  _evo.evaluate(*_maze, nullptr, 110);
  _saved = false;
  tracePath();
}

void NeuroTrainScreen::enter() { draw(); }

void NeuroTrainScreen::tracePath() {
  Program prog; prog.brains.push_back(_evo.best());
  Node loop = Node::repeatUntil(AT_GOAL);
  loop.body.push_back(Node::neuro(0));
  prog.main.push_back(loop);
  Interpreter it; it.load(&prog, _maze, _maze->startPose(), 64);
  _pathLen = 0; _won = false;
  for (int s = 0; s < 64; s++) {
    Pose p = it.pose();
    if (_pathLen < 64) _path[_pathLen++] = (uint8_t)(p.row * _maze->cols() + p.col);
    if (it.finished()) { _won = (it.lastOutcome() == OUT_WIN); break; }
    it.step();
  }
}

void NeuroTrainScreen::mazeGeom(int& tile, int& ox, int& oy) const {
  int cols = _maze->cols(), rows = _maze->rows();
  tile = (SCREEN_W - 20) / cols;
  int th = (BAND_H - 8) / rows;
  if (th < tile) tile = th;
  ox = (SCREEN_W - tile * cols) / 2;
  oy = BAND_Y + 6 + (BAND_H - 8 - tile * rows) / 2;
}

void NeuroTrainScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Train the brain", ui::rgb(120, 230, 245), textdatum_t::top_left, 2);
  char hd[28]; snprintf(hd, sizeof(hd), "gen %d  %s", _evo.gen, _won ? "solves!" : "...");
  label(g, SCREEN_W - 6, 6, hd, _won ? C_GO : C_DIM, textdatum_t::top_right);

  int tile, ox, oy; mazeGeom(tile, ox, oy);
  for (int r = 0; r < _maze->rows(); r++)
    for (int c = 0; c < _maze->cols(); c++) {
      int x = ox + c * tile, y = oy + r * tile;
      Tile t = _maze->at(r, c);
      uint16_t col = ((r + c) & 1) ? C_FLOOR : C_FLOOR2;
      if (t == WALL) col = C_WALL; else if (t == PIT) col = C_BG;
      g.fillRect(x, y, tile - 1, tile - 1, col);
      if (_maze->isGoal(r, c)) g.fillCircle(x + tile / 2, y + tile / 2, tile / 3, C_ACCENT);
    }
  for (int i = 0; i < _pathLen; i++) {
    int r = _path[i] / _maze->cols(), c = _path[i] % _maze->cols();
    g.fillCircle(ox + c * tile + tile / 2, oy + r * tile + tile / 2, tile / 6 + 1,
                 _won ? C_GO : C_MOVE);
  }

  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_TRAIN, "Train x5", C_GO, C_PANEL);
  button(g, R_USE, _saved ? "saved!" : "Use it", _saved ? C_DIM : ui::rgb(120, 230, 245), C_PANEL);
  button(g, R_BACK, "Back", C_INK, C_PANEL);
}

app::Signal NeuroTrainScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  if (R_TRAIN.contains(tx, ty)) {
    for (int gg = 0; gg < 5; gg++) { _evo.breed(); _evo.evaluate(*_maze, nullptr, 110); }
    _saved = false;
    tracePath(); hal::audio.blip(); draw();
  } else if (R_USE.contains(tx, ty)) {
    if (_prog && _idx < (int)_prog->brains.size()) _prog->brains[_idx] = _evo.best();
    _saved = true; hal::audio.badge(); draw();
  } else if (R_BACK.contains(tx, ty)) {
    return app::Signal::BACK;
  }
  return app::Signal::NONE;
}

}  // namespace screens
