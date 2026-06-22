#include "screens/TransferLessonScreen.h"
#include "hal/Audio.h"
#include "assets/Assets.h"
#include "game/MazeGen.h"
#include "game/Program.h"
#include "game/Interpreter.h"
#include "game/Distill.h"

using namespace ui;
using namespace gb;

namespace screens {

static const Rect R_NEXT = {6,   (int16_t)(BOTBAR_Y + 2), 170, 26};
static const Rect R_RST  = {182, (int16_t)(BOTBAR_Y + 2), 64, 26};
static const Rect R_BACK = {252, (int16_t)(BOTBAR_Y + 2), 62, 26};

static const char* STATUS[4] = {
  "this brain knows nothing yet.",
  "learned maze A! now try a NEW maze.",
  "new maze B: it already gets part way - it GENERALIZES!",
  "fine-tuned: kept A's skills + mastered B (no over-fit).",
};
static const char* BTN[4] = {"Learn maze A >", "New maze B! >", "Fine-tune on B >", "Reset"};

void TransferLessonScreen::begin() {
  MazeGen::generate(_maze, 4u, 5);            // maze A
  _brain.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 7);
  _phase = 0;
  trace();
}

void TransferLessonScreen::enter() { draw(); }

void TransferLessonScreen::trace() {
  Program prog; prog.brains.push_back(_brain);
  Node loop = Node::repeatUntil(AT_GOAL); loop.body.push_back(Node::neuro(0)); prog.main.push_back(loop);
  Interpreter it; it.load(&prog, &_maze, _maze.startPose(), 64);
  _pathLen = 0; _won = false;
  for (int s = 0; s < 64; s++) {
    Pose p = it.pose();
    if (_pathLen < 64) _path[_pathLen++] = (uint8_t)(p.row * _maze.cols() + p.col);
    if (it.finished()) { _won = (it.lastOutcome() == OUT_WIN); break; }
    it.step();
  }
}

void TransferLessonScreen::mazeGeom(int& tile, int& ox, int& oy) const {
  int cols = _maze.cols(), rows = _maze.rows();
  tile = (SCREEN_W - 20) / cols; int th = (BAND_H - 30) / rows;
  if (th < tile) tile = th; if (tile > 30) tile = 30;
  ox = (SCREEN_W - tile * cols) / 2; oy = BAND_Y + 22;
}

void TransferLessonScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Transfer learning", ui::rgb(120, 230, 245), textdatum_t::top_left, 2);
  label(g, 6, TOPBAR_H + 4, STATUS[_phase], _won ? C_GO : C_INK);

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
  for (int i = 0; i < _pathLen; i++) {
    int r = _path[i] / _maze.cols(), c = _path[i] % _maze.cols();
    g.fillCircle(ox + c * tile + tile / 2, oy + r * tile + tile / 2, tile / 6 + 1, _won ? C_GO : C_MOVE);
  }

  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_NEXT, BTN[_phase], C_GO, C_PANEL);
  button(g, R_RST, "Reset", C_ACCENT, C_PANEL);
  button(g, R_BACK, "Back", C_INK, C_PANEL);
}

app::Signal TransferLessonScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  if (R_NEXT.contains(tx, ty)) {
    if (_phase == 0) {                       // learn maze A
      distillSolver(_brain, _maze, false, 500); _phase = 1;
    } else if (_phase == 1) {                // swap to a NEW maze, same brain (transfer)
      MazeGen::generate(_maze, 19u, 5); _phase = 2;
    } else if (_phase == 2) {                // fine-tune on B
      distillSolver(_brain, _maze, false, 500); _phase = 3;
    } else {                                 // reset
      begin(); draw(); return app::Signal::NONE;
    }
    trace(); hal::audio.blip(); draw();
  } else if (R_RST.contains(tx, ty)) {
    begin(); draw();
  } else if (R_BACK.contains(tx, ty)) {
    return app::Signal::BACK;
  }
  return app::Signal::NONE;
}

}  // namespace screens
