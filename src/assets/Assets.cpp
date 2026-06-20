#include "assets/Assets.h"
#include "ui/Theme.h"
#include <math.h>

namespace assets {

using ui::rgb;

// Roster (SPEC §4.1). Colours stand in for full sprites; the matched goal token is
// drawn in goalColor. Adding a character = appending one row here.
// Eight differently-coloured robots; every goal is a battery (drawn the same, tinted
// per robot so the prize matches the bot). Add a robot = append one row.
static const RosterEntry kRoster[ROSTER_SIZE] = {
  {"Sparky", "battery", rgb(96, 170, 255),  rgb(120, 230, 120)},  // blue
  {"Rusty",  "battery", rgb(255, 140, 50),  rgb(120, 230, 120)},  // orange
  {"Cobalt", "battery", rgb(120, 130, 235), rgb(120, 230, 120)},  // indigo
  {"Cherry", "battery", rgb(232, 80, 96),   rgb(120, 230, 120)},  // red
  {"Lemon",  "battery", rgb(255, 205, 50),  rgb(120, 230, 120)},  // yellow
  {"Mint",   "battery", rgb(80, 210, 140),  rgb(120, 230, 120)},  // green
  {"Violet", "battery", rgb(180, 120, 245), rgb(120, 230, 120)},  // purple
  {"Frost",  "battery", rgb(150, 220, 235), rgb(120, 230, 120)},  // cyan
};

const RosterEntry& roster(int avatar) {
  if (avatar < 0 || avatar >= ROSTER_SIZE) avatar = 0;
  return kRoster[avatar];
}

// ---- shop catalogue --------------------------------------------------------
const ShopColor SHOP_COLORS[SHOP_COLOR_N] = {
  {rgb(255, 210, 60),  20, "Gold"},
  {rgb(255, 120, 180), 15, "Pink"},
  {rgb(80, 220, 200),  15, "Teal"},
  {rgb(180, 120, 245), 25, "Purple"},
  {rgb(120, 230, 120), 20, "Lime"},
  {rgb(255, 90, 90),   30, "Crimson"},
};
const ShopEmoji SHOP_EMOJIS[SHOP_EMOJI_N] = {
  {1, 15, "Heart"}, {2, 15, "Star"}, {3, 40, "Crown"},
  {4, 25, "Bolt"},  {5, 20, "Smile"}, {6, 30, "Cool"},
};

void drawEmoji(LGFX& g, uint8_t id, int cx, int cy, int s) {
  int h = s / 2; if (h < 3) h = 3;
  switch (id) {
    case 1:  // heart
      g.fillCircle(cx - h / 2, cy - h / 4, h / 2, rgb(232, 72, 96));
      g.fillCircle(cx + h / 2, cy - h / 4, h / 2, rgb(232, 72, 96));
      g.fillTriangle(cx - h, cy - h / 6, cx + h, cy - h / 6, cx, cy + h, rgb(232, 72, 96));
      break;
    case 2:  // star
      for (int a = 0; a < 360; a += 72) {
        float r1 = a * 3.14159f / 180.f, r2 = (a + 36) * 3.14159f / 180.f;
        g.fillTriangle(cx, cy, cx + h * cosf(r1), cy + h * sinf(r1),
                       cx + (h / 2) * cosf(r2), cy + (h / 2) * sinf(r2), rgb(255, 210, 60));
      }
      break;
    case 3:  // crown
      g.fillTriangle(cx - h, cy + h, cx - h, cy - h, cx - h / 2, cy, rgb(255, 210, 60));
      g.fillTriangle(cx + h, cy + h, cx + h, cy - h, cx + h / 2, cy, rgb(255, 210, 60));
      g.fillTriangle(cx, cy - h, cx - h / 2, cy + h, cx + h / 2, cy + h, rgb(255, 210, 60));
      g.fillRect(cx - h, cy + h - 1, 2 * h, 2, rgb(255, 210, 60));
      break;
    case 4:  // bolt
      g.fillTriangle(cx + 1, cy - h, cx - h / 2, cy + 1, cx + 1, cy + 1, rgb(255, 230, 80));
      g.fillTriangle(cx - 1, cy + h, cx + h / 2, cy - 1, cx - 1, cy - 1, rgb(255, 230, 80));
      break;
    case 5:  // smile
      g.fillCircle(cx, cy, h, rgb(255, 210, 60));
      g.fillCircle(cx - h / 3, cy - h / 4, h / 6 + 1, ui::C_BG);
      g.fillCircle(cx + h / 3, cy - h / 4, h / 6 + 1, ui::C_BG);
      g.drawFastHLine(cx - h / 3, cy + h / 3, 2 * h / 3, ui::C_BG);
      break;
    case 6:  // cool (sunglasses)
      g.fillCircle(cx, cy, h, rgb(255, 210, 60));
      g.fillRect(cx - h, cy - h / 4, 2 * h, h / 3, ui::C_BG);
      g.drawFastHLine(cx - h / 3, cy + h / 3, 2 * h / 3, ui::C_BG);
      break;
  }
}

const uint16_t PIXEL_PALETTE[PALETTE_N] = {
  0,                       // 0 = empty (sentinel, never drawn)
  rgb(232, 72, 72),        // red
  rgb(255, 140, 50),       // orange
  rgb(255, 210, 70),       // yellow
  rgb(80, 200, 120),       // green
  rgb(96, 170, 255),       // blue
  rgb(180, 130, 255),      // purple
  rgb(245, 245, 250),      // white
  rgb(20, 20, 28),         // black
};

void drawCustomSprite(LGFX& g, int cx, int cy, int tile, const uint8_t* pix) {
  int px = tile / 16;
  if (px < 1) px = 1;
  int x0 = cx - px * 8, y0 = cy - px * 8;
  for (int r = 0; r < 16; r++) {
    for (int c = 0; c < 16; c++) {
      uint8_t idx = pix[r * 16 + c];
      if (idx == 0 || idx >= PALETTE_N) continue;
      g.fillRect(x0 + c * px, y0 + r * px, px, px, PIXEL_PALETTE[idx]);
    }
  }
}

static void drawRobot(LGFX& g, int cx, int cy, int tile, uint16_t bodyColor, gb::Facing facing) {
  int s = tile * 0.66f; if (s < 10) s = 10;
  int half = s / 2;
  int ax = cx - half, ay = cy - half;
  int rad = s / 6; if (rad < 2) rad = 2;
  // antenna
  int antH = s / 4;
  g.drawFastVLine(cx, ay - antH, antH, bodyColor);
  g.fillCircle(cx, ay - antH, (s / 12) > 1 ? s / 12 : 2, ui::C_ACCENT);
  // robot head (rounded square)
  g.fillRoundRect(ax, ay, s, s, rad, bodyColor);
  g.drawRoundRect(ax, ay, s, s, rad, ui::C_BG);
  // a darker "visor" band with two glowing eyes
  int vy = cy - s / 8, vh = s / 3;
  g.fillRoundRect(ax + s / 6, vy, s - s / 3, vh, 2, ui::C_BG);
  int eo = s / 6, er = (s / 12) > 1 ? s / 12 : 2;
  g.fillCircle(cx - eo, vy + vh / 2, er, ui::C_GO);
  g.fillCircle(cx + eo, vy + vh / 2, er, ui::C_GO);
  // mouth grille
  for (int i = -1; i <= 1; i++)
    g.drawFastVLine(cx + i * (s / 8), cy + s / 5, s / 6, ui::C_BG);
  // facing arrow cue (which way is "forward")
  int dr, dc; gb::facingDelta(facing, dr, dc);
  int nx = cx + dc * (half + 3), ny = cy + dr * (half + 3);
  uint16_t nose = ui::C_ACCENT;
  if (dc != 0) g.fillTriangle(nx, ny - 4, nx, ny + 4, nx + dc * 5, ny, nose);
  else         g.fillTriangle(nx - 4, ny, nx + 4, ny, nx, ny + dr * 5, nose);
}

void drawCharacter(LGFX& g, int cx, int cy, int tile, int avatar, gb::Facing facing) {
  drawRobot(g, cx, cy, tile, roster(avatar).bodyColor, facing);
}

void drawCharacterTinted(LGFX& g, int cx, int cy, int tile, uint16_t bodyColor, gb::Facing facing) {
  drawRobot(g, cx, cy, tile, bodyColor, facing);
}

void drawGoalToken(LGFX& g, int cx, int cy, int tile, int avatar) {
  const RosterEntry& e = roster(avatar);
  int w = tile * 0.42f; if (w < 8) w = 8;
  int h = tile * 0.58f; if (h < 10) h = 10;
  int x = cx - w / 2, y = cy - h / 2;
  // battery terminal nub on top
  int nubW = w / 3, nubH = h / 6 + 1;
  g.fillRect(cx - nubW / 2, y - nubH, nubW, nubH, ui::C_DIM);
  // battery body
  g.fillRoundRect(x, y, w, h, 2, e.goalColor);
  g.drawRoundRect(x, y, w, h, 2, ui::C_BG);
  // charge level segments (dark gaps)
  g.drawFastHLine(x + 2, y + h / 3, w - 4, ui::C_BG);
  g.drawFastHLine(x + 2, y + 2 * h / 3, w - 4, ui::C_BG);
  // lightning bolt
  int bx = cx, by = cy;
  g.fillTriangle(bx + 1, by - h / 4, bx - w / 6, by + 1, bx + 1, by + 1, ui::C_BG);
  g.fillTriangle(bx - 1, by + h / 4, bx + w / 6, by - 1, bx - 1, by - 1, ui::C_BG);
}

}  // namespace assets
