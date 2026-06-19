#include "game/Score.h"

namespace gb {

// BFS over (row, col, facing) states. Unit-cost edges => BFS gives the minimum.
int shortestSolutionLen(const Maze& m, bool allowJump) {
  const int R = m.rows(), C = m.cols();
  if (R <= 0 || C <= 0) return -1;
  const int N = R * C * 4;
  // dist array on the stack-ish (max 320 states). Use a small static buffer.
  static int16_t dist[MAZE_MAX_CELLS * 4];
  static int16_t queue[MAZE_MAX_CELLS * 4];
  for (int i = 0; i < N; i++) dist[i] = -1;

  auto idx = [&](int r, int c, int f) { return (r * C + c) * 4 + f; };

  Pose s = m.startPose();
  int start = idx(s.row, s.col, s.facing);
  dist[start] = 0;
  int head = 0, tail = 0;
  queue[tail++] = start;

  while (head < tail) {
    int cur = queue[head++];
    int f = cur & 3;
    int rc = cur >> 2;
    int c = rc % C;
    int r = rc / C;
    if (m.isGoal(r, c)) return dist[cur];
    int d = dist[cur];

    // turn left / right
    int turns[2] = {(f + 1) & 3, (f + 3) & 3};
    for (int k = 0; k < 2; k++) {
      int ni = idx(r, c, turns[k]);
      if (dist[ni] < 0) { dist[ni] = d + 1; queue[tail++] = ni; }
    }
    // forward
    int dr, dc;
    facingDelta((Facing)f, dr, dc);
    int fr = r + dr, fc = c + dc;
    if (m.inBounds(fr, fc) && m.isWalkable(fr, fc)) {
      int ni = idx(fr, fc, f);
      if (dist[ni] < 0) { dist[ni] = d + 1; queue[tail++] = ni; }
    }
    // jump over a single pit
    if (allowJump) {
      int mr = r + dr, mc = c + dc;
      int lr = r + 2 * dr, lc = c + 2 * dc;
      if (m.inBounds(lr, lc) && m.at(mr, mc) != WALL && m.isWalkable(lr, lc)) {
        int ni = idx(lr, lc, f);
        if (dist[ni] < 0) { dist[ni] = d + 1; queue[tail++] = ni; }
      }
    }
  }
  return -1;  // unreachable (generation should prevent this)
}

}  // namespace gb
