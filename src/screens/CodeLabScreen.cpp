#include "screens/CodeLabScreen.h"
#include "hal/Audio.h"

using namespace ui;

namespace screens {

struct Item { const char* title; const char* sub; uint16_t col; };
static const int N = 5;
// In unlock order: Move (start), Jump (L6), Repeat (L10), Sense (L15), Functions (L20).
static const Item ITEMS[N] = {
  {"1. Move", "forward & turn", C_MOVE},
  {"2. Jump", "leap over a pit", C_GO},
  {"3. Repeat", "do steps again and again", C_LOOP},
  {"4. Sense", "react with IF", C_SENSE},
  {"5. Functions", "name steps, call them", C_FUNC},
};

static Rect rowRect(int i) { return {10, (int16_t)(44 + i * 32), 300, 30}; }
static const Rect R_BACK = {6, (int16_t)(BOTBAR_Y + 2), 120, 26};

void CodeLabScreen::enter() { _pick = -1; draw(); }

void CodeLabScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "CodeLab", C_ACCENT, textdatum_t::top_left, 2);
  label(g, SCREEN_W / 2, 30, "learn the programming blocks", C_DIM, textdatum_t::top_center);
  for (int i = 0; i < N; i++) {
    Rect r = rowRect(i);
    g.fillRoundRect(r.x, r.y, r.w, r.h, 5, C_PANEL);
    g.drawRoundRect(r.x, r.y, r.w, r.h, 5, ITEMS[i].col);
    label(g, r.x + 10, r.y + 3, ITEMS[i].title, ITEMS[i].col);
    label(g, r.x + 10, r.y + 16, ITEMS[i].sub, C_DIM);  // full-width 2nd line (no overflow)
  }
  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
}

app::Signal CodeLabScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  for (int i = 0; i < N; i++)
    if (rowRect(i).contains(tx, ty)) { _pick = i; hal::audio.blip(); return app::Signal::PLAY; }
  if (R_BACK.contains(tx, ty)) return app::Signal::BACK;
  return app::Signal::NONE;
}

}  // namespace screens
