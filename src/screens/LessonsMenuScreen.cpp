#include "screens/LessonsMenuScreen.h"
#include "hal/Audio.h"

using namespace ui;

namespace screens {

static const Rect R_CODE  = {20, 56, 280, 56};
static const Rect R_NEURO = {20, 124, 280, 56};
static const Rect R_BACK  = {6, (int16_t)(BOTBAR_Y + 2), 120, 26};

void LessonsMenuScreen::enter() { _pick = -1; draw(); }

void LessonsMenuScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Lessons", C_ACCENT, textdatum_t::top_left, 2);
  label(g, SCREEN_W / 2, 32, "two ways to learn", C_DIM, textdatum_t::top_center);

  g.fillRoundRect(R_CODE.x, R_CODE.y, R_CODE.w, R_CODE.h, 8, C_PANEL);
  g.drawRoundRect(R_CODE.x, R_CODE.y, R_CODE.w, R_CODE.h, 8, C_MOVE);
  label(g, R_CODE.cx(), R_CODE.y + 14, "CodeLab", C_MOVE, textdatum_t::middle_center, 2);
  label(g, R_CODE.cx(), R_CODE.y + 38, "the programming blocks (you write the rules)", C_DIM, textdatum_t::middle_center);

  g.fillRoundRect(R_NEURO.x, R_NEURO.y, R_NEURO.w, R_NEURO.h, 8, C_PANEL);
  g.drawRoundRect(R_NEURO.x, R_NEURO.y, R_NEURO.w, R_NEURO.h, 8, ui::rgb(120, 230, 245));
  label(g, R_NEURO.cx(), R_NEURO.y + 14, "NeuroLab", ui::rgb(120, 230, 245), textdatum_t::middle_center, 2);
  label(g, R_NEURO.cx(), R_NEURO.y + 38, "machine learning (you train the rules)", C_DIM, textdatum_t::middle_center);

  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
}

app::Signal LessonsMenuScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  if (R_CODE.contains(tx, ty)) { _pick = 0; hal::audio.blip(); return app::Signal::PLAY; }
  if (R_NEURO.contains(tx, ty)) { _pick = 1; hal::audio.blip(); return app::Signal::PLAY; }
  if (R_BACK.contains(tx, ty)) return app::Signal::BACK;
  return app::Signal::NONE;
}

}  // namespace screens
