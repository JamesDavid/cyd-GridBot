// GridBot / NeuroBot — the "Generalist" challenge. Run ONE frozen brain across the
// campaign's first levels and count how many it clears WITHOUT any further training,
// proving the net generalizes. A brain that clears 1..50 earns the Generalist badge.
// Platform-agnostic (host-tested) — the same deterministic mazes the campaign uses.
#pragma once
#include <stdint.h>
#include "game/Net.h"

namespace gb {

// Number of campaign levels a frozen `brain` clears starting from level 1, stopping at
// the first miss (a level counts only if the brain wins EVERY board it presents). The
// return is the count of consecutive cleared levels (0..upToLevel). No training happens.
int gauntletRun(const Net& brain, uint32_t seedBase, int upToLevel);

constexpr int GENERALIST_LEVELS = 50;  // the agreed prize bar

}  // namespace gb
