// GridBot — shared UI widgets tuned for big fingers on resistive touch (SPEC §10).
// Thin helpers over LovyanGFX; all targets are drawn >= ~40px by callers.
#pragma once
#include <cstdio>
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

// Word-wrapped text drawn top-left from (x,y), breaking on spaces so no line exceeds
// maxW pixels (measured with the live font). Returns the y just below the last line, so a
// caller can flow content underneath. A single word wider than maxW is left to overhang.
inline int labelWrap(LGFX& g, int x, int y, int maxW, const char* s, uint16_t fg,
                     uint8_t size = 1, int lineH = 0) {
  g.setTextColor(fg);
  g.setTextDatum(textdatum_t::top_left);
  g.setTextSize(size);
  if (lineH <= 0) lineH = g.fontHeight() + 2;
  char line[128] = {0};
  const char* w = s;
  char word[64];
  while (*w) {
    while (*w == ' ') w++;
    int wl = 0;
    while (*w && *w != ' ' && wl < (int)sizeof(word) - 1) word[wl++] = *w++;
    word[wl] = 0;
    if (wl == 0) break;
    char cand[128];
    if (line[0] == 0) snprintf(cand, sizeof(cand), "%s", word);
    else              snprintf(cand, sizeof(cand), "%s %s", line, word);
    if (line[0] == 0 || (int)g.textWidth(cand) <= maxW) {
      snprintf(line, sizeof(line), "%s", cand);
    } else {
      g.drawString(line, x, y); y += lineH;
      snprintf(line, sizeof(line), "%s", word);
    }
  }
  if (line[0]) { g.drawString(line, x, y); y += lineH; }
  g.setTextSize(1);
  return y;
}

// A glyph (small vector icon) centred in a rect — used on the control pad so the
// art set stays tiny and scalable.
enum class Glyph : uint8_t { ARROW_UP, ARROW_DOWN, TURN_L, TURN_R, JUMP, LOCK,
                             PLAY, REPEAT, CALL, SENSE, ZAP };

void drawGlyph(LGFX& g, Glyph gl, int cx, int cy, int size, uint16_t color);

}  // namespace ui
