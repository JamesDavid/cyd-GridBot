#include "ui/UI.h"
#include <math.h>

namespace ui {

// Simple vector glyphs (no bitmaps) — cheap to draw and scale with the control pad.
void drawGlyph(LGFX& g, Glyph gl, int cx, int cy, int s, uint16_t color) {
  int h = s / 2;
  switch (gl) {
    case Glyph::ARROW_UP:
      g.fillTriangle(cx, cy - h, cx - h, cy, cx + h, cy, color);
      g.fillRect(cx - h / 3, cy, h / 1.5 + 1, h, color);
      break;
    case Glyph::ARROW_DOWN:
      g.fillTriangle(cx, cy + h, cx - h, cy, cx + h, cy, color);
      g.fillRect(cx - h / 3, cy - h, h / 1.5 + 1, h, color);
      break;
    case Glyph::TURN_L:
      // curved (semi-circle) arrow turning left
      for (int a = 0; a < 270; a += 6) {
        float rad = a * 3.14159f / 180.f;
        int x = cx + (int)(h * 0.8f * cosf(rad));
        int y = cy + (int)(h * 0.8f * sinf(rad));
        g.fillCircle(x, y, 2, color);
      }
      g.fillTriangle(cx - h, cy, cx - h - 4, cy - 6, cx - h + 4, cy - 6, color);
      break;
    case Glyph::TURN_R:
      for (int a = 270; a > 0; a -= 6) {
        float rad = a * 3.14159f / 180.f;
        int x = cx + (int)(h * 0.8f * cosf(rad));
        int y = cy + (int)(h * 0.8f * sinf(rad));
        g.fillCircle(x, y, 2, color);
      }
      g.fillTriangle(cx + h, cy, cx + h + 4, cy - 6, cx + h - 4, cy - 6, color);
      break;
    case Glyph::JUMP:
      // an arc hop
      for (int x = -h; x <= h; x += 2) {
        int y = (int)(-(h - (x * x) / (float)h));
        g.fillCircle(cx + x, cy + y / 1, 2, color);
      }
      g.fillTriangle(cx + h, cy, cx + h - 5, cy - 5, cx + h + 1, cy - 7, color);
      break;
    case Glyph::LOCK:
      g.drawRoundRect(cx - h / 2, cy - 2, s / 2, h, 2, color);
      g.drawCircle(cx, cy - 2, h / 2 - 1, color);
      break;
    case Glyph::PLAY:
      g.fillTriangle(cx - h / 2, cy - h, cx - h / 2, cy + h, cx + h, cy, color);
      break;
    case Glyph::REPEAT:  // a clear "R" badge (matches the CALL "F"), not a cryptic ring
      g.drawRoundRect(cx - h, cy - h, s, s, 2, color);
      ui::label(g, cx, cy, "R", color, textdatum_t::middle_center);
      break;
    case Glyph::CALL:
      g.drawRect(cx - h, cy - h / 2, s, h, color);
      ui::label(g, cx, cy, "F", color, textdatum_t::middle_center);
      break;
    case Glyph::SENSE:
      g.drawCircle(cx, cy, h - 1, color);
      g.fillCircle(cx, cy, 2, color);
      break;
    case Glyph::ZAP: {  // a lightning bolt
      g.fillTriangle(cx + 1, cy - h, cx - h / 2, cy + 1, cx, cy + 1, color);
      g.fillTriangle(cx - 1, cy + h, cx + h / 2, cy - 1, cx, cy - 1, color);
      break;
    }
  }
}

}  // namespace ui
