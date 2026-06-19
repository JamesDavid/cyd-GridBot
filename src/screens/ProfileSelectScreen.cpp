#include "screens/ProfileSelectScreen.h"
#include "assets/Assets.h"

using namespace ui;

namespace screens {

// Up to 6 profile cards in a 3x2 grid, plus a "+ New" card in the next slot.
static constexpr int COLS = 3, CARD_W = 100, CARD_H = 78, GAP = 4;
static constexpr int GRID_X = (SCREEN_W - (COLS * CARD_W + (COLS - 1) * GAP)) / 2;
static constexpr int GRID_Y = BAND_Y + 6;

void ProfileSelectScreen::begin(store::ProfileStore* store) {
  _store = store;
  _metas.clear();
  if (_store) _store->listProfiles(_metas);
  _chosenId.clear();
}

ui::Rect ProfileSelectScreen::cardRect(int i) const {
  int r = i / COLS, c = i % COLS;
  return {(int16_t)(GRID_X + c * (CARD_W + GAP)),
          (int16_t)(GRID_Y + r * (CARD_H + GAP)),
          CARD_W, CARD_H};
}

void ProfileSelectScreen::enter() { draw(); }

void ProfileSelectScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, SCREEN_W / 2, 4, "GridBot - Choose a Player", C_ACCENT,
        textdatum_t::top_center);

  int n = (int)_metas.size();
  if (n > 6) n = 6;
  for (int i = 0; i < n; i++) {
    Rect r = cardRect(i);
    panel(g, r, C_PANEL);
    g.drawRoundRect(r.x, r.y, r.w, r.h, 6, C_PANEL_HI);
    assets::drawCharacter(g, r.cx(), r.y + 26, 40, _metas[i].avatar, gb::SOUTH);
    label(g, r.cx(), r.y + 46, _metas[i].name.c_str(), C_INK, textdatum_t::top_center);
    char lv[16]; snprintf(lv, sizeof(lv), "Lv %u", (unsigned)_metas[i].level);
    label(g, r.cx(), r.y + 58, lv, C_DIM, textdatum_t::top_center);
    // lower "stats" strip hint
    g.drawFastHLine(r.x + 6, r.y + r.h - 16, r.w - 12, C_PANEL_HI);
    label(g, r.cx(), r.y + r.h - 14, "stats", C_DIM, textdatum_t::top_center);
  }
  // New Player card
  Rect nr = cardRect(n);
  g.fillRoundRect(nr.x, nr.y, nr.w, nr.h, 6, C_PANEL);
  g.drawRoundRect(nr.x, nr.y, nr.w, nr.h, 6, C_GO);
  label(g, nr.cx(), nr.cy() - 10, "+", C_GO, textdatum_t::middle_center, 3);
  label(g, nr.cx(), nr.y + nr.h - 18, "New Player", C_GO, textdatum_t::top_center);

  label(g, SCREEN_W / 2, SCREEN_H - 12, "tap to play - hold to delete", C_DIM,
        textdatum_t::middle_center);
}

app::Signal ProfileSelectScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  int n = (int)_metas.size(); if (n > 6) n = 6;

  if (_tap.tapped(tp, now, tx, ty)) {
    _pressActive = true; _longFired = false; _pressIdx = -1;
    for (int i = 0; i <= n; i++) {  // include the New card at index n
      if (cardRect(i).contains(tx, ty)) { _pressIdx = i; break; }
    }
  }

  if (_pressActive) {
    // long-press to delete an existing card
    if (_pressIdx >= 0 && _pressIdx < n && !_longFired && _tap.heldMs(now) > 700) {
      _longFired = true;
      if (_store) { _store->remove(_metas[_pressIdx].id); _store->listProfiles(_metas); }
      draw();
    }
    if (!_tap.held()) {  // released
      _pressActive = false;
      if (!_longFired && _pressIdx >= 0) {
        if (_pressIdx == n) return app::Signal::NEW_PROFILE;
        // lower strip -> stats, else play
        Rect r = cardRect(_pressIdx);
        if (_tap.y() >= r.y + r.h - 16) {
          _chosenId = _metas[_pressIdx].id;
          return app::Signal::GOTO_STATS;
        }
        _chosenId = _metas[_pressIdx].id;
        return app::Signal::PLAY;
      }
    }
  }
  return app::Signal::NONE;
}

}  // namespace screens
