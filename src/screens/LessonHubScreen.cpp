#include "screens/LessonHubScreen.h"
#include "hal/Audio.h"

using namespace ui;

namespace screens {

struct LessonInfo { const char* title; const char* sub; uint16_t col; };
static const LessonInfo LESSONS[3] = {
  {"1. One neuron", "learn from a teacher (backprop)", C_MOVE},
  {"2. Q-learning", "learn from reward (no teacher)", C_SENSE},
  {"3. Evolution", "breed the best, no teacher", C_FUNC},
};

static Rect rowRect(int i) { return {10, (int16_t)(50 + i * 46), 300, 40}; }
static const Rect R_BACK = {6, (int16_t)(BOTBAR_Y + 2), 120, 26};

void LessonHubScreen::enter() { _pick = -1; draw(); }

void LessonHubScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "NeuroLab", C_ACCENT, textdatum_t::top_left, 2);
  label(g, SCREEN_W / 2, 32, "how do machines learn?", C_DIM, textdatum_t::top_center);
  for (int i = 0; i < 3; i++) {
    Rect r = rowRect(i);
    g.fillRoundRect(r.x, r.y, r.w, r.h, 6, C_PANEL);
    g.drawRoundRect(r.x, r.y, r.w, r.h, 6, LESSONS[i].col);
    label(g, r.x + 12, r.y + 6, LESSONS[i].title, LESSONS[i].col);
    label(g, r.x + 12, r.y + 22, LESSONS[i].sub, C_DIM);
  }
  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
}

app::Signal LessonHubScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  for (int i = 0; i < 3; i++) {
    if (rowRect(i).contains(tx, ty)) { _pick = i; hal::audio.blip(); return app::Signal::PLAY; }
  }
  if (R_BACK.contains(tx, ty)) return app::Signal::BACK;
  return app::Signal::NONE;
}

}  // namespace screens
