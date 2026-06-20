#include "screens/BrainMapScreen.h"
#include "hal/Audio.h"

using namespace ui;

namespace screens {

static const uint16_t CYAN = ui::rgb(120, 230, 245);

static const Rect R_BACK = {6,   (int16_t)(BOTBAR_Y + 2), 74, 26};
static const Rect R_PREV = {120, (int16_t)(BOTBAR_Y + 2), 88, 26};
static const Rect R_NEXT = {224, (int16_t)(BOTBAR_Y + 2), 88, 26};
static const int N_PAGE = 3;

void BrainMapScreen::enter() { _page = 0; draw(); }

// the real brain's shape, drawn small on the left; `highlight` lights the layer this page is about
void BrainMapScreen::drawSchematic(int highlight) {
  auto& g = hal::display.gfx();
  const int IX = 22, HX = 54, OX = 84;
  const int iy0 = 48, idy = 13;   // 10 inputs
  const int hy0 = 54, hdy = 16;   // 8 hidden
  const int oy0 = 66, ody = 26;   // 5 outputs
  uint16_t cin  = (highlight == 0 || highlight == 2) ? CYAN    : C_LOCK;
  uint16_t chid = (highlight == 2)                   ? C_SENSE : C_LOCK;
  uint16_t cout = (highlight == 1 || highlight == 2) ? CYAN    : C_LOCK;
  if (highlight == 2) {  // show the wiring only on the "whole brain" page
    for (int i = 0; i < 10; i++)
      for (int j = 0; j < 8; j++) g.drawLine(IX, iy0 + i * idy, HX, hy0 + j * hdy, C_PANEL);
    for (int j = 0; j < 8; j++)
      for (int k = 0; k < 5; k++) g.drawLine(HX, hy0 + j * hdy, OX, oy0 + k * ody, C_PANEL);
  }
  for (int i = 0; i < 10; i++) g.fillCircle(IX, iy0 + i * idy, 2, cin);
  for (int j = 0; j < 8; j++)  g.fillCircle(HX, hy0 + j * hdy, 3, chid);
  for (int k = 0; k < 5; k++)  g.fillCircle(OX, oy0 + k * ody, 3, cout);
  label(g, IX - 4, 36, "in",  cin);
  label(g, HX - 6, 36, "hid", chid);
  label(g, OX - 6, 36, "out", cout);
}

void BrainMapScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Robot Brain", CYAN, textdatum_t::top_left, 2);
  char pg[8]; snprintf(pg, sizeof(pg), "%d/%d", _page + 1, N_PAGE);
  label(g, SCREEN_W - 6, 6, pg, C_DIM, textdatum_t::top_right);

  drawSchematic(_page);  // page 0 lights inputs, 1 lights outputs, 2 the whole net

  const int X = 100;  // text column to the right of the schematic
  if (_page == 0) {
    label(g, X, 40, "WHAT IT SENSES", CYAN);
    label(g, X, 54, "10 numbers, every step", C_DIM);
    label(g, X, 74,  "Walls (3)", C_MOVE);
    label(g, X, 86,  "wall ahead?  left?  right?", C_DIM);
    label(g, X, 104, "Pit (1)", C_TURN);
    label(g, X, 116, "a hole right in front?", C_DIM);
    label(g, X, 134, "Goal (3)", C_ACCENT);
    label(g, X, 146, "battery: ahead? right? far?", C_DIM);
    label(g, X, 164, "Enemy (3)", C_BAD);
    label(g, X, 176, "a rival: ahead? right? far?", C_DIM);
    label(g, X, 196, "all from the robot's own view", C_DIM);
  } else if (_page == 1) {
    label(g, X, 40, "WHAT IT DOES", CYAN);
    label(g, X, 54, "it picks the strongest one", C_DIM);
    const int N = X + 78;
    label(g, X, 80,  "forward",   C_GO);   label(g, N, 80,  "step ahead", C_DIM);
    label(g, X, 104, "turn L/R",  C_TURN); label(g, N, 104, "spin in place", C_DIM);
    label(g, X, 128, "jump",      C_LOOP); label(g, N, 128, "leap a pit", C_DIM);
    label(g, X, 152, "zap",       C_BAD);  label(g, N, 152, "blast a rival", C_DIM);
    label(g, X, 178, "(zap only works in the arena)", C_DIM);
  } else {
    label(g, X, 40, "ONE BRAIN, EVERY JOB", CYAN);
    label(g, X, 66,  "10 senses", C_MOVE);
    label(g, X, 84,  "8 hidden neurons", C_SENSE);
    label(g, X, 102, "5 actions", C_GO);
    label(g, X, 128, "The SAME brain solves mazes", C_INK);
    label(g, X, 140, "AND fights in the arena.", C_INK);
    label(g, X, 164, "Train it, drop a brain block", C_DIM);
    label(g, X, 176, "in your code, or watch it", C_DIM);
    label(g, X, 188, "think in Brain Cam.", C_DIM);
  }

  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Lab", C_INK, C_PANEL);
  button(g, R_PREV, "< Prev", _page > 0 ? C_ACCENT : C_LOCK, C_PANEL);
  button(g, R_NEXT, _page < N_PAGE - 1 ? "Next >" : "done", _page < N_PAGE - 1 ? CYAN : C_GO, C_PANEL);
}

app::Signal BrainMapScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  if (R_BACK.contains(tx, ty)) return app::Signal::BACK;
  if (R_PREV.contains(tx, ty) && _page > 0) { _page--; hal::audio.blip(); draw(); }
  else if (R_NEXT.contains(tx, ty)) {
    if (_page < N_PAGE - 1) { _page++; hal::audio.blip(); draw(); }
    else return app::Signal::BACK;
  }
  return app::Signal::NONE;
}

}  // namespace screens
