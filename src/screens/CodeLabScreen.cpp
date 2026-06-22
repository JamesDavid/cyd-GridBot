#include "screens/CodeLabScreen.h"
#include "hal/Audio.h"

using namespace ui;

namespace screens {

struct Item { const char* title; const char* sub; uint16_t col; };
static const int N = 7;
static const int PER_PAGE = 5;
static const int N_PAGES = (N + PER_PAGE - 1) / PER_PAGE;
// In unlock order: Move (start), Jump (L6), Repeat (L10), Sense (L15), Functions (L20),
// then the neurosymbolic Brain block (L28) and Debug-it (a skill, any time).
static const Item ITEMS[N] = {
  {"1. Move", "forward & turn", C_MOVE},
  {"2. Jump", "leap over a pit", C_GO},
  {"3. Repeat", "do steps again and again", C_LOOP},
  {"4. Sense", "react with IF / until", C_SENSE},
  {"5. Functions", "name steps, call them", C_FUNC},
  {"6. Brain", "code + a trained brain", ui::rgb(120, 230, 245)},
  {"7. Debug", "find the bug, fix one thing", C_BAD},
};

static Rect rowRect(int i) { return {10, (int16_t)(44 + i * 30), 300, 28}; }
static const Rect R_BACK = {6,   (int16_t)(BOTBAR_Y + 2), 96, 26};
static const Rect R_PREV = {150, (int16_t)(BOTBAR_Y + 2), 78, 26};
static const Rect R_NEXT = {234, (int16_t)(BOTBAR_Y + 2), 80, 26};

void CodeLabScreen::enter() { _pick = -1; _page = 0; draw(); }

void CodeLabScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "CodeLab", C_ACCENT, textdatum_t::top_left, 2);
  char hdr[28]; snprintf(hdr, sizeof(hdr), "the programming blocks  (%d/%d)", _page + 1, N_PAGES);
  label(g, SCREEN_W - 6, 6, hdr, C_DIM, textdatum_t::top_right);

  int start = _page * PER_PAGE;
  for (int r = 0; r < PER_PAGE; r++) {
    int idx = start + r;
    if (idx >= N) break;
    Rect rr = rowRect(r);
    g.fillRoundRect(rr.x, rr.y, rr.w, rr.h, 5, C_PANEL);
    g.drawRoundRect(rr.x, rr.y, rr.w, rr.h, 5, ITEMS[idx].col);
    label(g, rr.x + 10, rr.y + 3, ITEMS[idx].title, ITEMS[idx].col);
    label(g, rr.x + 10, rr.y + 15, ITEMS[idx].sub, C_DIM);
  }

  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
  button(g, R_PREV, "< Prev", _page > 0 ? C_ACCENT : C_LOCK, C_PANEL);
  button(g, R_NEXT, "Next >", _page < N_PAGES - 1 ? C_ACCENT : C_LOCK, C_PANEL);
}

app::Signal CodeLabScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  int start = _page * PER_PAGE;
  for (int r = 0; r < PER_PAGE; r++) {
    int idx = start + r;
    if (idx >= N) break;
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
