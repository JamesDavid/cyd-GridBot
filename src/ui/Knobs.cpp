#include "ui/Knobs.h"
#include "hal/Display.h"
#include "hal/Audio.h"
#include <cstdio>

namespace ui {

// Three stepper rows (- value +) + Done. y per row; the - / + buttons sit at the edges.
static constexpr int KY0 = 44, KY1 = 96, KY2 = 148;
static const Rect R_LR_DN  = {196, KY0, 30, 26}, R_LR_UP  = {284, KY0, 30, 26};
static const Rect R_RND_DN = {196, KY1, 30, 26}, R_RND_UP = {284, KY1, 30, 26};
static const Rect R_EXP_DN = {196, KY2, 30, 26}, R_EXP_UP = {284, KY2, 30, 26};
static const Rect R_DONE   = {120, (int16_t)(BOTBAR_Y + 2), 80, 32};

void Knobs::draw() const {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Training knobs", C_ACCENT, textdatum_t::top_left, 2);
  label(g, SCREEN_W - 6 - SOUND_ICON_W, 6, "(advanced)", C_DIM, textdatum_t::top_right);

  auto row = [&](int y, const char* name, const char* val, const char* hint,
                 const Rect& dn, const Rect& up, uint16_t vcol) {
    label(g, 12, y + 2, name, C_INK);
    button(g, dn, "-", C_INK, C_PANEL_HI);
    button(g, up, "+", C_INK, C_PANEL_HI);
    label(g, (dn.x + up.x + up.w) / 2, y + 6, val, vcol, textdatum_t::middle_center, 2);
    label(g, 12, y + 26, hint, C_DIM);
  };
  char v[16];
  snprintf(v, sizeof(v), "%.2f", lr);
  row(KY0, "Learning rate", v, "step size for Teach & Q-Learn", R_LR_DN, R_LR_UP,
      lr == 0.30f ? C_INK : C_MOVE);
  snprintf(v, sizeof(v), "%dx", rounds);
  row(KY1, "Rounds", v, "how long it trains (gens/epochs)", R_RND_DN, R_RND_UP,
      rounds == 1 ? C_INK : C_MOVE);
  snprintf(v, sizeof(v), "%.2f", explore);
  row(KY2, "Explore", v, "randomness: Evolve mutate + Q epsilon", R_EXP_DN, R_EXP_UP,
      explore == 1.0f ? C_INK : C_MOVE);

  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  label(g, 6, BOTBAR_Y - 12, "lower LR = slower; higher = jittery. defaults are tuned.", C_DIM);
  button(g, R_DONE, "Done", C_GO, C_PANEL);
}

bool Knobs::handleTap(int tx, int ty) {
  if      (R_LR_DN.contains(tx, ty))  lr = lr > 0.075f ? lr - 0.05f : 0.05f;
  else if (R_LR_UP.contains(tx, ty))  lr = lr < 0.875f ? lr + 0.05f : 0.90f;
  else if (R_RND_DN.contains(tx, ty)) rounds = rounds > 1 ? rounds - 1 : 1;
  else if (R_RND_UP.contains(tx, ty)) rounds = rounds < 4 ? rounds + 1 : 4;
  else if (R_EXP_DN.contains(tx, ty)) explore = explore > 0.125f ? explore - 0.25f : 0.0f;
  else if (R_EXP_UP.contains(tx, ty)) explore = explore < 1.875f ? explore + 0.25f : 2.0f;
  else if (R_DONE.contains(tx, ty))   { hal::audio.blip(); return true; }
  else return false;
  hal::audio.blip();
  return false;
}

}  // namespace ui
