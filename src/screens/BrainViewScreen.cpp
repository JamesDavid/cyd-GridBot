#include "screens/BrainViewScreen.h"
#include "hal/Audio.h"
#include "assets/Assets.h"
#include "game/MazeGen.h"
#include "game/Sensors.h"
#include "game/Distill.h"

using namespace ui;
using namespace gb;

namespace screens {

static const Rect R_STEP = {6,   (int16_t)(BOTBAR_Y + 2), 84, 26};
static const Rect R_RUN  = {94,  (int16_t)(BOTBAR_Y + 2), 78, 26};
static const Rect R_RST  = {176, (int16_t)(BOTBAR_Y + 2), 66, 26};
static const Rect R_BACK = {250, (int16_t)(BOTBAR_Y + 2), 64, 26};

static const char* INLBL[SENSOR_COUNT_FOR_BRAIN] =
  {"wallF","wallL","wallR","pitF","goalF","goalR","goalD","foeF","foeR","foeD"};
static const char* OUTLBL[5] = {"fwd","turnL","turnR","jump","zap"};

// node colour by activation (~[-1,1]): green = positive/firing, red = negative, dim = ~0
static uint16_t actCol(float a) {
  if (a > 0.05f) { int i = (int)(a * 200); if (i > 200) i = 200; return ui::rgb(20, 55 + i, 50); }
  if (a < -0.05f) { int i = (int)(-a * 180); if (i > 180) i = 180; return ui::rgb(60 + i, 28, 28); }
  return C_PANEL_HI;
}

void BrainViewScreen::begin() {
  MazeGen::generate(_maze, 9u, 6);
  _brain.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 7);
  distillSolver(_brain, _maze, true, 500);     // a brain that actually navigates
  _prog.clear();
  uint8_t idx = _prog.addBrain(1); _prog.brains[idx] = _brain;
  Node loop = Node::repeatUntil(AT_GOAL); loop.body.push_back(Node::neuro(idx)); _prog.main.push_back(loop);
  _it.load(&_prog, &_maze, _maze.startPose(), 200);
  _running = false;
  computeActivations();
}

void BrainViewScreen::enter() { draw(); }

void BrainViewScreen::computeActivations() {
  senseEgo(_maze, _it.pose(), nullptr, _in);
  _brain.forward(_in, _out, _hid);
  _action = 0;
  for (int k = 1; k < _brain.nOut; k++) if (_out[k] > _out[_action]) _action = k;
}

void BrainViewScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Brain Cam", ui::rgb(120, 230, 245), textdatum_t::top_left, 2);
  label(g, 150, 4, "sense -> think -> act", C_DIM, textdatum_t::top_left);

  // small maze (top-left) so you see WHAT it senses
  int mtile = 52 / _maze.cols(); if (44 / _maze.rows() < mtile) mtile = 44 / _maze.rows();
  int mox = 6, moy = BAND_Y + 4;
  for (int r = 0; r < _maze.rows(); r++)
    for (int c = 0; c < _maze.cols(); c++) {
      int x = mox + c * mtile, y = moy + r * mtile;
      Tile t = _maze.at(r, c);
      uint16_t col = ((r + c) & 1) ? C_FLOOR : C_FLOOR2;
      if (t == WALL) col = C_WALL; else if (t == PIT) col = C_BG;
      g.fillRect(x, y, mtile - 1, mtile - 1, col);
      if (_maze.isGoal(r, c)) g.fillCircle(x + mtile / 2, y + mtile / 2, mtile / 3, C_ACCENT);
    }
  Pose p = _it.pose();
  assets::drawCharacter(g, mox + p.col * mtile + mtile / 2, moy + p.row * mtile + mtile / 2, mtile, 0, p.facing);

  // ---- the network: inputs (left) -> hidden (mid) -> outputs (right) ----
  int IX = 96, HX = 168, OX = 238;
  // node columns centred vertically in the band (was scrunched at the top)
  int iy0 = BAND_Y + 22, idy = 16;
  int hy0 = BAND_Y + 26, hdy = 20;
  int oy0 = BAND_Y + 32, ody = 28;
  // dim wiring
  for (int i = 0; i < SENSOR_COUNT_FOR_BRAIN; i++)
    for (int j = 0; j < _brain.nHid; j++) g.drawLine(IX, iy0 + i * idy, HX, hy0 + j * hdy, C_LOCK);
  for (int j = 0; j < _brain.nHid; j++)
    for (int k = 0; k < _brain.nOut; k++) g.drawLine(HX, hy0 + j * hdy, OX, oy0 + k * ody, C_LOCK);
  // input nodes + labels
  for (int i = 0; i < SENSOR_COUNT_FOR_BRAIN; i++) {
    int y = iy0 + i * idy;
    label(g, 62, y - 3, INLBL[i], C_DIM);
    g.fillCircle(IX, y, 4, actCol(_in[i]));
  }
  // hidden nodes
  for (int j = 0; j < _brain.nHid; j++) g.fillCircle(HX, hy0 + j * hdy, 6, actCol(_hid[j]));
  // output nodes + labels (winner ringed)
  for (int k = 0; k < _brain.nOut; k++) {
    int y = oy0 + k * ody;
    g.fillCircle(OX, y, 8, actCol(_out[k] * 2 - 1));
    if (k == _action) g.drawCircle(OX, y, 10, ui::rgb(120, 230, 245));
    label(g, OX + 14, y - 3, OUTLBL[k], k == _action ? ui::rgb(120, 230, 245) : C_DIM);
  }
  // headers + verdict
  label(g, 70, BAND_Y - 0, "SEES", C_MOVE);
  label(g, HX - 14, BAND_Y, "THINKS", C_SENSE);
  label(g, OX - 6, BAND_Y, "DOES", C_GO);
  char v[24]; snprintf(v, sizeof(v), "decides: %s", OUTLBL[_action]);
  label(g, SCREEN_W / 2, BOTBAR_Y - 16, v, ui::rgb(120, 230, 245), textdatum_t::top_center);

  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_STEP, "Step", C_GO, C_PANEL);
  button(g, R_RUN, _running ? "..." : "Run", C_GO, C_PANEL);
  button(g, R_RST, "Reset", C_ACCENT, C_PANEL);
  button(g, R_BACK, "Back", C_INK, C_PANEL);
}

app::Signal BrainViewScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  if (_running && !_it.finished() && now - _last >= 600) {
    _last = now; _it.step(); computeActivations();
    if (_it.finished()) _running = false;
    draw();
  }
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  if (R_STEP.contains(tx, ty)) {
    if (!_it.finished()) { _it.step(); computeActivations(); }
    else { _it.load(&_prog, &_maze, _maze.startPose(), 200); computeActivations(); }
    hal::audio.blip(); draw();
  } else if (R_RUN.contains(tx, ty)) {
    if (_it.finished()) { _it.load(&_prog, &_maze, _maze.startPose(), 200); computeActivations(); }
    _running = !_running; _last = now; draw();
  } else if (R_RST.contains(tx, ty)) {
    begin(); draw();
  } else if (R_BACK.contains(tx, ty)) {
    return app::Signal::BACK;
  }
  return app::Signal::NONE;
}

}  // namespace screens
