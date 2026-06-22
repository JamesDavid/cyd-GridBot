#include "screens/BackpropLessonScreen.h"
#include "hal/Audio.h"
#include <math.h>

using namespace ui;
using namespace gb;

namespace screens {

// The rule it learns: "turn if wall OR pit". Four situations (wall, pit -> should-turn).
static const int EX[4][3] = {{0, 0, 0}, {0, 1, 1}, {1, 0, 1}, {1, 1, 1}};
static const char* STEPN[4] = {"LOOK", "SCORE", "BLAME", "NUDGE"};

static const Rect R_NEXT = {6,   (int16_t)(BOTBAR_Y + 2), 150, 26};
static const Rect R_RST  = {162, (int16_t)(BOTBAR_Y + 2), 76,  26};
static const Rect R_BACK = {244, (int16_t)(BOTBAR_Y + 2), 70,  26};

void BackpropLessonScreen::begin() {
  _p.nIn = 2; _p.lr = 0.6f; _p.reset(7);
  _ex = -1; _step = 0; _errN = 0;   // start on the intro slide
}
void BackpropLessonScreen::enter() { draw(); }

float BackpropLessonScreen::meanErr() const {
  float s = 0;
  for (int i = 0; i < 4; i++) {
    float xi[2] = {(float)EX[i][0], (float)EX[i][1]};
    float e = (float)EX[i][2] - _p.forward(xi);
    s += e < 0 ? -e : e;
  }
  return s / 4;
}

void BackpropLessonScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Backprop", C_ACCENT, textdatum_t::top_left, 2);

  // ---------- intro slide: the LOOK/SCORE/BLAME/NUDGE loop ----------
  if (_ex < 0) {
    label(g, SCREEN_W - 6, 6, "how a neuron learns", C_DIM, textdatum_t::top_right);
    g.fillRoundRect(20, 36, SCREEN_W - 40, 138, 8, C_PANEL);
    label(g, SCREEN_W / 2, 46, "4 moves, on repeat:", C_INK, textdatum_t::top_center);
    label(g, 44, 70,  "1. LOOK", C_GO);     label(g, 130, 70,  "make a guess", C_DIM);
    label(g, 44, 92,  "2. SCORE", C_ACCENT);label(g, 130, 92,  "how wrong was it?", C_DIM);
    label(g, 44, 114, "3. BLAME", C_BAD);   label(g, 130, 114, "which weights caused it?", C_DIM);
    label(g, 44, 136, "4. NUDGE", C_MOVE);  label(g, 130, 136, "fix them a little", C_DIM);
    label(g, SCREEN_W / 2, 158, "...repeat per example until it's right.", C_DIM, textdatum_t::top_center);
    g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
    button(g, R_NEXT, "Start >", C_GO, C_PANEL);
    button(g, R_BACK, "Back", C_INK, C_PANEL);
    return;
  }

  char hd[24]; snprintf(hd, sizeof(hd), "ex %d/4  -  %s", _ex + 1, STEPN[_step]);
  label(g, SCREEN_W - 6, 6, hd, C_DIM, textdatum_t::top_right);

  int x0 = EX[_ex][0], x1 = EX[_ex][1], t = EX[_ex][2];
  float xin[2] = {(float)x0, (float)x1};
  float pred = _p.forward(xin);
  float err = (float)t - pred;                          // friendly "off by"
  float dz = (pred - (float)t) * pred * (1.0f - pred);  // same chain rule as trainStep
  float dw0 = -_p.lr * dz * x0, dw1 = -_p.lr * dz * x1, db = -_p.lr * dz;

  // ---------- neuron diagram: inputs (name + value) -> output (guess + desired) ----------
  int IX = 54, IY0 = 42, IY1 = 72, OX = 182, OY = 54;
  auto wcol = [](float w) { return w >= 0 ? C_GO : C_BAD; };
  g.drawLine(IX, IY0, OX, OY, wcol(_p.w[0]));
  g.drawLine(IX, IY1, OX, OY, wcol(_p.w[1]));
  g.fillCircle(IX, IY0, 12, x0 ? C_MOVE : C_PANEL_HI);
  label(g, IX, IY0, x0 ? "1" : "0", x0 ? C_BG : C_DIM, textdatum_t::middle_center);
  label(g, IX - 16, IY0, "wall", x0 ? C_MOVE : C_DIM, textdatum_t::middle_right);
  g.fillCircle(IX, IY1, 12, x1 ? C_SENSE : C_PANEL_HI);
  label(g, IX, IY1, x1 ? "1" : "0", x1 ? C_BG : C_DIM, textdatum_t::middle_center);
  label(g, IX - 16, IY1, "pit", x1 ? C_SENSE : C_DIM, textdatum_t::middle_right);
  char w0s[8], w1s[8]; snprintf(w0s, sizeof(w0s), "%+.2f", _p.w[0]); snprintf(w1s, sizeof(w1s), "%+.2f", _p.w[1]);
  label(g, 104, IY0 - 12, w0s, wcol(_p.w[0]));
  label(g, 104, IY1 + 4, w1s, wcol(_p.w[1]));
  g.fillCircle(OX, OY, 16, pred > 0.5f ? C_GO : C_PANEL_HI);
  char po[6]; snprintf(po, sizeof(po), "%.2f", pred);
  label(g, OX, OY - 3, po, C_BG, textdatum_t::middle_center);
  label(g, OX, OY + 7, "guess", C_BG, textdatum_t::middle_center);
  char bs[10]; snprintf(bs, sizeof(bs), "bias %+.2f", _p.b); label(g, OX, OY + 22, bs, C_DIM, textdatum_t::middle_center);
  char want[10]; snprintf(want, sizeof(want), "want %d", t);
  label(g, OX + 22, OY - 3, want, C_ACCENT, textdatum_t::middle_left);

  // ---------- convergence chart: error after each nudge, trending to 0 ----------
  int cy0 = 104, ch = 24, cx0 = 8, cw = SCREEN_W - 16;
  char es[40]; snprintf(es, sizeof(es), "error: %.2f   (lower = learning)", meanErr());
  label(g, cx0, cy0 - 12, es, meanErr() < 0.1f ? C_GO : C_INK);
  g.drawFastHLine(cx0, cy0 + ch, cw, C_PANEL_HI);
  int bw = 6;
  for (int i = 0; i < _errN && i < 48; i++) {
    int h = (int)(_err[i] / 0.5f * ch); if (h > ch) h = ch; if (h < 1) h = 1;
    g.fillRect(cx0 + i * bw, cy0 + ch - h, bw - 1, h, _err[i] < 0.1f ? C_GO : C_MOVE);
  }

  // ---------- narration (changes per sub-step) ----------
  int ny = 134;
  g.fillRoundRect(4, ny - 2, SCREEN_W - 8, 42, 5, C_PANEL);
  char l1[44] = "", l2[44] = "";
  switch (_step) {
    case 0:  snprintf(l1, sizeof(l1), "LOOK: with wall=%d pit=%d, it guesses %.2f.", x0, x1, pred);
             snprintf(l2, sizeof(l2), "(we want %d.)", t); break;
    case 1:  snprintf(l1, sizeof(l1), "SCORE: wanted %d, guessed %.2f.", t, pred);
             snprintf(l2, sizeof(l2), "So it is wrong by %+.2f.", err); break;
    case 2:  snprintf(l1, sizeof(l1), "BLAME ON inputs (OFF inputs: no change):");
             snprintf(l2, sizeof(l2), "w0 %+.2f   w1 %+.2f   bias %+.2f", dw0, dw1, db); break;
    default: snprintf(l1, sizeof(l1), "NUDGE: new guess = %.2f (closer to %d!).", pred, t);
             snprintf(l2, sizeof(l2), "Watch the error bar shrink as it learns."); break;
  }
  label(g, 12, ny + 4, l1, C_INK);
  label(g, 12, ny + 22, l2, _step == 3 ? C_GO : C_ACCENT);

  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_NEXT, _step == 3 ? "Next example >" : "Next step >", C_GO, C_PANEL);
  button(g, R_RST, "Reset", C_ACCENT, C_PANEL);
  button(g, R_BACK, "Back", C_INK, C_PANEL);
}

app::Signal BackpropLessonScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  if (R_NEXT.contains(tx, ty)) {
    if (_ex < 0) { _ex = 0; _step = 0; }      // leave the intro slide
    else if (_step < 3) {
      _step++;
      if (_step == 3) {  // entering NUDGE: apply this example's gradient step + log the error
        float xin[2] = {(float)EX[_ex][0], (float)EX[_ex][1]};
        _p.trainStep(xin, (float)EX[_ex][2]);
        if (_errN < 48) _err[_errN++] = meanErr();
      }
    } else {
      _step = 0; _ex = (_ex + 1) % 4;
    }
    hal::audio.blip(); draw();
  } else if (R_RST.contains(tx, ty)) {
    begin(); hal::audio.blip(); draw();
  } else if (R_BACK.contains(tx, ty)) {
    return app::Signal::BACK;
  }
  return app::Signal::NONE;
}

}  // namespace screens
