#include "screens/LevelSelectScreen.h"
#include "hal/Display.h"
#include "hal/Audio.h"

using namespace ui;
using namespace gb;

namespace screens {

// 4x4 grid of level tiles per page.
static const int COLS = 4, ROWS = 4, PER = COLS * ROWS;
static const int GTOP = 30, GLEFT = 8, GW = 304, GAP = 6, TH = 42;
static int tileW() { return (GW - (COLS - 1) * GAP) / COLS; }
static Rect tileRect(int slot) {
  int r = slot / COLS, c = slot % COLS, w = tileW();
  return {(int16_t)(GLEFT + c * (w + GAP)), (int16_t)(GTOP + r * (TH + GAP)), (int16_t)w, (int16_t)TH};
}
static const Rect R_BACK = {6,   (int16_t)(BOTBAR_Y + 2), 96, 26};
static const Rect R_PREV = {150, (int16_t)(BOTBAR_Y + 2), 78, 26};
static const Rect R_NEXT = {234, (int16_t)(BOTBAR_Y + 2), 80, 26};

void LevelSelectScreen::begin(Profile* profile) {
  _p = profile;
  _picked = 0;
  uint32_t cur = profile ? profile->level : 1;
  _page = cur > 0 ? (int)((cur - 1) / PER) : 0;
}

void LevelSelectScreen::enter() { draw(); }

int LevelSelectScreen::pages() const {
  uint32_t hi = _p ? _p->level : 1;             // levels 1..current are reachable
  int pg = (int)((hi + PER - 1) / PER);
  return pg < 1 ? 1 : pg;
}

void LevelSelectScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Levels", C_ACCENT, textdatum_t::top_left, 2);
  label(g, SCREEN_W - 6, 6, "go for 3 stars", C_DIM, textdatum_t::top_right);

  uint32_t cur = _p ? _p->level : 1;
  for (int s = 0; s < PER; s++) {
    uint32_t lvl = (uint32_t)_page * PER + s + 1;
    Rect t = tileRect(s);
    bool locked = lvl > cur;
    bool current = lvl == cur;
    g.fillRoundRect(t.x, t.y, t.w, t.h, 6, locked ? ui::rgb(20, 28, 42) : C_PANEL);
    g.drawRoundRect(t.x, t.y, t.w, t.h, 6, current ? C_ACCENT : ui::rgb(44, 60, 90));

    char nb[8]; snprintf(nb, sizeof(nb), "%u", (unsigned)lvl);
    label(g, t.x + t.w / 2, t.y + 4, nb, locked ? C_LOCK : C_INK, textdatum_t::top_center, 2);
    if (locked) {
      label(g, t.x + t.w / 2, t.y + t.h - 13, "locked", C_LOCK, textdatum_t::top_center);
      continue;
    }
    // star pips: gold-filled for earned, hollow for not
    const LevelRec* rec = _p ? _p->levelRec(lvl) : nullptr;
    int stars = rec ? rec->stars : 0;
    int py = t.y + 25;
    for (int i = 0; i < 3; i++) {
      int px = t.x + t.w / 2 + (i - 1) * 12;
      if (i < stars) g.fillCircle(px, py, 4, C_ACCENT);
      else           g.drawCircle(px, py, 4, C_LOCK);
    }
    // best blocks vs par (or a prompt if not yet beaten)
    if (rec && rec->bestBlocks > 0) {
      char bb[12];
      snprintf(bb, sizeof(bb), "%u/%u", (unsigned)rec->bestBlocks, (unsigned)(rec->par ? rec->par : rec->bestBlocks));
      label(g, t.x + t.w / 2, t.y + t.h - 13, bb, C_DIM, textdatum_t::top_center);
    } else {
      label(g, t.x + t.w / 2, t.y + t.h - 13, current ? "play >" : "new", current ? C_GO : C_DIM, textdatum_t::top_center);
    }
  }

  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
  char pg[12]; snprintf(pg, sizeof(pg), "%d/%d", _page + 1, pages());
  label(g, SCREEN_W / 2, BOTBAR_Y + 14, pg, C_DIM, textdatum_t::middle_center);
  if (_page > 0)            button(g, R_PREV, "< Prev", C_INK, C_PANEL);
  if (_page < pages() - 1)  button(g, R_NEXT, "Next >", C_INK, C_PANEL);
}

app::Signal LevelSelectScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  if (R_BACK.contains(tx, ty)) { hal::audio.blip(); return app::Signal::BACK; }
  if (_page > 0 && R_PREV.contains(tx, ty)) { hal::audio.blip(); _page--; draw(); return app::Signal::NONE; }
  if (_page < pages() - 1 && R_NEXT.contains(tx, ty)) { hal::audio.blip(); _page++; draw(); return app::Signal::NONE; }
  uint32_t cur = _p ? _p->level : 1;
  for (int s = 0; s < PER; s++) {
    uint32_t lvl = (uint32_t)_page * PER + s + 1;
    if (lvl > cur) continue;  // locked
    if (tileRect(s).contains(tx, ty)) { _picked = lvl; hal::audio.blip(); return app::Signal::GOTO_LEVEL; }
  }
  return app::Signal::NONE;
}

}  // namespace screens
