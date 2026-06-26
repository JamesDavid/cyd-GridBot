#include "screens/LessonHubScreen.h"
#include "hal/Audio.h"

using namespace ui;

namespace screens {

struct LessonInfo { const char* title; const char* sub; uint16_t col; };
static const int N_LESSONS = 17;
static const int PER_PAGE = 5;
static const int N_PAGES = (N_LESSONS + PER_PAGE - 1) / PER_PAGE;
static const LessonInfo LESSONS[N_LESSONS] = {
  {"1. One neuron", "watch it learn", C_MOVE},
  {"2. Backprop", "how it learns, step by step", C_MOVE},
  {"3. Perception", "the senses we hand the brain", C_MOVE},
  {"4. Hidden layer", "what one neuron can't", C_LOOP},
  {"5. Many actions", "go / turn / jump", C_TURN},
  {"6. Robot brain", "meet its senses", ui::rgb(120, 230, 245)},
  {"7. Data & labels", "tag examples, train to them", C_SENSE},
  {"8. Q-learning", "smarter: learn from reward", C_SENSE},
  {"9. Tuning the grid", "explore + step on a Q table", C_SENSE},
  {"10. Tuning a real net", "learn rate + rounds on a brain", C_SENSE},
  {"11. Evolution", "fittest breed: parents -> babies", C_FUNC},
  {"12. Transfer", "reuse skills, new maze", ui::rgb(120, 230, 245)},
  {"13. Brain Cam", "watch a brain think", ui::rgb(120, 230, 245)},
  {"14. Pilot", "plan + steer (like FSD)", C_ACCENT},
  {"15. Memory", "an RNN remembers", C_ACCENT},
  {"16. Self-play", "train by beating yourself", C_FUNC},
  {"17. Right tool", "match the method to the problem", C_TURN},
};

// Roomy rows now that there are only 5 per page.
static Rect rowRect(int i) { return {12, (int16_t)(46 + i * 30), 296, 28}; }
static const Rect R_BACK = {6,   (int16_t)(BOTBAR_Y + 2), 96, 26};
static const Rect R_PREV = {150, (int16_t)(BOTBAR_Y + 2), 78, 26};
static const Rect R_NEXT = {234, (int16_t)(BOTBAR_Y + 2), 80, 26};

void LessonHubScreen::enter() {
  _pick = -1;
  if (_page < 0 || _page >= N_PAGES) _page = 0;  // keep the page you were on (so exiting a lesson
  draw();                                        // returns you to the same page), just clamp it valid
}

void LessonHubScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "NeuroLab", C_ACCENT, textdatum_t::top_left, 2);
  char hdr[28]; snprintf(hdr, sizeof(hdr), "how machines learn  (%d/%d)", _page + 1, N_PAGES);
  label(g, SCREEN_W - 6 - SOUND_ICON_W, 6, hdr, C_DIM, textdatum_t::top_right);

  int start = _page * PER_PAGE;
  for (int r = 0; r < PER_PAGE; r++) {
    int idx = start + r;
    if (idx >= N_LESSONS) break;
    Rect rr = rowRect(r);
    g.fillRoundRect(rr.x, rr.y, rr.w, rr.h, 6, C_PANEL);
    g.drawRoundRect(rr.x, rr.y, rr.w, rr.h, 6, LESSONS[idx].col);
    label(g, rr.x + 12, rr.y + 4, LESSONS[idx].title, LESSONS[idx].col);
    label(g, rr.x + 12, rr.y + 15, LESSONS[idx].sub, C_DIM);
  }

  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
  button(g, R_PREV, "< Prev", _page > 0 ? C_ACCENT : C_LOCK, C_PANEL);
  button(g, R_NEXT, "Next >", _page < N_PAGES - 1 ? C_ACCENT : C_LOCK, C_PANEL);
}

app::Signal LessonHubScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  int start = _page * PER_PAGE;
  for (int r = 0; r < PER_PAGE; r++) {
    int idx = start + r;
    if (idx >= N_LESSONS) break;
    if (rowRect(r).contains(tx, ty)) { _pick = idx; hal::audio.blip(); return app::Signal::PLAY; }
  }
  if (R_PREV.contains(tx, ty)) {
    if (_page > 0) { _page--; hal::audio.blip(); draw(); }
  } else if (R_NEXT.contains(tx, ty)) {
    if (_page < N_PAGES - 1) { _page++; hal::audio.blip(); draw(); }
  } else if (R_BACK.contains(tx, ty)) {
    return app::Signal::BACK;
  }
  return app::Signal::NONE;
}

}  // namespace screens
