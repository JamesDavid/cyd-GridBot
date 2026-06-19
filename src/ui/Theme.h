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
constexpr uint16_t C_WALL      = rgb(96, 104, 140);
constexpr uint16_t C_PIT       = rgb(8, 9, 14);

// Layout bands.
constexpr int SCREEN_W = 320, SCREEN_H = 240;
constexpr int TOPBAR_H = 22;
constexpr int BOTBAR_H = 30;
constexpr int BAND_Y = TOPBAR_H;
constexpr int BAND_H = SCREEN_H - TOPBAR_H - BOTBAR_H;  // ~188
constexpr int BOTBAR_Y = SCREEN_H - BOTBAR_H;

struct Rect {
  int16_t x = 0, y = 0, w = 0, h = 0;
  bool contains(int px, int py) const {
    return px >= x && px < x + w && py >= y && py < y + h;
  }
  int cx() const { return x + w / 2; }
  int cy() const { return y + h / 2; }
};

}  // namespace ui
