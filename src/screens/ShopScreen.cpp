#include "screens/ShopScreen.h"
#include "assets/Assets.h"
#include "hal/Audio.h"
#include "store/ProfileStore.h"

using namespace ui;

namespace screens {

static const Rect R_BACK = {6, (int16_t)(BOTBAR_Y + 2), 120, 26};

ui::Rect ShopScreen::colorRect(int i) const {
  return {(int16_t)(8 + i * 52), 92, 46, 34};
}
ui::Rect ShopScreen::emojiRect(int i) const {
  return {(int16_t)(8 + i * 52), 162, 46, 34};
}

void ShopScreen::enter() { _msg = nullptr; draw(); }

void ShopScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Shop", C_ACCENT, textdatum_t::top_left, 2);
  char bal[20]; snprintf(bal, sizeof(bal), "Coins: %u", _p ? (unsigned)_p->coins : 0);
  label(g, SCREEN_W - 6, 6, bal, ui::rgb(255, 210, 60), textdatum_t::top_right);

  // live preview of the equipped look
  uint16_t tint = (_p && _p->shopColor > 0) ? assets::SHOP_COLORS[_p->shopColor - 1].color : 0;
  if (tint) assets::drawCharacterTinted(g, 28, BAND_Y + 12, 30, tint, gb::SOUTH);
  else assets::drawCharacter(g, 28, BAND_Y + 12, 30, _p ? _p->avatar : 0, gb::SOUTH);
  if (_p && _p->shopEmoji > 0) assets::drawEmoji(g, _p->shopEmoji, 40, BAND_Y + 2, 8);
  if (_msg) label(g, 60, BAND_Y + 8, _msg, C_INK);

  label(g, 8, 80, "Robot colour", C_DIM);
  for (int i = 0; i < assets::SHOP_COLOR_N; i++) {
    Rect r = colorRect(i);
    const assets::ShopColor& sc = assets::SHOP_COLORS[i];
    bool owned = _p && (_p->ownedColors & (1u << i));
    bool equip = _p && _p->shopColor == i + 1;
    g.fillRoundRect(r.x, r.y, r.w, r.h - 12, 4, sc.color);
    if (equip) g.drawRoundRect(r.x - 1, r.y - 1, r.w + 2, r.h - 10, 5, C_INK);
    char p[8];
    if (owned) snprintf(p, sizeof(p), equip ? "on" : "have");
    else snprintf(p, sizeof(p), "%dc", sc.price);
    label(g, r.cx(), r.y + r.h - 10, p, owned ? C_GO : C_DIM, textdatum_t::top_center);
  }

  label(g, 8, 150, "Emoji", C_DIM);
  for (int i = 0; i < assets::SHOP_EMOJI_N; i++) {
    Rect r = emojiRect(i);
    const assets::ShopEmoji& se = assets::SHOP_EMOJIS[i];
    bool owned = _p && (_p->ownedEmojis & (1u << i));
    bool equip = _p && _p->shopEmoji == se.id;
    g.fillRoundRect(r.x, r.y, r.w, r.h - 12, 4, equip ? C_PANEL_HI : C_PANEL);
    assets::drawEmoji(g, se.id, r.cx(), r.y + 11, 9);
    char p[8];
    if (owned) snprintf(p, sizeof(p), equip ? "on" : "have");
    else snprintf(p, sizeof(p), "%dc", se.price);
    label(g, r.cx(), r.y + r.h - 10, p, owned ? C_GO : C_DIM, textdatum_t::top_center);
  }

  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
}

app::Signal ShopScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  if (R_BACK.contains(tx, ty)) return app::Signal::BACK;
  if (!_p) return app::Signal::NONE;

  for (int i = 0; i < assets::SHOP_COLOR_N; i++) {
    if (!colorRect(i).contains(tx, ty)) continue;
    bool owned = _p->ownedColors & (1u << i);
    if (owned) { _p->shopColor = (_p->shopColor == i + 1) ? 0 : (i + 1); _msg = "equipped!"; store::profiles.save(*_p); hal::audio.blip(); }
    else if (_p->coins >= (uint32_t)assets::SHOP_COLORS[i].price) {
      _p->coins -= assets::SHOP_COLORS[i].price;
      _p->ownedColors |= (1u << i); _p->shopColor = i + 1; _msg = "bought!"; store::profiles.save(*_p); hal::audio.badge();
    } else { _msg = "need more coins"; hal::audio.fail(); }
    draw(); return app::Signal::NONE;
  }
  for (int i = 0; i < assets::SHOP_EMOJI_N; i++) {
    if (!emojiRect(i).contains(tx, ty)) continue;
    uint8_t id = assets::SHOP_EMOJIS[i].id;
    bool owned = _p->ownedEmojis & (1u << i);
    if (owned) { _p->shopEmoji = (_p->shopEmoji == id) ? 0 : id; _msg = "equipped!"; store::profiles.save(*_p); hal::audio.blip(); }
    else if (_p->coins >= (uint32_t)assets::SHOP_EMOJIS[i].price) {
      _p->coins -= assets::SHOP_EMOJIS[i].price;
      _p->ownedEmojis |= (1u << i); _p->shopEmoji = id; _msg = "bought!"; store::profiles.save(*_p); hal::audio.badge();
    } else { _msg = "need more coins"; hal::audio.fail(); }
    draw(); return app::Signal::NONE;
  }
  return app::Signal::NONE;
}

}  // namespace screens
