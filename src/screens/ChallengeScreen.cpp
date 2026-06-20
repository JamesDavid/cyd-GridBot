#include "screens/ChallengeScreen.h"
#include "hal/Audio.h"
#include <Arduino.h>  // millis() for the "Surprise" code

using namespace ui;

namespace screens {

static const Rect R_M10 = {20, 112, 56, 36};
static const Rect R_M1  = {84, 112, 56, 36};
static const Rect R_P1  = {180, 112, 56, 36};
static const Rect R_P10 = {244, 112, 56, 36};
static const Rect R_SURP = {20, 158, 130, 34};
static const Rect R_PLAY = {170, 158, 130, 34};
static const Rect R_BACK = {6, (int16_t)(BOTBAR_Y + 2), 120, 26};

void ChallengeScreen::begin(gb::Profile* profile) {
  _profile = profile;
  if (_code == 0) _code = 1;  // keep the last-picked code across visits
}

void ChallengeScreen::enter() { draw(); }

void ChallengeScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Seed Challenge", C_ACCENT, textdatum_t::top_left, 2);
  label(g, SCREEN_W / 2, 30, "Same code = same maze. Race a friend!", C_DIM,
        textdatum_t::top_center);

  // the big 4-digit code
  char code[8]; snprintf(code, sizeof(code), "%04u", (unsigned)(_code % 10000));
  label(g, SCREEN_W / 2, 78, code, C_INK, textdatum_t::middle_center, 4);

  button(g, R_M10, "-10", C_MOVE, C_PANEL);
  button(g, R_M1, "-1", C_MOVE, C_PANEL);
  button(g, R_P1, "+1", C_GO, C_PANEL);
  button(g, R_P10, "+10", C_GO, C_PANEL);
  button(g, R_SURP, "Surprise", C_FUNC, C_PANEL);
  button(g, R_PLAY, "Play >", C_GO, C_PANEL);

  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
}

app::Signal ChallengeScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;

  auto bump = [&](int d) { _code = (_code + 10000 + d) % 10000; hal::audio.blip(); draw(); };
  if (R_M10.contains(tx, ty)) { bump(-10); return app::Signal::NONE; }
  if (R_M1.contains(tx, ty))  { bump(-1);  return app::Signal::NONE; }
  if (R_P1.contains(tx, ty))  { bump(1);   return app::Signal::NONE; }
  if (R_P10.contains(tx, ty)) { bump(10);  return app::Signal::NONE; }
  if (R_SURP.contains(tx, ty)) {
    _code = (now ^ (now >> 7) ^ millis()) % 10000;  // no RNG on device; mix the clock
    hal::audio.blip(); draw(); return app::Signal::NONE;
  }
  if (R_PLAY.contains(tx, ty)) { hal::audio.blip(); return app::Signal::PLAY; }
  if (R_BACK.contains(tx, ty)) return app::Signal::BACK;
  return app::Signal::NONE;
}

}  // namespace screens
