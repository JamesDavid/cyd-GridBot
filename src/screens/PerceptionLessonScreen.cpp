#include "screens/PerceptionLessonScreen.h"
#include "hal/Audio.h"
#include "assets/Assets.h"
#include "game/Types.h"

using namespace ui;

namespace screens {

// A few example "robot's-eye views": is there a wall ahead / left / right?
struct View { bool a, l, r; };
static const View VIEWS[4] = {
  {false, true,  false},   // wall on the left, open ahead  -> NO
  {true,  false, false},   // wall straight ahead           -> YES
  {false, false, true },   // wall on the right, open ahead -> NO
  {true,  true,  false},   // boxed in ahead + left         -> YES
};
static const int NUM_VIEWS = 4;

static const char* STATUS[3] = {
  "The brain is HANDED 'wall ahead = 1'. But what LOOKS at the world?",
  "A perception net reads the raw squares -> decides: wall ahead?",
  "PERCEPTION = raw squares into meaning. The hard part of real robots.",
};

static const Rect R_NEXT = {6,   (int16_t)(BOTBAR_Y + 2), 150, 26};
static const Rect R_VIEW = {162, (int16_t)(BOTBAR_Y + 2), 96,  26};
static const Rect R_BACK = {262, (int16_t)(BOTBAR_Y + 2), 52,  26};

void PerceptionLessonScreen::begin() { _phase = 0; _view = 0; }
void PerceptionLessonScreen::enter() { draw(); }

static void cell(LGFX& g, int x, int y, bool wall) {
  g.fillRect(x, y, 26, 26, wall ? C_WALL : C_FLOOR);
  g.drawRect(x, y, 26, 26, C_PANEL_HI);
}

void PerceptionLessonScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Perception", ui::rgb(120, 230, 245), textdatum_t::top_left, 2);
  label(g, 6, TOPBAR_H + 4, STATUS[_phase], C_INK);

  const View& v = VIEWS[_view];

  // The robot's-eye view: a plus of raw squares (ahead above, left/right beside, robot centre).
  int cx = 56, cy = 118;
  cell(g, cx - 13, cy - 13 - 30, v.a);          // ahead (above)
  cell(g, cx - 13 - 30, cy - 13, v.l);          // left
  cell(g, cx - 13 + 30, cy - 13, v.r);          // right
  assets::drawCharacter(g, cx, cy, 24, 0, gb::NORTH);
  label(g, cx, cy - 13 - 30 - 10, "ahead", C_DIM, textdatum_t::bottom_center);
  label(g, 6, cy + 24, "raw squares it can see", C_DIM);

  if (_phase >= 1) {
    // The perception net: 3 raw inputs -> 1 output ("wall ahead?"). Only the AHEAD square
    // drives the answer here, so its wire is the bright one.
    int ix = 150, ox = 252;
    int iy[3] = {cy - 34, cy, cy + 34};        // ahead, left, right inputs
    bool raw[3] = {v.a, v.l, v.r};
    for (int i = 0; i < 3; i++) {
      uint16_t wire = (i == 0) ? (v.a ? C_GO : C_PANEL_HI) : C_PANEL;
      g.drawLine(ix, iy[i], ox, cy, wire);
    }
    for (int i = 0; i < 3; i++) g.fillCircle(ix, iy[i], 5, raw[i] ? C_WALL : C_FLOOR);
    label(g, ix - 8, iy[0] - 3, "ahead", C_DIM, textdatum_t::top_right);
    label(g, ix - 8, iy[1] - 3, "left",  C_DIM, textdatum_t::top_right);
    label(g, ix - 8, iy[2] - 3, "right", C_DIM, textdatum_t::top_right);
    g.fillCircle(ox, cy, 8, v.a ? C_GO : C_PANEL_HI);
    label(g, ox + 12, cy - 8, "wall ahead?", C_INK);
    label(g, ox + 12, cy + 4, v.a ? "YES" : "no", v.a ? C_GO : C_DIM);
  }

  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  const char* nx = _phase == 0 ? "See how it sees >" : _phase == 1 ? "Why it matters >" : "Start over";
  button(g, R_NEXT, nx, C_GO, C_PANEL);
  if (_phase == 1) button(g, R_VIEW, "Next view", C_ACCENT, C_PANEL);
  button(g, R_BACK, "Back", C_INK, C_PANEL);
}

app::Signal PerceptionLessonScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  if (R_NEXT.contains(tx, ty)) {
    _phase = (_phase + 1) % 3;
    hal::audio.blip(); draw();
  } else if (_phase == 1 && R_VIEW.contains(tx, ty)) {
    _view = (_view + 1) % NUM_VIEWS;
    hal::audio.blip(); draw();
  } else if (R_BACK.contains(tx, ty)) {
    return app::Signal::BACK;
  }
  return app::Signal::NONE;
}

}  // namespace screens
