#include "screens/NeuroLessonScreen.h"
#include "hal/Audio.h"

using namespace ui;

namespace screens {

// The lesson: TURN (1) if wall OR pit, else GO (0). Linearly separable -> one neuron
// can learn it (unlike XOR, the next lesson).
static const float DX[8] = {0,0, 0,1, 1,0, 1,1};
static const float DY[4] = {0, 1, 1, 1};
static const int N_EX = 4;

static const Rect R_T1   = {6,   (int16_t)(BOTBAR_Y + 2), 74, 26};
static const Rect R_T50  = {84,  (int16_t)(BOTBAR_Y + 2), 86, 26};
static const Rect R_RST  = {174, (int16_t)(BOTBAR_Y + 2), 64, 26};
static const Rect R_BACK = {242, (int16_t)(BOTBAR_Y + 2), 72, 26};

// diagram node centres
static const int WALL_X = 40, WALL_Y = 66;
static const int PIT_X = 40,  PIT_Y = 120;
static const int OUT_X = 122, OUT_Y = 92;

void NeuroLessonScreen::begin() {
  _p.nIn = 2;
  _p.lr = 0.6f;
  _p.reset(7);
  _epochs = 0;
  _loss = lossNow();
}

void NeuroLessonScreen::enter() { draw(); }

float NeuroLessonScreen::lossNow() const {
  float s = 0;
  for (int i = 0; i < N_EX; i++) {
    float pred = _p.forward(&DX[i * 2]);
    float e = pred - DY[i];
    s += e * e;
  }
  return s / N_EX;
}

void NeuroLessonScreen::trainEpochs(int n) {
  for (int e = 0; e < n; e++) _p.trainEpoch(DX, DY, N_EX);
  _epochs += n;
  _loss = lossNow();
  hal::audio.blip();
  draw();
}

static void edge(LGFX& g, int x0, int y0, int x1, int y1, float w) {
  uint16_t col = (w >= 0) ? C_GO : C_BAD;     // green = pushes toward TURN, red = toward GO
  int th = 1 + (int)(fabsf(w) * 2.0f);
  if (th > 7) th = 7;
  g.drawWideLine(x0, y0, x1, y1, th, col);
}

void NeuroLessonScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Neuron: watch it learn", C_ACCENT, textdatum_t::top_left, 2);
  char hd[28]; snprintf(hd, sizeof(hd), "epoch %d", _epochs);
  label(g, SCREEN_W - 6, 6, hd, C_DIM, textdatum_t::top_right);

  label(g, 6, TOPBAR_H + 2, "Goal: TURN if wall or pit, else GO", C_INK);

  // ---- the neuron diagram (left) ----
  edge(g, WALL_X, WALL_Y, OUT_X, OUT_Y, _p.w[0]);
  edge(g, PIT_X, PIT_Y, OUT_X, OUT_Y, _p.w[1]);
  // input nodes
  g.fillCircle(WALL_X, WALL_Y, 15, C_MOVE); g.drawCircle(WALL_X, WALL_Y, 15, C_INK);
  label(g, WALL_X, WALL_Y, "wall", C_BG, textdatum_t::middle_center);
  g.fillCircle(PIT_X, PIT_Y, 15, C_PANEL_HI); g.drawCircle(PIT_X, PIT_Y, 15, C_INK);
  label(g, PIT_X, PIT_Y, "pit", C_INK, textdatum_t::middle_center);
  // output node
  g.fillCircle(OUT_X, OUT_Y, 17, C_LOOP); g.drawCircle(OUT_X, OUT_Y, 17, C_INK);
  label(g, OUT_X, OUT_Y - 4, "act", C_BG, textdatum_t::middle_center);
  char bb[10]; snprintf(bb, sizeof(bb), "b%+.1f", _p.b);
  label(g, OUT_X, OUT_Y + 7, bb, C_BG, textdatum_t::middle_center);
  // weight readouts
  char w0[10], w1[10];
  snprintf(w0, sizeof(w0), "%+.2f", _p.w[0]); label(g, 70, 54, w0, C_DIM);
  snprintf(w1, sizeof(w1), "%+.2f", _p.w[1]); label(g, 70, 124, w1, C_DIM);

  // ---- the four examples (right): does it get each case right? ----
  int ex0 = 150;
  for (int i = 0; i < N_EX; i++) {
    int y = TOPBAR_H + 18 + i * 42;
    int wall = (int)DX[i * 2], pit = (int)DX[i * 2 + 1];
    // sensor swatches
    g.fillRoundRect(ex0, y, 16, 16, 3, wall ? C_MOVE : C_PANEL);
    g.drawRoundRect(ex0, y, 16, 16, 3, C_DIM);
    g.fillRoundRect(ex0 + 20, y, 16, 16, 3, pit ? C_PANEL_HI : C_PANEL);
    g.drawRoundRect(ex0 + 20, y, 16, 16, 3, C_DIM);
    label(g, ex0 + 40, y + 4, "->", C_DIM);
    float pred = _p.forward(&DX[i * 2]);
    bool turn = pred > 0.5f;
    bool want = DY[i] > 0.5f;
    bool ok = (turn == want);
    label(g, ex0 + 58, y + 4, turn ? "TURN" : "GO", turn ? C_ACCENT : C_GO);
    // confidence bar (how sure the neuron is)
    int bx = ex0 + 58, bw = 70;
    g.drawRect(bx, y + 18, bw, 6, C_PANEL_HI);
    g.fillRect(bx, y + 18, (int)(bw * pred), 6, turn ? C_ACCENT : C_GO);
    label(g, ex0 + 134, y + 4, ok ? "ok" : "X", ok ? C_GO : C_BAD);
  }

  // ---- loss bar ----
  int ly = TOPBAR_H + 18 + N_EX * 42 - 6;
  char lb[20]; snprintf(lb, sizeof(lb), "error %.3f", _loss);
  label(g, 6, ly, lb, _loss < 0.02f ? C_GO : C_INK);
  int barW = 120;
  g.drawRect(80, ly, barW, 8, C_PANEL_HI);
  int fill = (int)(barW * (_loss / 0.25f)); if (fill > barW) fill = barW;
  g.fillRect(80, ly, fill, 8, _loss < 0.02f ? C_GO : C_BAD);
  if (_loss < 0.02f) label(g, 210, ly - 1, "learned!", C_GO);

  // ---- buttons ----
  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_T1, "Train", C_GO, C_PANEL);
  button(g, R_T50, "Train x50", C_GO, C_PANEL);
  button(g, R_RST, "Reset", C_ACCENT, C_PANEL);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
}

app::Signal NeuroLessonScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  if (R_T1.contains(tx, ty)) { trainEpochs(1); return app::Signal::NONE; }
  if (R_T50.contains(tx, ty)) { trainEpochs(50); return app::Signal::NONE; }
  if (R_RST.contains(tx, ty)) { _p.reset((uint32_t)(now | 1u)); _epochs = 0; _loss = lossNow(); hal::audio.blip(); draw(); return app::Signal::NONE; }
  if (R_BACK.contains(tx, ty)) return app::Signal::BACK;
  return app::Signal::NONE;
}

}  // namespace screens
