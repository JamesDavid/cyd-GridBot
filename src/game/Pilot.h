// GridBot / NeuroBot — "Pilot" mode: the planner + follower split (the Tesla-FSD idea).
// A reactive brain can't plan a whole maze, but a route PLANNER can hand it waypoints and
// the brain only has to steer between them. planWaypoints() runs the BFS route and keeps
// the corners; the interpreter feeds the next corner's bearing to the brain (senseEgoTo).
// Platform-agnostic (host-tested).
#pragma once
#include <stdint.h>
#include "game/Types.h"

namespace gb {

class Maze;
class Net;

constexpr int MAX_WAYPOINTS = 40;

// Plan corner waypoints along the optimal route from `start` to the goal. Fills `wp` with
// tile indices (row*cols + col) at each turn, ending at the goal. Returns the count
// (0 if the maze is unsolvable from `start`).
int planWaypoints(const Maze& m, const Pose& start, uint8_t* wp, int maxN);

// One "Pilot" training batch: distill `brain` (in place) to STEER toward planned waypoints
// across campaign mazes (levels 1..trainLevels). The brain learns "head to the next dot,
// jump pits, round walls" — which then follows any route the planner lays down.
void pilotTrain(Net& brain, uint32_t seedBase, int trainLevels, int epochs);

// Test/UI helper: how many of campaign levels 1..upToLevel a PILOTED `brain` clears
// (planner waypoints + the brain steering). Every board of a level must be cleared.
int pilotRun(const Net& brain, uint32_t seedBase, int upToLevel);

}  // namespace gb
