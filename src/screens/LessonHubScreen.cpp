#include "screens/LessonHubScreen.h"
#include "hal/Audio.h"

using namespace ui;

namespace screens {

struct LessonInfo { const char* title; const char* sub; uint16_t col; };
static const int N_LESSONS = 6;
static const LessonInfo LESSONS[N_LESSONS] = {
  {"1. One neuron", "backprop: learn from a teacher", C_MOVE},
  {"2. Many actions", "go / turn / jump (multi-class)", C_TURN},
  {"3. Hidden layer", "what one neuron can't (XOR)", C_LOOP},
  {"4. Q-learning", "reinforcement: learn from reward", C_SENSE},
  {"5. Evolution", "breed the best, no teacher", C_FUNC},
  {"6. Transfer", "reuse skills on a new maze", ui::rgb(120, 230, 245)},
};

static Rect rowRect(int i) { return {10, (int16_t)(42 + i * 27), 300, 25}; }
static const Rect R_BACK = {6, (int16_t)(BOTBAR_Y + 2), 120, 26};

void LessonHubScreen::enter() { _pick = -1; draw(); }

void LessonHubScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "NeuroLab", C_ACCENT, textdatum_t::top_left, 2);
  label(g, SCREEN_W / 2, 30, "how do machines learn?", C_DIM, textdatum_t::top_center);
  for (int i = 0; i < N_LESSONS; i++) {
    Rect r = rowRect(i);
    g.fillRoundRect(r.x, r.y, r.w, r.h, 5, C_PANEL);
    g.drawRoundRect(r.x, r.y, r.w, r.h, 5, LESSONS[i].col);
    label(g, r.x + 10, r.y + 3, LESSONS[i].title, LESSONS[i].col);
    label(g, r.x + 10, r.y + 16, LESSONS[i].sub, C_DIM);  // full-width 2nd line (no overflow)
  }
  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
}

app::Signal LessonHubScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  for (int i = 0; i < N_LESSONS; i++) {
    if (rowRect(i).contains(tx, ty)) { _pick = i; hal::audio.blip(); return app::Signal::PLAY; }
  }
  if (R_BACK.contains(tx, ty)) return app::Signal::BACK;
  return app::Signal::NONE;
}

}  // namespace screens
