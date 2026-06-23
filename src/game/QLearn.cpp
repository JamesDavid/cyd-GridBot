#include "game/QLearn.h"

namespace gb {

// actions: 0=N, 1=E, 2=S, 3=W
static const int dR[4] = {-1, 0, 1, 0};
static const int dC[4] = {0, 1, 0, -1};

static inline uint32_t lcg(uint32_t& s) { s = s * 1664525u + 1013904223u; return s; }

void QLearn::init(const Maze* maze, uint32_t seed) {
  m = maze;
  rng = seed ? seed : 1u;
  episodes = 0;
  for (int s = 0; s < MAZE_MAX_CELLS; s++)
    for (int a = 0; a < 4; a++) Q[s][a] = 0.0f;
}

float QLearn::maxQ(int r, int c) const {
  int s = r * m->cols() + c;
  float mx = Q[s][0];
  for (int a = 1; a < 4; a++) if (Q[s][a] > mx) mx = Q[s][a];
  return mx;
}

int QLearn::bestAction(int r, int c) const {
  int s = r * m->cols() + c, best = 0;
  for (int a = 1; a < 4; a++) if (Q[s][a] > Q[s][best]) best = a;
  return best;
}

bool QLearn::runEpisode(int maxSteps) {
  episodes++;
  Pose start = m->startPose();
  int r = start.row, c = start.col;
  bool collected[MAZE_MAX_CELLS] = {false};  // coins/gems pay out once per episode (no farming)
  for (int step = 0; step < maxSteps; step++) {
    int s = r * m->cols() + c;
    // epsilon-greedy: usually exploit, sometimes try something random (explore)
    int a;
    if ((lcg(rng) >> 8) % 100 < (uint32_t)(epsilon * 100)) a = (lcg(rng) >> 8) % 4;
    else a = bestAction(r, c);

    int nr = r + dR[a], nc = c + dC[a];
    float reward; bool terminal = false; bool won = false;
    if (!m->inBounds(nr, nc) || m->at(nr, nc) == WALL) {
      nr = r; nc = c; reward = -0.05f;          // bumped a wall: stay put
    } else if (m->at(nr, nc) == PIT) {
      reward = -1.0f; terminal = true;          // fell
    } else if (m->isGoal(nr, nc)) {
      reward = 1.0f; terminal = true; won = true;  // reached the battery
    } else {
      reward = -0.01f;                          // a step costs a little (be efficient)
      Tile nt = m->at(nr, nc);                  // coins/gems are rewarding too (collect on the way)
      int ns = nr * m->cols() + nc;
      if (nt == STAR && !collected[ns]) { reward += 0.5f; collected[ns] = true; }  // gem
      else if (nt == COIN && !collected[ns]) { reward += 0.3f; collected[ns] = true; }
    }
    float future = terminal ? 0.0f : maxQ(nr, nc);
    Q[s][a] += alpha * (reward + gamma * future - Q[s][a]);
    if (terminal) return won;
    r = nr; c = nc;
  }
  return false;
}

bool QLearn::greedySolves(int maxSteps) const {
  Pose start = m->startPose();
  int r = start.row, c = start.col;
  for (int step = 0; step < maxSteps; step++) {
    if (m->isGoal(r, c)) return true;
    int a = bestAction(r, c);
    int nr = r + dR[a], nc = c + dC[a];
    if (!m->inBounds(nr, nc) || !m->isWalkable(nr, nc)) return false;  // stuck/into hazard
    r = nr; c = nc;
  }
  return m->isGoal(r, c);
}

}  // namespace gb
