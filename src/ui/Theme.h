// GridBot — colour palette + layout constants (320x240 landscape, SPEC §10).
// Colours are normal RGB565; the panel's R/B swap is handled by CYD_PANEL_RGB_ORDER.
#pragma once
#include <stdint.h>

namespace ui {

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// Palette — friendly, high-contrast for kids.
constexpr uint16_t C_BG        = rgb(24, 26, 38);     // deep slate
constexpr uint16_t C_PANEL     = rgb(40, 44, 64);
constexpr uint16_t C_PANEL_HI  = rgb(60, 66, 92);
constexpr uint16_t C_INK       = rgb(238, 240, 248);  // text
constexpr uint16_t C_DIM       = rgb(140, 146, 168);
constexpr uint16_t C_ACCENT    = rgb(255, 196, 0);    // GridBot yellow
constexpr uint16_t C_GO        = rgb(64, 200, 120);   // run / win green
constexpr uint16_t C_BAD       = rgb(232, 72, 72);    // fail red
constexpr uint16_t C_MOVE      = rgb(96, 170, 255);   // move commands (blue)
constexpr uint16_t C_TURN      = rgb(180, 130, 255);  // turn commands (purple)
constexpr uint16_t C_LOOP      = rgb(255, 150, 70);   // repeat construct
constexpr uint16_t C_SENSE     = rgb(80, 200, 200);   // sense construct
constexpr uint16_t C_FUNC      = rgb(230, 110, 180);  // function construct
constexpr uint16_t C_LOCK      = rgb(70, 74, 96);

// Maze tile colours.
constexpr uint16_t C_FLOOR     = rgb(52, 58, 82);
constexpr uint16_t C_FLOOR2    = rgb(46, 52, 74);     // checker
constexpr uint16_t C_WALL      = rgb(156, 64, 52);    // brick red
constexpr uint16_t C_WALL_LINE = rgb(120, 44, 36);    // brick mortar lines
constexpr uint16_t C_PIT       = rgb(8, 9, 14);       // (legacy) — pits drawn as void

// Layout bands.
constexpr int SCREEN_W = 320, SCREEN_H = 240;
constexpr int TOPBAR_H = 22;
constexpr int BOTBAR_H = 36;  // taller toolbar so buttons sit clear of the touch-
                              // inaccurate bottom edge (kids kept missing them)
constexpr int BAND_Y = TOPBAR_H;
constexpr int BAND_H = SCREEN_H - TOPBAR_H - BOTBAR_H;  // ~182
constexpr int BOTBAR_Y = SCREEN_H - BOTBAR_H;

// Formerly reserved for the always-on corner sound icon. The icon moved into a long-press
// menu (App), so the corner is free again -- kept as 0 so the header layouts that subtract it
// simply reclaim the space (no per-screen edits needed).
constexpr int SOUND_ICON_W = 0;

// Level biomes — the floor/wall palette shifts as you climb so progress feels
// visual. Walls keep the brick texture, just tinted; pits stay the background void.
struct Biome {
  uint16_t floorA, floorB, wall, wallLine, crumb;  // crumb = visited-tile breadcrumb
  const char* name;
};

inline Biome biomeFor(int level) {
  if (level <= 5)  return {rgb(40, 72, 52), rgb(34, 62, 46), rgb(96, 150, 86),  rgb(62, 104, 56), rgb(190, 230, 120), "Meadow"};
  if (level <= 10) return {rgb(58, 50, 44), rgb(50, 44, 40), rgb(156, 88, 60),  rgb(116, 60, 42), rgb(255, 180, 90),  "Cavern"};
  if (level <= 15) return {rgb(44, 60, 84), rgb(38, 54, 78), rgb(126, 176, 214),rgb(82, 124, 162),rgb(130, 230, 245), "Glacier"};
  if (level <= 21) return {rgb(34, 46, 42), rgb(28, 40, 36), rgb(70, 140, 96),  rgb(46, 96, 64),  rgb(120, 245, 160), "Circuit"};
  return                  {rgb(36, 30, 54), rgb(30, 26, 48), rgb(126, 92, 176), rgb(84, 62, 124), rgb(240, 150, 230), "Nebula"};
}

struct Rect {
  int16_t x = 0, y = 0, w = 0, h = 0;
  bool contains(int px, int py) const {
    return px >= x && px < x + w && py >= y && py < y + h;
  }
  int cx() const { return x + w / 2; }
  int cy() const { return y + h / 2; }
};

}  // namespace ui
