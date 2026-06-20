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

// BFS that records each state's predecessor + the action taken, then reconstructs
// the command list. Mirrors shortestSolutionLen's transitions.
bool solveMaze(const Maze& m, bool allowJump, Program& out) {
  out.clear();
  const int R = m.rows(), C = m.cols();
  if (R <= 0 || C <= 0) return false;
  const int N = R * C * 4;
  static int16_t dist[MAZE_MAX_CELLS * 4];
  static int16_t prev[MAZE_MAX_CELLS * 4];
  static uint8_t act[MAZE_MAX_CELLS * 4];   // 0=TURN_L,1=TURN_R,2=FWD,3=JUMP
  static int16_t queue[MAZE_MAX_CELLS * 4];
  for (int i = 0; i < N; i++) { dist[i] = -1; prev[i] = -1; }
  auto idx = [&](int r, int c, int f) { return (r * C + c) * 4 + f; };

  Pose s = m.startPose();
  int start = idx(s.row, s.col, s.facing);
  dist[start] = 0;
  int head = 0, tail = 0, goalState = -1;
  queue[tail++] = start;
  while (head < tail) {
    int cur = queue[head++];
    int f = cur & 3, rc = cur >> 2, c = rc % C, r = rc / C;
    if (m.isGoal(r, c)) { goalState = cur; break; }
    int d = dist[cur];
    auto relax = [&](int ni, uint8_t a) {
      if (dist[ni] < 0) { dist[ni] = d + 1; prev[ni] = cur; act[ni] = a; queue[tail++] = ni; }
    };
    relax(idx(r, c, (f + 1) & 3), 1);  // turn right
    relax(idx(r, c, (f + 3) & 3), 0);  // turn left
    int dr, dc; facingDelta((Facing)f, dr, dc);
    int fr = r + dr, fc = c + dc;
    if (m.inBounds(fr, fc) && m.isWalkable(fr, fc)) relax(idx(fr, fc, f), 2);
    if (allowJump) {
      int mr = r + dr, mc = c + dc, lr = r + 2 * dr, lc = c + 2 * dc;
      if (m.inBounds(lr, lc) && m.at(mr, mc) != WALL && m.isWalkable(lr, lc))
        relax(idx(lr, lc, f), 3);
    }
  }
  if (goalState < 0) return false;

  // walk back, collecting actions in reverse
  static uint8_t seq[MAZE_MAX_CELLS * 4];
  int n = 0;
  for (int cur = goalState; cur != start && cur >= 0; cur = prev[cur]) seq[n++] = act[cur];
  const Cmd cmdOf[4] = {CMD_TURN_L, CMD_TURN_R, CMD_FWD, CMD_JUMP};
  for (int i = n - 1; i >= 0; i--) out.main.push_back(Node::command(cmdOf[seq[i]]));
  return true;
}

int distanceToGoal(const Maze& m, int r0, int c0) {
  const int R = m.rows(), C = m.cols();
  if (R <= 0 || C <= 0 || !m.inBounds(r0, c0)) return -1;
  static int16_t dist[MAZE_MAX_CELLS];
  static int16_t queue[MAZE_MAX_CELLS];
  for (int i = 0; i < R * C; i++) dist[i] = -1;
  int head = 0, tail = 0;
  dist[r0 * C + c0] = 0;
  queue[tail++] = r0 * C + c0;
  const int dR[4] = {-1, 1, 0, 0}, dC[4] = {0, 0, -1, 1};
  while (head < tail) {
    int cur = queue[head++], r = cur / C, c = cur % C;
    if (m.isGoal(r, c)) return dist[cur];
    for (int k = 0; k < 4; k++) {
      int nr = r + dR[k], nc = c + dC[k];
      if (!m.inBounds(nr, nc) || !m.isWalkable(nr, nc)) continue;
      int ni = nr * C + nc;
      if (dist[ni] < 0) { dist[ni] = dist[cur] + 1; queue[tail++] = ni; }
    }
  }
  return -1;  // goal unreachable from here (e.g. cut off)
}

}  // namespace gb

