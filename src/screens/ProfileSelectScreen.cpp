#include "screens/ProfileSelectScreen.h"
#include "assets/Assets.h"
#include "hal/Audio.h"
#include "hal/Touch.h"

using namespace ui;

namespace screens {

static const Rect R_SOUND = {250, 1, 68, 20};  // music on/off toggle
static const Rect R_CAL   = {6, (int16_t)(SCREEN_H - 22), 96, 20};  // re-run touch calibration

// Up to 6 profile cards in a 3x2 grid, plus a "+ New" card in the next slot.
static constexpr int COLS = 3, CARD_W = 102, CARD_H = 84, GAP = 4;
static constexpr int GRID_X = (SCREEN_W - (COLS * CARD_W + (COLS - 1) * GAP)) / 2;
static constexpr int GRID_Y = BAND_Y + 4;
static constexpr int STRIP_H = 26;  // big, easy-to-hit stats/edit button

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
  label(g, SCREEN_W / 2, 3, "Choose a Player", C_ACCENT,
        textdatum_t::top_center, 2);
  bool snd = hal::audio.enabled();
  button(g, R_SOUND, snd ? "Music:on" : "Music:off", snd ? C_GO : C_DIM, C_PANEL);

  int n = (int)_metas.size();
  if (n > 6) n = 6;
  for (int i = 0; i < n; i++) {
    Rect r = cardRect(i);
    panel(g, r, C_PANEL);
    g.drawRoundRect(r.x, r.y, r.w, r.h, 6, C_PANEL_HI);
    assets::drawCharacter(g, r.cx(), r.y + 22, 36, _metas[i].avatar, gb::SOUTH);
    label(g, r.cx(), r.y + 40, _metas[i].name.c_str(), C_INK, textdatum_t::top_center);
    char lv[16]; snprintf(lv, sizeof(lv), "Lv %u", (unsigned)_metas[i].level);
    label(g, r.cx(), r.y + 50, lv, C_DIM, textdatum_t::top_center);
    // big stats/edit button across the bottom of the card
    Rect strip = {r.x, (int16_t)(r.y + r.h - STRIP_H), r.w, STRIP_H};
    g.fillRoundRect(strip.x + 2, strip.y, strip.w - 4, strip.h - 2, 5, C_PANEL_HI);
    label(g, strip.cx(), strip.cy(), "STATS / EDIT", C_ACCENT, textdatum_t::middle_center);
  }
  // New Player card
  Rect nr = cardRect(n);
  g.fillRoundRect(nr.x, nr.y, nr.w, nr.h, 6, C_PANEL);
  g.drawRoundRect(nr.x, nr.y, nr.w, nr.h, 6, C_GO);
  label(g, nr.cx(), nr.cy() - 10, "+", C_GO, textdatum_t::middle_center, 3);
  label(g, nr.cx(), nr.y + nr.h - 18, "New Player", C_GO, textdatum_t::top_center);

  button(g, R_CAL, "Calibrate", C_DIM, C_PANEL);
  label(g, SCREEN_W - 6, SCREEN_H - 12, "tap a player to play", C_DIM, textdatum_t::middle_right);
}

app::Signal ProfileSelectScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  int n = (int)_metas.size(); if (n > 6) n = 6;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;

  if (R_CAL.contains(tx, ty)) {         // re-run the inset touch calibration (modal, blocking)
    hal::touch.recalibrate();
    draw();
    return app::Signal::NONE;
  }
  if (R_SOUND.contains(tx, ty)) {       // toggle music/sound
    bool on = !hal::audio.enabled();
    hal::audio.setEnabled(on);
    if (on) hal::audio.startMusic(hal::kTitleMusic, hal::kTitleMusicLen, true);
    else hal::audio.stopMusic();
    draw();
    return app::Signal::NONE;
  }

  for (int i = 0; i <= n; i++) {        // include the New card at index n
    Rect r = cardRect(i);
    if (!r.contains(tx, ty)) continue;
    if (i == n) return app::Signal::NEW_PROFILE;
    _chosenId = _metas[i].id;
    // lower strip -> stats/edit (easy); the rest of the card -> play.
    // Deleting is intentionally NOT here — it lives behind confirmations in Stats.
    if (ty >= r.y + r.h - 17) return app::Signal::GOTO_STATS;
    return app::Signal::PLAY;
  }
  return app::Signal::NONE;
}

}  // namespace screens
