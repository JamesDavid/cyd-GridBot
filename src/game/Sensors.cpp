#include "game/Sensors.h"
#include <math.h>

namespace gb {

// Project a target (tr,tc) relative to the robot at p into the robot's frame:
//   aheadness = how much the target is in front (+) vs behind (-)   [-1..1]
//   rightness = how much the target is to the right (+) vs left (-) [-1..1]
//   dist      = straight-line distance, normalized by the board diagonal [0..1]
static void projectEgo(const Maze& m, const Pose& p, int tr, int tc,
                       float& aheadness, float& rightness, float& dist) {
  float gvr = (float)(tr - p.row), gvc = (float)(tc - p.col);
  float mag = sqrtf(gvr * gvr + gvc * gvc);
  float diag = sqrtf((float)(m.rows() * m.rows() + m.cols() * m.cols()));
  dist = (diag > 0) ? (mag / diag) : 0.0f;
  if (dist > 1.0f) dist = 1.0f;
  if (mag < 1e-3f) { aheadness = 0; rightness = 0; return; }
  float gur = gvr / mag, guc = gvc / mag;
  int fdr, fdc; facingDelta(p.facing, fdr, fdc);
  int rdr, rdc; facingDelta(turnRight(p.facing), rdr, rdc);
  aheadness = gur * fdr + guc * fdc;
  rightness = gur * rdr + guc * rdc;
}

void senseEgoTo(const Maze& m, const Pose& p, const EnemyView* enemy,
                int tr, int tc, float* out) {
  int fdr, fdc; facingDelta(p.facing, fdr, fdc);
  int ldr, ldc; facingDelta(turnLeft(p.facing), ldr, ldc);
  int rdr, rdc; facingDelta(turnRight(p.facing), rdr, rdc);
  int ar = p.row + fdr, ac = p.col + fdc;   // cell ahead

  out[0] = (m.at(ar, ac) == WALL || !m.inBounds(ar, ac)) ? 1.0f : 0.0f;  // wall ahead
  out[1] = !m.isWalkable(p.row + ldr, p.col + ldc) ? 1.0f : 0.0f;        // wall left
  out[2] = !m.isWalkable(p.row + rdr, p.col + rdc) ? 1.0f : 0.0f;        // wall right
  out[3] = (m.at(ar, ac) == PIT) ? 1.0f : 0.0f;                         // pit ahead

  if (tr < 0 || tc < 0) { out[4] = 0.0f; out[5] = 0.0f; out[6] = 1.0f; }  // no goal (e.g. Sumo)
  else projectEgo(m, p, tr, tc, out[4], out[5], out[6]);                  // target ego

  if (enemy && enemy->pose) {
    projectEgo(m, p, enemy->pose->row, enemy->pose->col, out[7], out[8], out[9]);
  } else {
    out[7] = out[8] = 0.0f; out[9] = 1.0f;  // no enemy: neutral bearing, far away
  }
}

void senseEgo(const Maze& m, const Pose& p, const EnemyView* enemy, float* out) {
  senseEgoTo(m, p, enemy, m.goalRow(), m.goalCol(), out);  // target = the goal
}

}  // namespace gb
