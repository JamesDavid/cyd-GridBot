#include "screens/BrainGraph.h"
#include "ui/UI.h"
#include "ui/Theme.h"

using namespace ui;
using namespace gb;

namespace screens {

const char* const BRAIN_INLBL[SENSOR_COUNT_FOR_BRAIN] =
  {"wallF","wallL","wallR","pitF","goalF","goalR","goalD","foeF","foeR","foeD"};
const char* const BRAIN_OUTLBL[5] = {"fwd","turnL","turnR","jump","zap"};

// network geometry (absolute screen coords)
static const int IX = 92, HX = 178, OX = 252;
static int iy(int i) { return 30 + i * 15; }
static int hy(int j) { return 34 + j * 18; }
static int oy(int k) { return 40 + k * 26; }
static int iabs(int v) { return v < 0 ? -v : v; }

static uint16_t actCol(float a) {
  if (a > 0.05f) { int i = (int)(a * 200); if (i > 200) i = 200; return ui::rgb(20, 55 + i, 50); }
  if (a < -0.05f) { int i = (int)(-a * 180); if (i > 180) i = 180; return ui::rgb(60 + i, 28, 28); }
  return C_PANEL_HI;
}
static uint16_t wtCol(float w) {
  float m = w < 0 ? -w : w; if (m > 1.5f) m = 1.5f; int s = (int)(m * 150);
  if (w > 0.03f) return ui::rgb(10, 40 + s, 30);
  if (w < -0.03f) return ui::rgb(40 + s, 18, 18);
  return ui::rgb(30, 30, 38);
}
static uint16_t recCol(float w) {
  float m = w < 0 ? -w : w; if (m > 1.5f) m = 1.5f; int s = (int)(m * 130);
  if (w > 0.03f) return ui::rgb(20, 70 + s / 2, 110 + s);
  if (w < -0.03f) return ui::rgb(70 + s, 25, 110 + s);
  return ui::rgb(24, 28, 44);
}

float brainWIn(const Net* ff, const RNet* rnn, bool useRnn, int i, int j) { return useRnn ? rnn->wih[j][i] : ff->w1[j][i]; }
float brainWOut(const Net* ff, const RNet* rnn, bool useRnn, int j, int k) { return useRnn ? rnn->who[k][j] : ff->w2[k][j]; }
static float wRec(const RNet* rnn, bool useRnn, int a, int b) { return useRnn ? rnn->whh[b][a] : 0.0f; }
float brainBHid(const Net* ff, const RNet* rnn, bool useRnn, int j) { return useRnn ? rnn->bh[j] : ff->b1[j]; }
float brainBOut(const Net* ff, const RNet* rnn, bool useRnn, int k) { return useRnn ? rnn->bo[k] : ff->b2[k]; }

bool brainGraphNodeAt(int x, int y, int nHid, int& layer, int& idx) {
  for (int j = 0; j < nHid; j++) if (iabs(x - HX) < 10 && iabs(y - hy(j)) < 9) { layer = 1; idx = j; return true; }
  for (int k = 0; k < 5; k++) if (iabs(x - OX) < 12 && iabs(y - oy(k)) < 12) { layer = 2; idx = k; return true; }
  return false;
}

void drawBrainGraph(LGFX& g, const Net* ff, const RNet* rnn, bool useRnn,
                    const float* in, const float* hid, const float* out, int action, int sel) {
  int nHid = useRnn ? rnn->nHid : ff->nHid;
  int selLayer = sel < 0 ? -1 : sel / 100, selIdx = sel < 0 ? -1 : sel % 100;

  // connections coloured by WEIGHT (recolour live as backprop runs)
  for (int i = 0; i < SENSOR_COUNT_FOR_BRAIN; i++)
    for (int j = 0; j < nHid; j++) {
      bool hot = (selLayer == 1 && selIdx == j);
      float w = brainWIn(ff, rnn, useRnn, i, j);
      g.drawLine(IX, iy(i), HX, hy(j), hot ? wtCol(w * 2) : wtCol(w));
    }
  for (int j = 0; j < nHid; j++)
    for (int k = 0; k < 5; k++) {
      bool hot = (selLayer == 2 && selIdx == k);
      float w = brainWOut(ff, rnn, useRnn, j, k);
      g.drawLine(HX, hy(j), OX, oy(k), hot ? wtCol(w * 2) : wtCol(w));
    }
  if (useRnn) {  // the FULL hidden->hidden recurrent matrix + self-loops
    for (int a = 0; a < nHid; a++)
      for (int b = 0; b < nHid; b++) {
        if (a == b) continue;
        bool hot = (selLayer == 1 && (selIdx == a || selIdx == b));
        uint16_t col = recCol(wRec(rnn, useRnn, a, b) * (hot ? 2 : 1));
        int mx = HX + 12 + iabs(a - b) * 4, my = (hy(a) + hy(b)) / 2;
        g.drawLine(HX, hy(a), mx, my, col);
        g.drawLine(mx, my, HX, hy(b), col);
      }
    for (int a = 0; a < nHid; a++) g.drawCircle(HX + 7, hy(a), 4, recCol(wRec(rnn, useRnn, a, a)));
  }

  // nodes (fill = activation), with input + output labels
  for (int i = 0; i < SENSOR_COUNT_FOR_BRAIN; i++) {
    label(g, IX - 8, iy(i) - 3, BRAIN_INLBL[i], C_DIM, textdatum_t::top_right);
    g.fillCircle(IX, iy(i), 3, actCol(in[i]));
  }
  for (int j = 0; j < nHid; j++) {
    g.fillCircle(HX, hy(j), 5, actCol(hid[j]));
    if (selLayer == 1 && selIdx == j) g.drawCircle(HX, hy(j), 7, C_ACCENT);
  }
  for (int k = 0; k < 5; k++) {
    g.fillCircle(OX, oy(k), 7, actCol(out[k] * 2 - 1));
    if (k == action) g.drawCircle(OX, oy(k), 9, ui::rgb(120, 230, 245));
    if (selLayer == 2 && selIdx == k) g.drawCircle(OX, oy(k), 9, C_ACCENT);
    label(g, OX + 12, oy(k) - 3, BRAIN_OUTLBL[k], k == action ? ui::rgb(120, 230, 245) : C_DIM);
  }
}

}  // namespace screens
