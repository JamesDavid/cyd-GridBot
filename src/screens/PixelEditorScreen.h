// GridBot — KidPix-style 16x16 pixel editor for a custom character + goal sprite.
// Pencil/eraser/fill/mirror, a colour palette, and a playful BOOM explosion clear.
// Saved per profile (and shareable over the radio). Drawn in-game instead of the
// roster art when present.
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"
#include "game/Profile.h"

namespace screens {

class PixelEditorScreen : public app::IScreen {
 public:
  void begin(gb::Profile* profile);
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;

 private:
  enum class Tool : uint8_t { PENCIL, ERASER, FILL };
  void draw();
  void drawGridAll();
  void drawCellAt(int r, int c);
  void drawPalette();
  void drawTools();
  void paintCell(int r, int c);
  void floodFill(int r, int c, uint8_t from, uint8_t to);
  void boom();
  void mirror();
  void selectTarget(bool goal);
  uint8_t& px(int r, int c) { return (*_target)[r * gb::PIX_DIM + c]; }

  gb::Profile* _profile = nullptr;
  std::vector<uint8_t>* _target = nullptr;
  bool _editingGoal = false;
  uint8_t _color = 4;          // current palette index
  Tool _tool = Tool::PENCIL;
  app::TapDetector _tap;
};

}  // namespace screens
