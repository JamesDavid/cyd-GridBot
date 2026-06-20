#include "assets/Assets.h"
#include "ui/Theme.h"
#include <math.h>

namespace assets {

using ui::rgb;

// Roster (SPEC §4.1). Colours stand in for full sprites; the matched goal token is
// drawn in goalColor. Adding a character = appending one row here.
static const RosterEntry kRoster[ROSTER_SIZE] = {
  {"Mouse",     "cheese",  rgb(200, 200, 210), rgb(255, 210, 90)},
  {"Rabbit",    "carrot",  rgb(235, 235, 240), rgb(255, 140, 50)},
  {"Dragon",    "egg",     rgb(90, 200, 110),  rgb(250, 245, 230)},
  {"Cat",       "fish",    rgb(255, 170, 80),  rgb(150, 200, 230)},
  {"Bee",       "flower",  rgb(255, 205, 50),  rgb(255, 120, 180)},
  {"Robot",     "battery", rgb(150, 170, 200), rgb(120, 230, 120)},
  {"Astronaut", "star",    rgb(230, 235, 245), rgb(255, 230, 120)},
  {"Frog",      "fly",     rgb(120, 210, 90),  rgb(120, 120, 130)},
};

const RosterEntry& roster(int avatar) {
  if (avatar < 0 || avatar >= ROSTER_SIZE) avatar = 0;
  return kRoster[avatar];
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

void drawCharacter(LGFX& g, int cx, int cy, int tile, int avatar, gb::Facing facing) {
  const RosterEntry& e = roster(avatar);
  int r = tile * 0.36f;
  if (r < 5) r = 5;
  g.fillCircle(cx, cy, r, e.bodyColor);
  g.drawCircle(cx, cy, r, ui::C_BG);
  // ears / antennae hint (kept generic & upright)
  g.fillCircle(cx - r / 2, cy - r, r / 3, e.bodyColor);
  g.fillCircle(cx + r / 2, cy - r, r / 3, e.bodyColor);
  // eyes
  int eo = r / 3;
  g.fillCircle(cx - eo, cy - eo / 2, 2, ui::C_BG);
  g.fillCircle(cx + eo, cy - eo / 2, 2, ui::C_BG);
  // facing nose/arrow cue
  int dr, dc;
  gb::facingDelta(facing, dr, dc);
  int nx = cx + dc * (r + 2), ny = cy + dr * (r + 2);
  uint16_t nose = ui::C_ACCENT;
  if (dc != 0) g.fillTriangle(nx, ny - 3, nx, ny + 3, nx + dc * 4, ny, nose);
  else         g.fillTriangle(nx - 3, ny, nx + 3, ny, nx, ny + dr * 4, nose);
}

void drawGoalToken(LGFX& g, int cx, int cy, int tile, int avatar) {
  const RosterEntry& e = roster(avatar);
  int r = tile * 0.30f;
  if (r < 4) r = 4;
  // a little pedestal + a star-ish prize so the goal reads clearly at any size
  g.fillCircle(cx, cy, r, e.goalColor);
  for (int a = 0; a < 360; a += 72) {
    float rad = a * 3.14159f / 180.f;
    int x = cx + (int)((r + 2) * cosf(rad));
    int y = cy + (int)((r + 2) * sinf(rad));
    g.fillCircle(x, y, 1, e.goalColor);
  }
  g.drawCircle(cx, cy, r, ui::C_BG);
}

}  // namespace assets
