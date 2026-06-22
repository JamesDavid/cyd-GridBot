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
  _ex = 0; _step = 0;
}
void BackpropLessonScreen::enter() { draw(); }

void BackpropLessonScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Backprop", C_ACCENT, textdatum_t::top_left, 2);
  char hd[24]; snprintf(hd, sizeof(hd), "ex %d/4  -  %s", _ex + 1, STEPN[_step]);
  label(g, SCREEN_W - 6, 6, hd, C_DIM, textdatum_t::top_right);

  int x0 = EX[_ex][0], x1 = EX[_ex][1], t = EX[_ex][2];
  float xin[2] = {(float)x0, (float)x1};
  float pred = _p.forward(xin);
  float err = (float)t - pred;                          // friendly "off by"
  float dz = (pred - (float)t) * pred * (1.0f - pred);  // same chain rule as trainStep
  float dw0 = -_p.lr * dz * x0, dw1 = -_p.lr * dz * x1, db = -_p.lr * dz;

  // --- neuron diagram: two inputs -> one output ---
  int IX = 52, IY0 = 60, IY1 = 110, OX = 176, OY = 86;
  auto wcol = [](float w) { return w >= 0 ? C_GO : C_BAD; };
  g.drawLine(IX, IY0, OX, OY, wcol(_p.w[0]));
  g.drawLine(IX, IY1, OX, OY, wcol(_p.w[1]));
  g.fillCircle(IX, IY0, 14, x0 ? C_MOVE : C_PANEL_HI);
  label(g, IX, IY0, "wall", x0 ? C_BG : C_DIM, textdatum_t::middle_center);
  g.fillCircle(IX, IY1, 14, x1 ? C_SENSE : C_PANEL_HI);
  label(g, IX, IY1, "pit", x1 ? C_BG : C_DIM, textdatum_t::middle_center);
  g.fillCircle(OX, OY, 17, pred > 0.5f ? C_GO : C_PANEL_HI);
  char po[6]; snprintf(po, sizeof(po), "%.2f", pred);
  label(g, OX, OY - 4, po, C_BG, textdatum_t::middle_center);
  label(g, OX, OY + 7, "turn?", C_BG, textdatum_t::middle_center);
  // weight + bias numbers (watch them change on NUDGE)
  char w0s[8], w1s[8], bs[10];
  snprintf(w0s, sizeof(w0s), "%+.2f", _p.w[0]); label(g, 100, IY0 - 14, w0s, wcol(_p.w[0]));
  snprintf(w1s, sizeof(w1s), "%+.2f", _p.w[1]); label(g, 100, IY1 + 6, w1s, wcol(_p.w[1]));
  snprintf(bs, sizeof(bs), "bias %+.2f", _p.b); label(g, OX - 18, OY + 22, bs, C_DIM);

  // --- narration box (changes per sub-step) ---
  int ny = 150;
  g.fillRoundRect(4, ny - 4, SCREEN_W - 8, 44, 5, C_PANEL);
  char l1[44] = "", l2[44] = "";
  switch (_step) {
    case 0:  // LOOK
      snprintf(l1, sizeof(l1), "It sees wall=%d, pit=%d.", x0, x1);
      snprintf(l2, sizeof(l2), "Its guess (turn?) = %.2f", pred);
      break;
    case 1:  // SCORE
      snprintf(l1, sizeof(l1), "We wanted %d; it guessed %.2f.", t, pred);
      snprintf(l2, sizeof(l2), "So it is wrong by %+.2f.", err);
      break;
    case 2:  // BLAME
      snprintf(l1, sizeof(l1), "Nudge ON inputs (OFF inputs: no change):");
      snprintf(l2, sizeof(l2), "w0 %+.2f   w1 %+.2f   bias %+.2f", dw0, dw1, db);
      break;
    default: // NUDGE (weights already updated on entering this step)
      snprintf(l1, sizeof(l1), "Nudged! new guess = %.2f", pred);
      snprintf(l2, sizeof(l2), "closer to %d. Tap Next for the next one.", t);
      break;
  }
  label(g, 12, ny + 2, l1, C_INK);
  label(g, 12, ny + 18, l2, _step == 3 ? C_GO : C_ACCENT);

  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_NEXT, _step == 3 ? "Next example >" : "Next step >", C_GO, C_PANEL);
  button(g, R_RST, "Reset", C_ACCENT, C_PANEL);
  button(g, R_BACK, "Back", C_INK, C_PANEL);
}

app::Signal BackpropLessonScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  if (R_NEXT.contains(tx, ty)) {
    if (_step < 3) {
      _step++;
      if (_step == 3) {  // entering NUDGE: actually apply this example's gradient step
        float xin[2] = {(float)EX[_ex][0], (float)EX[_ex][1]};
        _p.trainStep(xin, (float)EX[_ex][2]);
      }
    } else {
      _step = 0; _ex = (_ex + 1) % 4;  // on to the next situation
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
