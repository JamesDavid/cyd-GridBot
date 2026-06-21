#include "screens/BrainViewScreen.h"
#include "hal/Audio.h"
#include "assets/Assets.h"
#include "game/MazeGen.h"
#include "game/Sensors.h"
#include "game/Reactive.h"
#include "game/Distill.h"

using namespace ui;
using namespace gb;

namespace screens {

static const Rect R_TEACH = {6,   (int16_t)(BOTBAR_Y + 2), 60, 26};
static const Rect R_RUN   = {70,  (int16_t)(BOTBAR_Y + 2), 48, 26};
static const Rect R_MAP   = {122, (int16_t)(BOTBAR_Y + 2), 52, 26};
static const Rect R_NEW   = {178, (int16_t)(BOTBAR_Y + 2), 48, 26};  // scramble to a fresh brain
static const Rect R_BACK  = {230, (int16_t)(BOTBAR_Y + 2), 84, 26};
static const Rect R_TYPE  = {196, 1, 56, 20};   // ff/rnn toggle, top bar

static const char* INLBL[SENSOR_COUNT_FOR_BRAIN] =
  {"wallF","wallL","wallR","pitF","goalF","goalR","goalD","foeF","foeR","foeD"};
static const char* OUTLBL[5] = {"fwd","turnL","turnR","jump","zap"};

// network geometry (absolute screen coords)
static const int IX = 92, HX = 178, OX = 252;
static int iy(int i) { return 30 + i * 15; }
static int hy(int j) { return 34 + j * 18; }
static int oy(int k) { return 40 + k * 26; }

// activation -> node colour (green fires, red negative, dim ~0)
static uint16_t actCol(float a) {
  if (a > 0.05f) { int i = (int)(a * 200); if (i > 200) i = 200; return ui::rgb(20, 55 + i, 50); }
  if (a < -0.05f) { int i = (int)(-a * 180); if (i > 180) i = 180; return ui::rgb(60 + i, 28, 28); }
  return C_PANEL_HI;
}
// weight -> connection colour (the thing that visibly changes as backprop runs)
static uint16_t wtCol(float w) {
  float m = w < 0 ? -w : w; if (m > 1.5f) m = 1.5f; int s = (int)(m * 150);
  if (w > 0.03f) return ui::rgb(10, 40 + s, 30);
  if (w < -0.03f) return ui::rgb(40 + s, 18, 18);
  return ui::rgb(30, 30, 38);
}
// recurrent (memory) weight -> a cyan/purple scale so it reads apart from the green/red forward web
static uint16_t recCol(float w) {
  float m = w < 0 ? -w : w; if (m > 1.5f) m = 1.5f; int s = (int)(m * 130);
  if (w > 0.03f) return ui::rgb(20, 70 + s / 2, 110 + s);   // positive: cyan
  if (w < -0.03f) return ui::rgb(70 + s, 25, 110 + s);      // negative: purple
  return ui::rgb(24, 28, 44);
}

// ---- accessors (branch on the active brain type) --------------------------
float BrainViewScreen::wIn(int i, int j) const { return _useRnn ? rnn().wih[j][i] : ff().w1[j][i]; }
float BrainViewScreen::wOut(int j, int k) const { return _useRnn ? rnn().who[k][j] : ff().w2[k][j]; }
float BrainViewScreen::wRec(int a, int b) const { return _useRnn ? rnn().whh[b][a] : 0.0f; }
float BrainViewScreen::bHid(int j) const { return _useRnn ? rnn().bh[j] : ff().b1[j]; }
float BrainViewScreen::bOut(int k) const { return _useRnn ? rnn().bo[k] : ff().b2[k]; }
int   BrainViewScreen::nHid() const { return _useRnn ? rnn().nHid : ff().nHid; }

void BrainViewScreen::loadMaze() {
  MazeGen::generate(_maze, 4242u, _level);
  _T = captureSolverShared(_maze);
}

void BrainViewScreen::resetBrains() {
  _prog.clear();
  _prog.addBrain(7);              // creates brains[0] (Net) + rbrains[0] (RNet), both untrained
  rnn().lr = 0.05f;
  _epoch = 0; _loss = 1.0f;
}

void BrainViewScreen::begin() {
  _level = 6; _useRnn = false; _sel = -1; _mode = M_IDLE;
  loadMaze();
  resetBrains();
  resetRun();
}

void BrainViewScreen::enter() { draw(); }

void BrainViewScreen::infer() {
  senseEgo(_maze, _pose, nullptr, _in);
  if (_useRnn) { rnn().step(_in, _out); for (int j = 0; j < rnn().nHid; j++) _hid[j] = rnn().h[j]; }
  else { ff().forward(_in, _out, _hid); }
  _action = 0;
  for (int k = 1; k < 5; k++) if (_out[k] > _out[_action]) _action = k;
}

void BrainViewScreen::resetRun() {
  _pose = _maze.startPose(); _done = false; _won = false;
  if (_useRnn) rnn().resetState();
  infer();
}

void BrainViewScreen::stepRun() {
  if (_done) return;
  Cmd c = (_action == 1) ? CMD_TURN_L : (_action == 2) ? CMD_TURN_R
        : (_action == 3) ? CMD_JUMP : (_action == 4) ? CMD_FIRE : CMD_FWD;
  Outcome o = reactiveApply(_maze, _pose, c);
  if (o == OUT_WIN) { _won = true; _done = true; }
  else if (o == OUT_BONK || o == OUT_FELL) { _done = true; }
  if (_maze.isGoal(_pose.row, _pose.col)) { _won = true; _done = true; }
  if (!_done) infer();
}

void BrainViewScreen::trainChunk() {
  if (_T <= 0) { _mode = M_IDLE; return; }
  if (_useRnn) { _loss = rnnTrainShared(rnn(), 3); _epoch += 3; }
  else         { _loss = ffTrainShared(ff(), 6);   _epoch += 6; }
  if (_epoch >= (_useRnn ? 240 : 150)) { _mode = M_IDLE; resetRun(); }  // trained -> ready to run
}

static int iabs(int v) { return v < 0 ? -v : v; }
bool BrainViewScreen::nodeAt(int x, int y, int& layer, int& idx) const {
  for (int j = 0; j < nHid(); j++) if (iabs(x - HX) < 10 && iabs(y - hy(j)) < 9) { layer = 1; idx = j; return true; }
  for (int k = 0; k < 5; k++) if (iabs(x - OX) < 12 && iabs(y - oy(k)) < 12) { layer = 2; idx = k; return true; }
  return false;
}

void BrainViewScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Brain Cam", ui::rgb(120, 230, 245), textdatum_t::top_left, 2);
  button(g, R_TYPE, _useRnn ? "rnn >" : "plain >", _useRnn ? C_ACCENT : C_INK, C_PANEL);

  // small maze (top-left) so you see WHAT it senses
  int mtile = 46 / _maze.cols(); if (40 / _maze.rows() < mtile) mtile = 40 / _maze.rows();
  if (mtile < 4) mtile = 4;
  int mox = 4, moy = BAND_Y + 4;
  for (int r = 0; r < _maze.rows(); r++)
    for (int c = 0; c < _maze.cols(); c++) {
      int x = mox + c * mtile, y = moy + r * mtile;
      Tile t = _maze.at(r, c);
      uint16_t col = ((r + c) & 1) ? C_FLOOR : C_FLOOR2;
      if (t == WALL) col = C_WALL; else if (t == PIT) col = C_BG;
      g.fillRect(x, y, mtile - 1, mtile - 1, col);
      if (_maze.isGoal(r, c)) g.fillCircle(x + mtile / 2, y + mtile / 2, mtile / 3, C_ACCENT);
    }
  assets::drawCharacter(g, mox + _pose.col * mtile + mtile / 2, moy + _pose.row * mtile + mtile / 2,
                        mtile, 0, _pose.facing);

  int selLayer = _sel < 0 ? -1 : _sel / 100, selIdx = _sel < 0 ? -1 : _sel % 100;

  // ---- connections coloured by WEIGHT (recolour live as backprop runs) ----
  for (int i = 0; i < SENSOR_COUNT_FOR_BRAIN; i++)
    for (int j = 0; j < nHid(); j++) {
      bool hot = (selLayer == 1 && selIdx == j);
      g.drawLine(IX, iy(i), HX, hy(j), hot ? wtCol(wIn(i, j) * 2) : wtCol(wIn(i, j)));
    }
  for (int j = 0; j < nHid(); j++)
    for (int k = 0; k < 5; k++) {
      bool hot = (selLayer == 2 && selIdx == k);
      g.drawLine(HX, hy(j), OX, oy(k), hot ? wtCol(wOut(j, k) * 2) : wtCol(wOut(j, k)));
    }
  if (_useRnn) {
    // recurrent feedback = the FULL hidden->hidden matrix. Every hidden neuron feeds back to
    // every hidden neuron (cross arcs bulging right) AND to itself (a small self-loop).
    int n = nHid();
    for (int a = 0; a < n; a++)
      for (int b = 0; b < n; b++) {
        if (a == b) continue;
        bool hot = (selLayer == 1 && (selIdx == a || selIdx == b));
        uint16_t col = recCol(wRec(a, b) * (hot ? 2 : 1));
        int mx = HX + 12 + iabs(a - b) * 4;       // bulge right; farther pairs bulge more
        int my = (hy(a) + hy(b)) / 2;
        g.drawLine(HX, hy(a), mx, my, col);
        g.drawLine(mx, my, HX, hy(b), col);
      }
    for (int a = 0; a < n; a++) g.drawCircle(HX + 7, hy(a), 4, recCol(wRec(a, a)));  // self-loop
  }

  // ---- nodes (fill = activation) ----
  for (int i = 0; i < SENSOR_COUNT_FOR_BRAIN; i++) {
    label(g, IX - 8, iy(i) - 3, INLBL[i], C_DIM, textdatum_t::top_right);  // sense name, left of the dot
    g.fillCircle(IX, iy(i), 3, actCol(_in[i]));
  }
  for (int j = 0; j < nHid(); j++) {
    g.fillCircle(HX, hy(j), 5, actCol(_hid[j]));
    if (selLayer == 1 && selIdx == j) g.drawCircle(HX, hy(j), 7, C_ACCENT);
  }
  for (int k = 0; k < 5; k++) {
    g.fillCircle(OX, oy(k), 7, actCol(_out[k] * 2 - 1));
    if (k == _action) g.drawCircle(OX, oy(k), 9, ui::rgb(120, 230, 245));
    if (selLayer == 2 && selIdx == k) g.drawCircle(OX, oy(k), 9, C_ACCENT);
    label(g, OX + 12, oy(k) - 3, OUTLBL[k], k == _action ? ui::rgb(120, 230, 245) : C_DIM);
  }

  // ---- status / zoom strip (just above the toolbar) ----
  int sy = BOTBAR_Y - 16;
  if (_mode == M_LEARN) {
    char m[28]; snprintf(m, sizeof(m), "learning  epoch %d", _epoch);
    label(g, 6, sy, m, C_ACCENT);
    int bw = 120, bx = 150; g.drawRect(bx, sy, bw, 10, C_DIM);
    float f = _loss > 1 ? 1 : (_loss < 0 ? 0 : _loss);
    g.fillRect(bx + 1, sy + 1, (int)((bw - 2) * (1 - f)), 8, C_GO);   // fills as loss falls
  } else if (_sel >= 0) {
    // zoom: the selected neuron's incoming weights as a bar strip + bias + value
    float val = (selLayer == 1) ? _hid[selIdx] : _out[selIdx];
    char m[40];
    snprintf(m, sizeof(m), "%s%d  bias %+.2f  out %+.2f",
             selLayer == 1 ? "hidden " : "output ", selIdx,
             selLayer == 1 ? bHid(selIdx) : bOut(selIdx), val);
    label(g, 6, sy, m, ui::rgb(120, 230, 245));
    int n = (selLayer == 1) ? SENSOR_COUNT_FOR_BRAIN : nHid();
    int bx = 6, bw = (SCREEN_W - 12) / n;
    for (int q = 0; q < n; q++) {
      float w = (selLayer == 1) ? wIn(q, selIdx) : wOut(q, selIdx);
      int h = (int)(w * 7); if (h > 7) h = 7; if (h < -7) h = -7;
      int cx = bx + q * bw + bw / 2;
      g.fillRect(cx - bw / 2 + 1, sy - 8 - (h > 0 ? h : 0), bw - 2, h > 0 ? h : -h, wtCol(w * 3));
    }
  } else {
    char v[28]; snprintf(v, sizeof(v), "decides: %s", OUTLBL[_action]);
    label(g, 6, sy, v, ui::rgb(120, 230, 245));
    label(g, 150, sy, _epoch ? "tap a neuron to zoom" : "tap Teach to train", C_DIM);
  }

  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_TEACH, _mode == M_LEARN ? "..." : "Teach", C_GO, C_PANEL);
  button(g, R_RUN, _mode == M_RUN ? "stop" : "Run", C_GO, C_PANEL);
  char mp[10]; snprintf(mp, sizeof(mp), "Map%d>", _level);
  button(g, R_MAP, mp, C_FUNC, C_PANEL);      // next map, KEEP the brain (transfer learning)
  button(g, R_NEW, "New", C_ACCENT, C_PANEL); // scramble to a fresh untrained brain
  button(g, R_BACK, "Back", C_INK, C_PANEL);
}

app::Signal BrainViewScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  if (_mode == M_LEARN && now - _last >= 70) { _last = now; trainChunk(); draw(); }
  else if (_mode == M_RUN && !_done && now - _last >= 550) { _last = now; stepRun(); draw(); if (_done) _mode = M_IDLE; }

  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  if (R_TYPE.contains(tx, ty)) {
    _useRnn = !_useRnn; _sel = -1; _mode = M_IDLE; resetRun(); hal::audio.blip(); draw();
  } else if (R_TEACH.contains(tx, ty)) {
    if (_mode == M_LEARN) { _mode = M_IDLE; resetRun(); }
    else { _epoch = 0; _mode = M_LEARN; _last = now; }
    hal::audio.blip(); draw();
  } else if (R_RUN.contains(tx, ty)) {
    if (_mode == M_RUN) { _mode = M_IDLE; }
    else { _sel = -1; resetRun(); _mode = M_RUN; _last = now; }
    hal::audio.blip(); draw();
  } else if (R_MAP.contains(tx, ty)) {
    _level = _level >= 14 ? 3 : _level + 1;   // cycle a few campaign maps
    // KEEP the trained brain — it carries over to the new map (transfer learning); Teach
    // then fine-tunes it. Tap "New" to scramble and learn the map from scratch instead.
    _sel = -1; _mode = M_IDLE; loadMaze(); resetRun(); hal::audio.blip(); draw();
  } else if (R_NEW.contains(tx, ty)) {
    _sel = -1; _mode = M_IDLE; resetBrains(); resetRun(); hal::audio.blip(); draw();  // fresh brain
  } else if (R_BACK.contains(tx, ty)) {
    return app::Signal::BACK;
  } else {
    int layer, idx;
    if (nodeAt(tx, ty, layer, idx)) { _sel = layer * 100 + idx; hal::audio.blip(); draw(); }
    else if (_sel >= 0) { _sel = -1; draw(); }
  }
  return app::Signal::NONE;
}

}  // namespace screens
