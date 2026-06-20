#include "screens/BadgesScreen.h"
#include "game/Achievements.h"

using namespace ui;
using namespace gb;

namespace screens {

static const Rect R_BACK = {6, (int16_t)(BOTBAR_Y + 2), 120, 26};

// One-line "what you did to earn it" hints, indexed like Achievement bits.
static const char* HINT[ACH_COUNT] = {
  "win a level", "earn 3 stars", "use a Jump", "use a Repeat", "use a Function",
  "clear a sense level", "win an arena", "5-win streak", "10-win streak",
  "reach level 10", "reach level 20", "draw a sprite", "earn 50 stars"};

void BadgesScreen::enter() { draw(); }

void BadgesScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  uint32_t mask = _p ? _p->achievements : 0;
  char hdr[24];
  snprintf(hdr, sizeof(hdr), "Badges %d/%d", achievementCount(mask), ACH_COUNT);
  label(g, 6, 3, hdr, C_ACCENT, textdatum_t::top_left, 2);

  for (int i = 0; i < ACH_COUNT; i++) {
    bool got = mask & (1u << i);
    int col = i / 7, row = i % 7;
    int x = 6 + col * 156, y = BAND_Y + 2 + row * 26;
    g.fillRoundRect(x, y, 152, 24, 4, got ? C_PANEL_HI : C_PANEL);
    // medal: filled gold star if earned, dim outline if not
    int mx = x + 12, my = y + 12;
    if (got) {
      g.fillCircle(mx, my, 7, C_ACCENT);
      g.fillCircle(mx, my, 4, C_GO);
    } else {
      g.drawCircle(mx, my, 7, C_LOCK);
    }
    label(g, x + 26, y + 2, achievementName(i), got ? C_INK : C_DIM);
    label(g, x + 26, y + 13, got ? "earned!" : HINT[i], got ? C_GO : C_LOCK);
  }
  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
}

app::Signal BadgesScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (_tap.tapped(tp, now, tx, ty) && R_BACK.contains(tx, ty))
    return app::Signal::BACK;
  return app::Signal::NONE;
}

}  // namespace screens
