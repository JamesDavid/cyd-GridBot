// GridBot — the shop. Spend coins (collected in levels) on a custom robot colour
// and an equippable emoji. Tap to buy (if you can afford it), tap an owned item to
// equip/unequip. Choices persist in the profile and show up in-game.
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"
#include "game/Profile.h"

namespace screens {

class ShopScreen : public app::IScreen {
 public:
  void begin(gb::Profile* p) { _p = p; }
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;

 private:
  void draw();
  ui::Rect colorRect(int i) const;
  ui::Rect emojiRect(int i) const;
  gb::Profile* _p = nullptr;
  const char* _msg = nullptr;
  app::TapDetector _tap;
};

}  // namespace screens
