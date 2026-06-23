#include "screens/NeuroLessonScreen.h"
#include "hal/Audio.h"

using namespace ui;
using namespace gb;

namespace screens {

// 4 situations (the truth table of 2 yes/no sensors: wall, pit).
static const float DX[8] = {0,0, 0,1, 1,0, 1,1};
static const float DY0[4] = {0, 1, 1, 1};                      // mode 0: turn if wall OR pit
static const float DY1[12] = {1,0,0,  0,0,1,  0,1,0,  0,0,1};  // mode 1: go/turn/jump (one-hot)
static const float DY2[4] = {0, 1, 1, 0};                      // mode 2: XOR (needs a hidden layer)

static const int N_EX = 4;
static const Rect R_T1   = {6,   (int16_t)(BOTBAR_Y + 2), 74, 26};
static const Rect R_T50  = {84,  (int16_t)(BOTBAR_Y + 2), 86, 26};
static const Rect R_RST  = {174, (int16_t)(BOTBAR_Y + 2), 64, 26};
static const Rect R_BACK = {242, (int16_t)(BOTBAR_Y + 2), 72, 26};

static const char* TITLES[3] = {"Neuron: watch it learn", "Multi-class: 3 actions", "Hidden layer: corners"};
static const char* RULES[3]  = {"turn if WALL or PIT", "jump if pit, turn if wall, else go", "turn if EXACTLY ONE side is open"};

void NeuroLessonScreen::begin(int mode) {
  _mode = mode;
  _epochs = 0;
  _seed = (mode == 0) ? 7u : (mode == 1) ? 11u : 5u;
  if (mode == 0) { _p.nIn = 2; _p.lr = 0.6f; _p.reset(_seed); }
  else if (mode == 1) { _net.config(2, 5, 3, _seed); _net.lr = 0.6f; }
  else { _net.config(2, 10, 1, _seed); _net.lr = 0.5f; }  // XOR: roomy hidden layer to converge
  _loss = lossNow();
}

void NeuroLessonScreen::enter() { draw(); }

int NeuroLessonScreen::decision(int i) const {
  if (_mode == 0) { float p = _p.forward(&DX[i * 2]); return p > 0.5f ? 1 : 0; }
  if (_mode == 1) return _net.argmax(&DX[i * 2]);
  float o[NET_MAX_OUT]; _net.forward(&DX[i * 2], o); return o[0] > 0.5f ? 1 : 0;
}

static int correctClass(int mode, int i) {
  if (mode == 0) return DY0[i] > 0.5f ? 1 : 0;
  if (mode == 1) { const float* t = &DY1[i * 3]; int b = 0; for (int k = 1; k < 3; k++) if (t[k] > t[b]) b = k; return b; }
  return DY2[i] > 0.5f ? 1 : 0;
}

static const char* actionName(int mode, int cls) {
  if (mode == 1) { static const char* a[3] = {"GO", "TURN", "JUMP"}; return a[cls]; }
  if (mode == 2) return cls ? "turn" : "go";
  return cls ? "TURN" : "GO";
}

float NeuroLessonScreen::lossNow() const {
  float s = 0; int outs = (_mode == 1) ? 3 : 1;
  for (int i = 0; i < N_EX; i++) {
    if (_mode == 0) { float e = _p.forward(&DX[i * 2]) - DY0[i]; s += e * e; }
    else {
      float o[NET_MAX_OUT]; _net.forward(&DX[i * 2], o);
      const float* t = (_mode == 1) ? &DY1[i * 3] : &DY2[i];
      for (int k = 0; k < outs; k++) { float e = o[k] - t[k]; s += e * e; }
    }
  }
  return s / (N_EX * outs);
}

void NeuroLessonScreen::trainEpochs(int n) {
  for (int e = 0; e < n; e++) {
    if (_mode == 0) _p.trainEpoch(DX, DY0, N_EX);
    else _net.trainEpoch(DX, (_mode == 1) ? DY1 : DY2, N_EX);
  }
  _epochs += n;
  _loss = lossNow();
  hal::audio.blip();
  draw();
}

void NeuroLessonScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, TITLES[_mode], C_ACCENT, textdatum_t::top_left, 2);
  char hd[20]; snprintf(hd, sizeof(hd), "epoch %d", _epochs);
  label(g, SCREEN_W - 6, 6, hd, C_DIM, textdatum_t::top_right);
  label(g, 6, TOPBAR_H + 2, RULES[_mode], C_INK);  // rule spans the top line (the column
  // headers below — "what it sees / it does" — already label the situations table)

  // ---- diagram (left) ----
  if (_mode == 0) {
    // 2 inputs -> 1 output, weight edges colored/thickened
    int WX = 40, WY = 66, PX = 40, PY = 120, OX = 122, OY = 92;
    for (int k = 0; k < 2; k++) {
      float w = _p.w[k]; uint16_t col = (w >= 0) ? C_GO : C_BAD; int th = 1 + (int)(fabsf(w) * 2); if (th > 7) th = 7;
      g.drawWideLine(40, k ? PY : WY, OX, OY, th, col);
    }
    g.fillCircle(WX, WY, 15, C_MOVE); label(g, WX, WY, "wall", C_BG, textdatum_t::middle_center);
    g.fillCircle(PX, PY, 15, C_PANEL_HI); label(g, PX, PY, "pit", C_INK, textdatum_t::middle_center);
    g.fillCircle(OX, OY, 17, C_LOOP); label(g, OX, OY - 4, "out", C_BG, textdatum_t::middle_center);
    // the actual weights + bias as numbers, so you watch them change each Train
    char w0[8], w1[8], bb[10];
    snprintf(w0, sizeof(w0), "%+.2f", _p.w[0]); label(g, 62, 56, w0, _p.w[0] >= 0 ? C_GO : C_BAD);
    snprintf(w1, sizeof(w1), "%+.2f", _p.w[1]); label(g, 62, 116, w1, _p.w[1] >= 0 ? C_GO : C_BAD);
    snprintf(bb, sizeof(bb), "b%+.2f", _p.b); label(g, OX, OY + 7, bb, C_BG, textdatum_t::middle_center);
    label(g, 8, 150, "weights & bias update as it learns", C_DIM);
  } else {
    // 2 inputs -> hidden row -> outputs (a layered blob — "more neurons").
    // mode 1 reacts to wall/pit; mode 2 senses whether the LEFT/RIGHT side is open (corners).
    const char* nA = (_mode == 2) ? "L" : "wall";
    const char* nB = (_mode == 2) ? "R" : "pit";
    g.fillCircle(30, 70, 12, C_MOVE); label(g, 30, 70, nA, C_BG, textdatum_t::middle_center);
    g.fillCircle(30, 120, 12, C_PANEL_HI); label(g, 30, 120, nB, C_INK, textdatum_t::middle_center);
    int outs = (_mode == 1) ? 3 : 1;
    for (int j = 0; j < 4; j++) {                 // hidden layer
      int hy = 50 + j * 30;
      for (int s = 0; s < 2; s++) g.drawLine(42, s ? 120 : 70, 80, hy, C_PANEL_HI);
      for (int k = 0; k < outs; k++) g.drawLine(80, hy, 120, 70 + k * 26, C_PANEL_HI);
      g.fillCircle(80, hy, 7, C_SENSE);
    }
    for (int k = 0; k < outs; k++) { g.fillCircle(120, 70 + k * 26, 9, C_LOOP); }
    char cap[24]; snprintf(cap, sizeof(cap), "hidden layer + %d output%s", outs, outs > 1 ? "s" : "");
    label(g, 8, 152, cap, C_DIM);
  }

  // ---- the four situations (right): every wall/pit combination, in words ----
  int ex0 = 150;
  label(g, ex0, TOPBAR_H + 14, "what it sees", C_DIM);
  label(g, ex0 + 90, TOPBAR_H + 14, "it does", C_DIM);
  for (int i = 0; i < N_EX; i++) {
    int y = TOPBAR_H + 28 + i * 38;
    int wall = (int)DX[i * 2], pit = (int)DX[i * 2 + 1];
    // a bright word when the input is ON, dim when OFF -> you read why each row decides.
    // modes 0/1 are wall/pit sensors; mode 2 (XOR) is two abstract inputs A,B.
    label(g, ex0, y, _mode == 2 ? (wall ? "L" : "l") : (wall ? "WALL" : "wall"), wall ? C_MOVE : C_LOCK);
    label(g, ex0 + 34, y, _mode == 2 ? (pit ? "R" : "r") : (pit ? "PIT" : "pit"), pit ? C_SENSE : C_LOCK);
    label(g, ex0 + 58, y, "->", C_DIM);
    int act = decision(i), want = correctClass(_mode, i);
    bool ok = (act == want);
    uint16_t ac = (_mode == 1) ? (act == 0 ? C_GO : act == 1 ? C_ACCENT : C_MOVE) : (act ? C_ACCENT : C_GO);
    label(g, ex0 + 74, y, actionName(_mode, act), ac);
    label(g, ex0 + 134, y, ok ? "ok" : "X", ok ? C_GO : C_BAD);
  }

  int ly = BOTBAR_Y - 14;  // anchored just above the toolbar (was pushed under it by the taller bar)
  char lb[20]; snprintf(lb, sizeof(lb), "loss %.3f", _loss);  // "loss" = how wrong it still is
  label(g, 6, ly, lb, _loss < 0.02f ? C_GO : C_INK);
  int barW = 120; g.drawRect(80, ly, barW, 8, C_PANEL_HI);
  int fill = (int)(barW * (_loss / 0.25f)); if (fill > barW) fill = barW;
  g.fillRect(80, ly, fill, 8, _loss < 0.02f ? C_GO : C_BAD);
  if (_loss < 0.02f) label(g, 210, ly - 1, "learned!", C_GO);

  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_T1, "step", C_GO, C_PANEL);
  button(g, R_T50, _mode == 0 ? "Train x50" : "Train x500", C_GO, C_PANEL);
  button(g, R_RST, "Reset", C_ACCENT, C_PANEL);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
}

app::Signal NeuroLessonScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  if (R_T1.contains(tx, ty)) { trainEpochs(1); return app::Signal::NONE; }
  if (R_T50.contains(tx, ty)) { trainEpochs(_mode == 0 ? 50 : _mode == 1 ? 500 : 1500); return app::Signal::NONE; }
  if (R_RST.contains(tx, ty)) {
    // a fresh random start (e.g. to escape an XOR local minimum)
    _seed = _seed * 1664525u + 1013904223u; _epochs = 0;
    if (_mode == 0) _p.reset(_seed);
    else if (_mode == 1) _net.config(2, 5, 3, _seed);
    else _net.config(2, 10, 1, _seed);
    _loss = lossNow(); hal::audio.blip(); draw(); return app::Signal::NONE;
  }
  if (R_BACK.contains(tx, ty)) return app::Signal::BACK;
  return app::Signal::NONE;
}

}  // namespace screens
