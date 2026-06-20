// GridBot — shared UI widgets tuned for big fingers on resistive touch (SPEC §10).
// Thin helpers over LovyanGFX; all targets are drawn >= ~40px by callers.
#pragma once
#include "hal/Display.h"
#include "ui/Theme.h"

namespace ui {

// A filled rounded button with a centred label. `pressed` darkens it for feedback.
inline void button(LGFX& g, const Rect& r, const char* label, uint16_t fg, uint16_t bg,
                   bool pressed = false, uint8_t textSize = 1) {
  uint16_t fill = pressed ? C_PANEL_HI : bg;
  g.fillRoundRect(r.x, r.y, r.w, r.h, 6, fill);
  g.drawRoundRect(r.x, r.y, r.w, r.h, 6, fg);
  if (label && *label) {
    g.setTextColor(fg, fill);
    g.setTextDatum(textdatum_t::middle_center);
    g.setTextSize(textSize);
    g.drawString(label, r.cx(), r.cy());
    g.setTextSize(1);
  }
}

inline void panel(LGFX& g, const Rect& r, uint16_t bg = C_PANEL) {
  g.fillRoundRect(r.x, r.y, r.w, r.h, 6, bg);
}

inline void label(LGFX& g, int x, int y, const char* s, uint16_t fg,
                  textdatum_t datum = textdatum_t::top_left, uint8_t size = 1) {
  g.setTextColor(fg);
  g.setTextDatum(datum);
  g.setTextSize(size);
  g.drawString(s, x, y);
  g.setTextSize(1);
}

// A glyph (small vector icon) centred in a rect — used on the control pad so the
// art set stays tiny and scalable.
enum class Glyph : uint8_t { ARROW_UP, ARROW_DOWN, TURN_L, TURN_R, JUMP, LOCK,
                             PLAY, REPEAT, CALL, SENSE };

void drawGlyph(LGFX& g, Glyph gl, int cx, int cy, int size, uint16_t color);

}  // namespace ui
