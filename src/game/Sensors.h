// GridBot / NeuroBot — egocentric sensors: turn the robot's situation into the brain's
// input vector. EGOCENTRIC (relative to the robot's heading) on purpose — it matches
// GridBot's core lesson ("everything is relative to the robot") and is what lets a
// trained brain GENERALIZE to mazes it never saw. Arduino-free, host-unit-tested.
#pragma once
#include "game/Maze.h"
#include "game/Interpreter.h"  // EnemyView

namespace gb {

// The 10-input layout the unified brain reads (see docs/NEUROBOT_IDEA.md):
//  0 wall ahead     1 wall left      2 wall right     3 pit ahead
//  4 goal aheadness 5 goal rightness 6 goal distance
//  7 enemy aheadness 8 enemy rightness 9 enemy distance
constexpr int SENSOR_COUNT = 10;

// Fill out[0..SENSOR_COUNT). `enemy` may be null (campaign) -> the enemy senses are 0.
void senseEgo(const Maze& m, const Pose& p, const EnemyView* enemy, float* out);

// Same, but the goal-bearing senses (4,5,6) point at an arbitrary target (tr,tc) instead
// of the maze goal. Used by Pilot mode so the brain steers toward the next WAYPOINT — the
// brain can't tell a waypoint from the goal, it just heads for the bearing it's given.
void senseEgoTo(const Maze& m, const Pose& p, const EnemyView* enemy,
                int tr, int tc, float* out);

}  // namespace gb
