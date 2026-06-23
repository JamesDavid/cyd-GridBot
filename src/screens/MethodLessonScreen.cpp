#include "screens/MethodLessonScreen.h"
#include "hal/Audio.h"

using namespace ui;

namespace screens {

struct Q { const char* prompt; const char* opt[3]; int nOpt; int correct; const char* why; };
static const Q QUESTIONS[] = {
  {"A maze FULL of dead-ends. Which brain?",
   {"Plain brain", "Memory brain", nullptr}, 2, 1,
   "Memory recalls dead-ends; a plain brain loops."},
  {"A battle: zap the foe in front of you, NOW.",
   {"Plain brain", "Memory brain", nullptr}, 2, 0,
   "Only NOW matters - memory just adds noise."},
  {"You already KNOW the winning path. How to train?",
   {"Teach (show it)", "Evolve (random)", "Q-learn (reward)"}, 3, 0,
   "You have the answer - just show it, don't re-discover."},
  {"No teacher, no examples - just win or lose.",
   {"Teach (show it)", "Q-learn (reward)", "Draw a path"}, 3, 1,
   "Q-learning needs only win/lose - no answer key."},
};
static const int N_Q = sizeof(QUESTIONS) / sizeof(QUESTIONS[0]);

static Rect optRect(int i) { return {30, (int16_t)(86 + i * 34), 260, 30}; }
static const Rect R_NEXT = {200, (int16_t)(BOTBAR_Y + 2), 114, 26};
static const Rect R_BACK = {6,   (int16_t)(BOTBAR_Y + 2), 90, 26};

void MethodLessonScreen::begin() { _q = 0; _score = 0; _picked = -1; }
void MethodLessonScreen::enter() { draw(); }

void MethodLessonScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "The right tool", C_ACCENT, textdatum_t::top_left, 2);

  bool done = (_q >= N_Q);
  if (done) {
    char s[28]; snprintf(s, sizeof(s), "%d / %d right", _score, N_Q);
    label(g, SCREEN_W / 2, 70, s, C_ACCENT, textdatum_t::middle_center, 2);
    label(g, SCREEN_W / 2, 110, _score == N_Q ? "You match the method to the problem!" : "Good - try again to ace it",
          _score == N_Q ? C_GO : C_INK, textdatum_t::middle_center);
    label(g, SCREEN_W / 2, 134, "No method wins everything - pick the one that fits.",
          C_DIM, textdatum_t::middle_center);
    g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
    button(g, R_BACK, "< Back", C_INK, C_PANEL);
    button(g, R_NEXT, "Again", C_GO, C_PANEL);
    return;
  }

  const Q& q = QUESTIONS[_q];
  char hd[16]; snprintf(hd, sizeof(hd), "%d / %d", _q + 1, N_Q);
  label(g, SCREEN_W - 6, 6, hd, C_DIM, textdatum_t::top_right);
  label(g, SCREEN_W / 2, 50, q.prompt, C_INK, textdatum_t::middle_center);

  for (int i = 0; i < q.nOpt; i++) {
    Rect r = optRect(i);
    uint16_t col = C_PANEL_HI;
    if (_picked >= 0) {                         // after answering: green=correct, red=your wrong pick
      if (i == q.correct) col = C_GO;
      else if (i == _picked) col = C_BAD;
    }
    button(g, r, q.opt[i], (_picked >= 0 && (i == q.correct || i == _picked)) ? C_BG : C_INK, col);
  }
  if (_picked >= 0)
    label(g, SCREEN_W / 2, 198, q.why, _picked == q.correct ? C_GO : C_ACCENT, textdatum_t::middle_center);

  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
  if (_picked >= 0) button(g, R_NEXT, _q + 1 < N_Q ? "Next >" : "Finish", C_GO, C_PANEL);
}

app::Signal MethodLessonScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  if (R_BACK.contains(tx, ty)) return app::Signal::BACK;
  if (_q >= N_Q) {                              // results screen: "Again" restarts
    if (R_NEXT.contains(tx, ty)) { begin(); hal::audio.blip(); draw(); }
    return app::Signal::NONE;
  }
  if (_picked < 0) {                            // answering
    const Q& q = QUESTIONS[_q];
    for (int i = 0; i < q.nOpt; i++)
      if (optRect(i).contains(tx, ty)) {
        _picked = i;
        if (i == q.correct) { _score++; hal::audio.badge(); } else hal::audio.fail();
        draw(); return app::Signal::NONE;
      }
  } else if (R_NEXT.contains(tx, ty)) {         // advance after feedback
    _q++; _picked = -1; hal::audio.blip(); draw();
  }
  return app::Signal::NONE;
}

}  // namespace screens
