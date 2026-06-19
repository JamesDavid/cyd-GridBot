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

}  // namespace assets
