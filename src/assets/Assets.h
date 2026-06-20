// GridBot — character + goal-token art (SPEC §4.1, §12). Vector-drawn (no bitmaps)
// so the whole roster costs a few hundred bytes of code, not KB of PROGMEM, and
// scales to any tile size. The avatar index selects a matched (character, goal)
// pair; art is purely cosmetic (zero effect on the engine).
#pragma once
#include "hal/Display.h"
#include "game/Types.h"

namespace assets {

constexpr int ROSTER_SIZE = 8;

struct RosterEntry {
  const char* name;
  const char* goalName;
  uint16_t bodyColor;
  uint16_t goalColor;
};

const RosterEntry& roster(int avatar);

// Draw the character upright, centred in the tile, with a small nose/arrow cue for
// `facing` (SPEC §4.1 — never rotate the whole sprite).
void drawCharacter(LGFX& g, int cx, int cy, int tile, int avatar, gb::Facing facing);

// Draw the themed goal token (cheese/carrot/egg…) centred in the tile.
void drawGoalToken(LGFX& g, int cx, int cy, int tile, int avatar);

// Pixel-editor palette: index 0 = empty (not drawn), 1..8 = colours.
constexpr int PALETTE_N = 9;
extern const uint16_t PIXEL_PALETTE[PALETTE_N];

// Draw a custom 16x16 sprite (palette indices) centred in the tile, scaled to fit.
void drawCustomSprite(LGFX& g, int cx, int cy, int tile, const uint8_t* pix);

// Draw the robot with an explicit body colour (for the shop's colour unlocks).
void drawCharacterTinted(LGFX& g, int cx, int cy, int tile, uint16_t bodyColor, gb::Facing facing);

// ---- shop catalogue --------------------------------------------------------
struct ShopColor { uint16_t color; int price; const char* name; };
struct ShopEmoji { uint8_t id; int price; const char* name; };
constexpr int SHOP_COLOR_N = 6;
constexpr int SHOP_EMOJI_N = 6;
extern const ShopColor SHOP_COLORS[SHOP_COLOR_N];
extern const ShopEmoji SHOP_EMOJIS[SHOP_EMOJI_N];
// Draw a little equippable emoji/accessory (id 1..6) centred at (cx,cy).
void drawEmoji(LGFX& g, uint8_t id, int cx, int cy, int size);

}  // namespace assets
