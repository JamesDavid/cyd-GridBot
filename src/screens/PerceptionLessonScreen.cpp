#include "screens/PerceptionLessonScreen.h"
#include "hal/Audio.h"
#include "assets/Assets.h"
#include "game/Types.h"

using namespace ui;

namespace screens {

// Tile types around the robot: 0 floor, 1 wall, 2 pit, 3 goal.
struct Scene {
  uint8_t ahead, left, right;   // the three tiles the robot is up against
  float goalA, goalR;           // goal bearing (ahead/behind, left/right), -1..1
  const char* desc;
};
static const Scene SCENES[5] = {
  {0, 1, 1,  0.6f,  0.0f, "a corridor"},
  {1, 0, 1,  0.3f,  0.5f, "wall ahead -> turn"},
  {2, 0, 0,  0.7f, -0.2f, "pit ahead -> jump"},
  {3, 1, 0,  1.0f,  0.0f, "goal ahead!"},
  {0, 0, 0,  0.2f,  0.8f, "open junction"},
};
static const int NUM_SCENES = 5;

static const char* STATUS[3] = {
  "The tiles around me become the brain's senses.",
  "We just HAND these. A real robot must perceive them.",
  "Only my immediate surroundings -- not the whole map.",
};

static const Rect R_NEXT = {6,   (int16_t)(BOTBAR_Y + 2), 150, 26};
static const Rect R_VIEW = {162, (int16_t)(BOTBAR_Y + 2), 96,  26};
static const Rect R_BACK = {262, (int16_t)(BOTBAR_Y + 2), 52,  26};

void PerceptionLessonScreen::begin() { _phase = 0; _scene = 0; }
void PerceptionLessonScreen::enter() { draw(); }

static void drawTile(LGFX& g, int x, int y, int s, uint8_t t) {
  uint16_t col = (t == 1) ? C_WALL : (t == 2) ? ui::rgb(12, 12, 18) : C_FLOOR;
  g.fillRect(x, y, s - 1, s - 1, col);
  g.drawRect(x, y, s - 1, s - 1, C_PANEL_HI);
  if (t == 3) assets::drawGoalToken(g, x + s / 2, y + s / 2, s, 0);
}

// One sense row: name, a lit node, and the value -- this list IS the brain's input layer.
static void senseRow(LGFX& g, int x, int y, const char* name, float v, bool signedVal) {
  label(g, x, y, name, C_DIM);
  float mag = v < 0 ? -v : v;
  uint16_t col = mag < 0.05f ? C_PANEL_HI : (signedVal ? C_MOVE : C_GO);
  g.fillCircle(x + 96, y + 4, 5, col);
  char val[8];
  if (signedVal) snprintf(val, sizeof(val), "%+.1f", v);
  else           snprintf(val, sizeof(val), "%d", (int)(v + 0.5f));
  label(g, x + 108, y, val, mag < 0.05f ? C_DIM : C_INK);
}

void PerceptionLessonScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Perception", ui::rgb(120, 230, 245), textdatum_t::top_left, 2);
  label(g, 6, TOPBAR_H + 4, STATUS[_phase], C_INK);

  const Scene& sc = SCENES[_scene];
  float wallA = (sc.ahead == 1), pitA = (sc.ahead == 2);
  float wallL = (sc.left == 1), wallR = (sc.right == 1);

  // ---- left: the robot's immediate surroundings (real tiles, robot facing up) ----
  const int s = 26, cx = 56, cy = 96;
  drawTile(g, cx - s / 2, cy - s / 2 - s, s, sc.ahead);   // ahead (above)
  drawTile(g, cx - s / 2 - s, cy - s / 2, s, sc.left);    // left
  drawTile(g, cx - s / 2 + s, cy - s / 2, s, sc.right);   // right
  assets::drawCharacter(g, cx, cy, s - 2, 0, gb::NORTH);
  label(g, cx, cy - s / 2 - s - 10, "ahead", C_DIM, textdatum_t::bottom_center);
  label(g, cx, cy + s, sc.desc, ui::rgb(120, 230, 245), textdatum_t::top_center);

  // ---- right: those tiles turned into numbers = the brain's input layer ----
  int lx = 150, ly = 52;
  label(g, lx, ly - 13, "robot brain - input layer", ui::rgb(120, 230, 245));
  senseRow(g, lx, ly,      "wall ahead", wallA, false);
  senseRow(g, lx, ly + 15, "wall left",  wallL, false);
  senseRow(g, lx, ly + 30, "wall right", wallR, false);
  senseRow(g, lx, ly + 45, "pit ahead",  pitA,  false);
  senseRow(g, lx, ly + 60, "goal ahead", sc.goalA, true);
  senseRow(g, lx, ly + 75, "goal right", sc.goalR, true);
  label(g, lx, ly + 92, "(+ enemy bearing, in battles)", C_DIM);

  // ---- phase-specific teaching note ----
  g.fillRect(0, 150, SCREEN_W, BOTBAR_Y - 150, C_BG);
  if (_phase == 0) {
    label(g, 8, 160, "These 0/1 numbers ARE the brain's input layer --", C_DIM);
    label(g, 8, 172, "the tiles around me, turned into senses it can use.", C_DIM);
  } else if (_phase == 1) {
    label(g, 8, 158, "Here we just HAND the brain these numbers.", C_INK);
    label(g, 8, 170, "A real robot needs a camera + sensors to work them", C_DIM);
    label(g, 8, 182, "out -- that's perception, the hard part. WE pick which", C_DIM);
    label(g, 8, 194, "things to sense (it's an engineering choice).", C_DIM);
  } else {
    // local-vs-global: a dim top-down map with a "?" -- the robot can't see it
    int mx = 16, my = 150, ms = 8;
    for (int r = 0; r < 4; r++)
      for (int c = 0; c < 6; c++) {
        bool wall = ((r * 7 + c * 3) % 5) == 0;
        g.fillRect(mx + c * ms, my + r * ms, ms - 1, ms - 1, wall ? C_WALL : C_FLOOR);
      }
    g.drawRect(mx - 1, my - 1, 6 * ms + 1, 4 * ms + 1, C_BAD);
    label(g, mx + 6 * ms + 8, my + 4, "the whole map?", C_BAD);
    label(g, mx + 6 * ms + 8, my + 16, "I can't see it.", C_DIM);
    label(g, 8, my + 38, "I only sense what's right next to me.", C_DIM);
  }

  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  const char* nx = _phase == 0 ? "Where from? >" : _phase == 1 ? "Just local >" : "Start over";
  button(g, R_NEXT, nx, C_GO, C_PANEL);
  button(g, R_VIEW, "Next view", C_ACCENT, C_PANEL);
  button(g, R_BACK, "Back", C_INK, C_PANEL);
}

app::Signal PerceptionLessonScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  if (R_NEXT.contains(tx, ty)) {
    _phase = (_phase + 1) % 3;
    hal::audio.blip(); draw();
  } else if (R_VIEW.contains(tx, ty)) {
    _scene = (_scene + 1) % NUM_SCENES;
    hal::audio.blip(); draw();
  } else if (R_BACK.contains(tx, ty)) {
    return app::Signal::BACK;
  }
  return app::Signal::NONE;
}

}  // namespace screens
