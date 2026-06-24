#include "game/Distill.h"
#include "core/Util.h"   // Rng for random foe placements (hunter distillation)
#include "game/Program.h"
#include "game/Interpreter.h"
#include "game/Sensors.h"
#include "game/Reactive.h"
#include "game/MazeGen.h"
#include "game/Score.h"

namespace gb {

// One shared episode scratch buffer, reused by rnnTrainGeneral and Brain Cam (saves DRAM).
static float gEpiX[RNET_MAX_T * SENSOR_COUNT];
static int   gEpiAct[RNET_MAX_T];
static int   gEpiT = 0;
// Shared per-episode Q-learning scratch (qTrainHunterRnn / qTrainMazeRnn never run concurrently).
static float gQrew[RNET_MAX_T], gQmax[RNET_MAX_T], gQt[RNET_MAX_T];

int captureSolverShared(const Maze& m) {
  gEpiT = 0;
  Program sol;
  if (!solveMaze(m, true, sol)) return 0;
  Interpreter it; it.load(&sol, &m, m.startPose(), 500);
  int guard = 0;
  while (!it.finished() && guard++ < 400 && gEpiT < RNET_MAX_T) {
    Pose before = it.pose();
    float s[SENSOR_COUNT]; senseEgo(m, before, nullptr, s);
    it.step();
    const Node* n = it.currentNode();
    if (!n || n->type != N_CMD) continue;
    int a = -1;
    switch (n->cmd) { case CMD_FWD: a = 0; break; case CMD_TURN_L: a = 1; break;
                      case CMD_TURN_R: a = 2; break; case CMD_JUMP: a = 3; break; default: break; }
    if (a < 0) continue;
    for (int i = 0; i < SENSOR_COUNT; i++) gEpiX[gEpiT * SENSOR_COUNT + i] = s[i];
    gEpiAct[gEpiT] = a; gEpiT++;
  }
  return gEpiT;
}

float ffTrainShared(Net& brain, int passes) {
  if (gEpiT <= 0) return 0.0f;
  float L = 0;
  for (int e = 0; e < passes; e++) {
    L = 0;
    for (int t = 0; t < gEpiT; t++) {
      float tg[NET_MAX_OUT] = {0}; tg[gEpiAct[t]] = 1.0f;
      L += brain.trainStep(&gEpiX[t * SENSOR_COUNT], tg);
    }
    L /= gEpiT;
  }
  return L;
}

float rnnTrainShared(RNet& brain, int passes) {
  if (gEpiT <= 0) return 0.0f;
  float L = 0;
  for (int e = 0; e < passes; e++) L = brain.trainEpisode(gEpiX, gEpiAct, gEpiT);
  return L;
}

void rnnTrainGeneral(RNet& brain, uint32_t seedBase, int levels, int epochs) {
  for (int e = 0; e < epochs; e++)
    for (int l = 1; l <= levels; l++) {
      Maze m; MazeGen::generate(m, seedBase, l);
      // roll out the memory-using explorer, capturing (senses, action) each step
      uint8_t vis[MAZE_MAX_CELLS] = {0};
      Pose p = m.startPose(); vis[p.row * m.cols() + p.col] = 1;
      int T = 0;
      for (int s = 0; s < RNET_MAX_T; s++) {
        if (m.isGoal(p.row, p.col)) break;
        senseEgo(m, p, nullptr, gEpiX + T * SENSOR_COUNT);
        Cmd c = exploreActionTo(m, p, vis, m.goalRow(), m.goalCol());
        gEpiAct[T] = reactiveCmdToAction(c); T++;
        if (reactiveApply(m, p, c) != OUT_OK) break;
        vis[p.row * m.cols() + p.col] = 1;
      }
      if (T > 0) brain.trainEpisode(gEpiX, gEpiAct, T);
    }
}

// The shared backprop loop: run `teacher` on `m` and train `brain` to copy each primitive
// move (egocentric sensors -> one-hot action). Re-simulates each epoch (no big buffers).
static void distillTeacher(Net& brain, const Maze& m, const Program& teacher, int epochs) {
  for (int e = 0; e < epochs; e++) {
    Interpreter it; it.load(&teacher, &m, m.startPose(), 500);
    int guard = 0;
    while (!it.finished() && guard++ < 400) {
      Pose before = it.pose();
      float s[SENSOR_COUNT];
      senseEgo(m, before, nullptr, s);
      it.step();
      const Node* n = it.currentNode();
      if (!n || n->type != N_CMD) continue;
      int act = -1;
      switch (n->cmd) {
        case CMD_FWD:    act = 0; break;
        case CMD_TURN_L: act = 1; break;
        case CMD_TURN_R: act = 2; break;
        case CMD_JUMP:   act = 3; break;
        default: break;
      }
      if (act < 0) continue;
      float t[NET_MAX_OUT] = {0};
      t[act] = 1.0f;                       // one-hot: copy the teacher's move
      brain.trainStep(s, t);               // backprop one example
    }
  }
}

bool distillSolver(Net& brain, const Maze& m, bool allowJump, int epochs) {
  Program sol;
  if (!solveMaze(m, allowJump, sol)) return false;  // no solution -> nothing to teach
  distillTeacher(brain, m, sol, epochs);
  return true;
}

bool pathToProgram(const Maze& m, const uint8_t* tiles, int n, Program& out) {
  out.clear();
  if (n < 1) return false;
  const int cols = m.cols();
  Pose p = m.startPose();
  if (tiles[0] != (uint8_t)(p.row * cols + p.col)) return false;  // must start at the bot
  for (int i = 1; i < n; i++) {
    int r0 = tiles[i - 1] / cols, c0 = tiles[i - 1] % cols;
    int r1 = tiles[i] / cols,     c1 = tiles[i] % cols;
    int dr = r1 - r0, dc = c1 - c0;
    if (dr != 0 && dc != 0) return false;                 // no diagonals
    int dist = (dr ? (dr < 0 ? -dr : dr) : (dc < 0 ? -dc : dc));
    if (dist != 1 && dist != 2) return false;             // forward (1) or jump (2) only
    Facing target;
    if (dr < 0) target = NORTH; else if (dr > 0) target = SOUTH;
    else if (dc > 0) target = EAST; else target = WEST;
    int diff = ((int)target - (int)p.facing) & 3;         // minimal turns to face target
    if (diff == 1)      out.main.push_back(Node::command(CMD_TURN_R));
    else if (diff == 3) out.main.push_back(Node::command(CMD_TURN_L));
    else if (diff == 2) { out.main.push_back(Node::command(CMD_TURN_R));
                          out.main.push_back(Node::command(CMD_TURN_R)); }
    p.facing = target;
    out.main.push_back(Node::command(dist == 2 ? CMD_JUMP : CMD_FWD));
    p.row = (int8_t)r1; p.col = (int8_t)c1;
  }
  return true;
}

bool distillPath(Net& brain, const Maze& m, const uint8_t* tiles, int n, int epochs) {
  Program teacher;
  if (!pathToProgram(m, tiles, n, teacher)) return false;
  if (teacher.main.empty()) return false;   // a single-tile "path" teaches nothing
  distillTeacher(brain, m, teacher, epochs);
  return true;
}

// Is the tile straight ahead a wall, a pit, or off-board? (a step there is fatal in Battle)
static bool dangerAhead(const Maze& m, const Pose& me) {
  int dr, dc; facingDelta(me.facing, dr, dc);
  int ar = me.row + dr, ac = me.col + dc;
  return !m.inBounds(ar, ac) || m.at(ar, ac) == WALL || m.at(ar, ac) == PIT;
}

// The expert Battle move for a (me, foe) layout: a greedy CONE-CHASER. Zap a foe in the tile
// ahead; else if the foe lies within the forward ~45deg cone (more ahead than off-axis) and the
// path is clear, charge straight in (this closes on a DIAGONAL foe, where the reactive seeker
// merely oscillates and never advances); else turn toward the foe; a foe dead behind -> turn
// around. Decidable from the egocentric senses (aheadness vs rightness + wall-ahead), so a tiny
// net can imitate it. Actions match the net's outputs: 0 fwd, 1 turnL, 2 turnR, 3 jump, 4 zap.
static int idealHuntAction(const Maze& m, const Pose& me, const Pose& foe) {
  int dr, dc; facingDelta(me.facing, dr, dc);
  int ar = me.row + dr, ac = me.col + dc;
  if (foe.row == ar && foe.col == ac) return 4;                 // foe in the tile ahead -> ZAP
  int er = foe.row - me.row, ec = foe.col - me.col;
  int side  = er * dc - ec * dr;                                 // >0 foe to the right, <0 to the left
  int front = er * dr + ec * dc;                                 // >0 foe in front, <0 behind
  int aside = side < 0 ? -side : side;
  if (front > 0 && front >= aside && !dangerAhead(m, me)) return 0;  // foe in front cone & clear -> charge
  if (side > 0) return 2;                                        // foe more to the right -> turn right
  if (side < 0) return 1;                                        // foe more to the left  -> turn left
  return 2;                                                      // foe dead behind / blocked -> rotate
}

bool distillHunter(Net& brain, uint32_t seed, int epochs, bool jitterFoe) {
  Rng rng(seed);
  Maze m; Pose s0, s1;
  MazeGen::generateSumoRing(m, seed, s0, s1);   // a real battle ring (no goal, open)
  int rows = m.rows(), cols = m.cols();
  float in[SENSOR_COUNT];
  int trained = 0;
  // ON-POLICY ROLLOUT: follow the expert hunter's own path toward a (static) foe and learn each
  // move it makes. This is the key to a balanced action mix -- uniform random states are almost
  // all "turn", which collapses the net to spinning in place; a real chase is turn-to-face, then
  // a run of forwards, then a zap, so forward/zap get seen in life-like proportion.
  while (trained < epochs) {
    Pose me;  me.row  = (int8_t)rng.below(rows); me.col  = (int8_t)rng.below(cols); me.facing = (Facing)rng.below(4);
    Pose foe; foe.row = (int8_t)rng.below(rows); foe.col = (int8_t)rng.below(cols);
    if ((me.row == foe.row && me.col == foe.col) ||
        m.at(me.row, me.col) == WALL || m.at(foe.row, foe.col) == WALL) continue;
    EnemyView ev; ev.pose = &foe;
    for (int step = 0; step < 30 && trained < epochs; step++) {
      senseEgo(m, me, &ev, in);                 // 10 senses incl. the foe's bearing/distance
      int act = idealHuntAction(m, me, foe);
      float tg[NET_MAX_OUT] = {0}; tg[act] = 1.0f;
      // Oversample the rare-but-decisive labels. ZAP happens once per chase (foe adjacent ahead),
      // so without weighting the net just rams forward and loses HP duels; the edge/wall "turn
      // away" label is rare too, and one mis-fired forward there rings the bot out and loses.
      int reps = (act == 4) ? 4 : (dangerAhead(m, me) ? 2 : 1);
      for (int r = 0; r < reps; r++) brain.trainStep(in, tg);
      trained += reps;
      // advance the expert along its own trajectory (foe stays put -> the chase closes the gap)
      if (act == 4) break;                       // reached & zapped -> new scenario
      if (act == 1) me.facing = turnLeft(me.facing);
      else if (act == 2) me.facing = turnRight(me.facing);
      else {                                     // forward
        int dr, dc; facingDelta(me.facing, dr, dc);
        int ar = me.row + dr, ac = me.col + dc;
        if (!m.isWalkable(ar, ac) || (ar == foe.row && ac == foe.col)) break;
        me.row = (int8_t)ar; me.col = (int8_t)ac;
      }
      // Jitter the foe so the net learns to track a MOVING target. Trained against a static foe it
      // dithers on the novel bearings a real (moving) player creates -> "stuck a few tiles away".
      // Opt-in (the game enables it; the host tests pin the original static-foe behaviour).
      if (jitterFoe && rng.below(100) < 30) {
        int fdr, fdc; facingDelta((Facing)rng.below(4), fdr, fdc);
        int nr = foe.row + fdr, nc = foe.col + fdc;
        if (m.isWalkable(nr, nc) && !(nr == me.row && nc == me.col)) { foe.row = (int8_t)nr; foe.col = (int8_t)nc; }
      }
    }
  }
  return true;
}

// Map a cardinal (dr,dc) step to the Facing that produces it.
static Facing facingFromDelta(int dr, int dc) {
  for (int f = 0; f < 4; f++) { int a, b; facingDelta((Facing)f, a, b); if (a == dr && b == dc) return (Facing)f; }
  return EAST;
}
// Turn toward a desired facing in one step (left or right; caller handles "already aligned").
static int turnToward(Facing from, Facing want) { return (turnRight(from) == want) ? 2 : 1; }

// The expert SOCCER move for a (me, ball, goal) layout. The ball can only be shoved one cardinal
// tile, so we push it along the dominant ball->goal axis: the "push point" is the tile on the
// OPPOSITE side of the ball from the goal. Get there, face the push direction, step forward (which
// lands on the ball and shoves it goalward). Actions: 0 fwd, 1 turnL, 2 turnR, 3 jump (unused), 4 n/a.
static int idealSoccerAction(const Maze& m, const Pose& me, const Pose& ball, const Pose& goal) {
  int gdr = goal.row - ball.row, gdc = goal.col - ball.col;
  int pdr, pdc;
  if ((gdr < 0 ? -gdr : gdr) >= (gdc < 0 ? -gdc : gdc) && gdr != 0) { pdr = gdr < 0 ? -1 : 1; pdc = 0; }
  else { pdr = 0; pdc = gdc < 0 ? -1 : 1; }                 // push along the bigger axis
  Facing pushF = facingFromDelta(pdr, pdc);
  Pose push; push.row = (int8_t)(ball.row - pdr); push.col = (int8_t)(ball.col - pdc);
  if (me.row == push.row && me.col == push.col) {           // standing on the push point
    if (me.facing == pushF) return 0;                       // aligned -> shove the ball
    return turnToward(me.facing, pushF);                    // rotate to the push direction
  }
  // Navigate to the push point with the same cone-chaser the hunter uses; "in the tile ahead"
  // means step ONTO it (forward) rather than zap.
  int act = idealHuntAction(m, me, push);
  return (act == 4) ? 0 : act;
}

bool distillSoccer(Net& brain, uint32_t seed, int epochs) {
  Rng rng(seed);
  Maze m; Pose s0, s1;
  MazeGen::generateSumoRing(m, seed, s0, s1);   // an open pitch (no internal walls)
  int rows = m.rows(), cols = m.cols();
  float in[SENSOR_COUNT];
  int trained = 0;
  while (trained < epochs) {
    // random ball, goal, and bot (all on walkable floor, all distinct)
    Pose ball; ball.row = (int8_t)rng.below(rows); ball.col = (int8_t)rng.below(cols);
    Pose goal; goal.row = (int8_t)rng.below(rows); goal.col = (int8_t)rng.below(cols);
    Pose me;   me.row = (int8_t)rng.below(rows); me.col = (int8_t)rng.below(cols); me.facing = (Facing)rng.below(4);
    if (!m.isWalkable(ball.row, ball.col) || !m.isWalkable(me.row, me.col)) continue;
    if ((ball.row == goal.row && ball.col == goal.col) ||
        (me.row == ball.row && me.col == ball.col)) continue;
    EnemyView ev; ev.pose = &goal;                // "enemy" bearing carries the goal (slots 7-9)
    for (int step = 0; step < 36 && trained < epochs; step++) {
      senseEgoTo(m, me, &ev, ball.row, ball.col, in);   // target = the ball (slots 4-6)
      int act = idealSoccerAction(m, me, ball, goal);
      float tg[NET_MAX_OUT] = {0}; tg[act] = 1.0f;
      int reps = (act == 0 ? 1 : 1) + (dangerAhead(m, me) ? 1 : 0);  // a touch of edge-safety weight
      for (int r = 0; r < reps; r++) brain.trainStep(in, tg);
      trained += reps;
      // advance the expert along its own trajectory; a forward that lands on the ball pushes it.
      if (act == 1) me.facing = turnLeft(me.facing);
      else if (act == 2) me.facing = turnRight(me.facing);
      else {
        int dr, dc; facingDelta(me.facing, dr, dc);
        int ar = me.row + dr, ac = me.col + dc;
        if (!m.inBounds(ar, ac) || !m.isWalkable(ar, ac)) break;
        if (ar == ball.row && ac == ball.col) {           // stepping into the ball -> shove it
          int nr = ball.row + dr, nc = ball.col + dc;
          bool intoGoal = (nr == goal.row && nc == goal.col);
          if (!intoGoal && !m.isWalkable(nr, nc)) break;   // can't push (wall) -> new scenario
          ball.row = (int8_t)nr; ball.col = (int8_t)nc;
          if (intoGoal) break;                             // scored -> fresh scenario
        }
        me.row = (int8_t)ar; me.col = (int8_t)ac;
      }
    }
  }
  return true;
}

bool distillHunterRnn(RNet& brain, uint32_t seed, int episodes) {
  Rng rng(seed);
  Maze m; Pose s0, s1;
  MazeGen::generateSumoRing(m, seed, s0, s1);
  int rows = m.rows(), cols = m.cols();
  if (episodes < 1) return false;
  for (int e = 0; e < episodes; e++) {
    if (e > 0 && e % 20 == 0) MazeGen::generateSumoRing(m, seed + (uint32_t)e * 2654435761u, s0, s1);
    Pose me; do { me.row = (int8_t)rng.below(rows); me.col = (int8_t)rng.below(cols); } while (m.at(me.row, me.col) == WALL);
    me.facing = (Facing)rng.below(4);
    Pose foe; do { foe.row = (int8_t)rng.below(rows); foe.col = (int8_t)rng.below(cols); }
              while (m.at(foe.row, foe.col) == WALL || (foe.row == me.row && foe.col == me.col));
    EnemyView ev; ev.pose = &foe;
    // capture one chase as a (senses -> action) SEQUENCE, then BPTT it (reuses the shared buffer)
    int T = 0;
    for (int step = 0; step < RNET_MAX_T && step < 30; step++) {
      senseEgo(m, me, &ev, gEpiX + T * SENSOR_COUNT);
      int a = idealHuntAction(m, me, foe);
      gEpiAct[T] = a; T++;
      if (a == 4) break;
      if (a == 1) me.facing = turnLeft(me.facing);
      else if (a == 2) me.facing = turnRight(me.facing);
      else {
        int dr, dc; facingDelta(me.facing, dr, dc);
        int ar = me.row + dr, ac = me.col + dc;
        if (!m.isWalkable(ar, ac) || (ar == foe.row && ac == foe.col)) break;
        me.row = (int8_t)ar; me.col = (int8_t)ac;
      }
    }
    if (T > 0) brain.trainEpisode(gEpiX, gEpiAct, T);
  }
  brain.trained = true;
  return true;
}

// Chebyshev distance -- how many bot moves to be adjacent (turns aside, diagonals close in one
// row + one col), so it matches how the hunter actually closes the gap on the grid.
static int chebyshev(const Pose& a, const Pose& b) {
  int dr = a.row - b.row; if (dr < 0) dr = -dr;
  int dc = a.col - b.col; if (dc < 0) dc = -dc;
  return dr > dc ? dr : dc;
}

bool qTrainHunter(Net& brain, uint32_t seed, int episodes, int globalDone, int globalTotal, float epsScale) {
  if (globalTotal <= 0) globalTotal = episodes;
  Rng rng(seed);
  Maze m; Pose s0, s1;
  MazeGen::generateSumoRing(m, seed, s0, s1);
  int rows = m.rows(), cols = m.cols();
  // Re-generate the ring every few episodes (different pillar layouts) so the fighter GENERALISES
  // to the random battle ring it'll really face, instead of overfitting one board's geometry.
  const int REROLL = 20;
  // ALL 5 outputs are in play: the match takes argmax over every output, so an untrained action
  // (e.g. jump) would keep its random init value and spuriously win -- jump MUST be experienced
  // and learned-down too, or the fighter leaps about instead of turning toward the foe.
  const int ACTS[5] = {0, 1, 2, 3, 4};  // forward, turnL, turnR, jump, zap
  const int NA = 5;
  const float G = 0.9f;               // discount: value a near-term KO over a far one
  float in[SENSOR_COUNT], nin[SENSOR_COUNT], out[NET_MAX_OUT], nout[NET_MAX_OUT];
  if (episodes < 1) return false;
  for (int e = 0; e < episodes; e++) {
    if (e > 0 && e % REROLL == 0) MazeGen::generateSumoRing(m, seed + (uint32_t)e * 2654435761u, s0, s1);
    // a random legal start: learner and a (static) foe on two different floor tiles
    Pose me; do { me.row = (int8_t)rng.below(rows); me.col = (int8_t)rng.below(cols); } while (m.at(me.row, me.col) == WALL);
    me.facing = (Facing)rng.below(4);
    Pose foe;
    // CURRICULUM: half the episodes start the foe within a couple of tiles, so the agent frequently
    // reaches the terminal ZAP win early on -- otherwise wins are too rare for the +1 to propagate
    // back and the value function collapses to ~0 (a dud fighter). The rest start anywhere.
    bool near = (rng.below(2) == 0);
    do {
      if (near) {
        int dr = (int)rng.below(5) - 2, dc = (int)rng.below(5) - 2;   // within a 2-tile box
        foe.row = (int8_t)(me.row + dr); foe.col = (int8_t)(me.col + dc);
      } else { foe.row = (int8_t)rng.below(rows); foe.col = (int8_t)rng.below(cols); }
    } while (!m.inBounds(foe.row, foe.col) || m.at(foe.row, foe.col) == WALL ||
             (foe.row == me.row && foe.col == me.col));
    EnemyView ev; ev.pose = &foe;
    float prog = (float)(globalDone + e) / (float)globalTotal;          // 0..1 across the WHOLE run
    float eps = 0.05f + 0.30f * epsScale * (1.0f - prog);               // explore a lot early, exploit late
    for (int step = 0; step < 28; step++) {
      senseEgo(m, me, &ev, in);
      brain.forward(in, out);
      // epsilon-greedy over all actions
      int bestI = 0; float bestQ = out[ACTS[0]];
      for (int k = 1; k < NA; k++) if (out[ACTS[k]] > bestQ) { bestQ = out[ACTS[k]]; bestI = k; }
      int ai  = (rng.below(1000) < (uint32_t)(eps * 1000.0f)) ? (int)rng.below(NA) : bestI;
      int act = ACTS[ai];

      int dr, dc; facingDelta(me.facing, dr, dc);
      int ar = me.row + dr, ac = me.col + dc;
      int oldD = chebyshev(me, foe);
      Pose nm = me; bool terminal = false; float r = 0.0f;
      if (act == 4) {                                       // ZAP
        if (foe.row == ar && foe.col == ac) { r = 1.0f; terminal = true; }   // KO -> WIN
        else r = -0.05f;                                    // wasted shot
      } else if (act == 1) { nm.facing = turnLeft(me.facing);  r = -0.01f; }
      else if (act == 2)   { nm.facing = turnRight(me.facing); r = -0.01f; }
      else if (act == 3) {                                  // JUMP (2 ahead; mostly a bad idea here)
        int br = me.row + 2 * dr, bc = me.col + 2 * dc;
        if (!m.inBounds(br, bc) || m.at(br, bc) == PIT) { r = 0.0f; terminal = true; } // leaps off -> LOSE
        else if (m.at(br, bc) == WALL || (br == foe.row && bc == foe.col)) { r = -0.05f; } // blocked, stay
        else { nm.row = (int8_t)br; nm.col = (int8_t)bc; }
      } else {                                              // FORWARD
        if (!m.inBounds(ar, ac) || m.at(ar, ac) == PIT) { r = 0.0f; terminal = true; }  // ring-out -> LOSE
        else if (m.at(ar, ac) == WALL)                  { r = -0.05f; }                  // bonk, stay put
        else if (ar == foe.row && ac == foe.col)        { r = -0.02f; }                  // bumping the foe is NOT the win -- ZAP is (else it learns to ram in place forever)
        else { nm.row = (int8_t)ar; nm.col = (int8_t)ac; }
      }
      if (!terminal) r += 0.03f * (float)(oldD - chebyshev(nm, foe));        // shaping: reward closing in

      float target = r;
      if (!terminal) {
        senseEgo(m, nm, &ev, nin);
        brain.forward(nin, nout);
        float mx = nout[ACTS[0]];
        for (int k = 1; k < NA; k++) if (nout[ACTS[k]] > mx) mx = nout[ACTS[k]];
        target += G * mx;
      }
      if (target < 0.0f) target = 0.0f; else if (target > 1.0f) target = 1.0f;  // keep in the sigmoid's range
      // semi-gradient TD update on ONLY the chosen action (others keep their current value)
      float tg[NET_MAX_OUT]; for (int k = 0; k < NET_MAX_OUT; k++) tg[k] = out[k];
      tg[act] = target;
      brain.trainStep(in, tg);

      me = nm;
      if (terminal) break;
    }
  }
  return true;
}

// One step of the Battle MDP (learner moves, static foe): apply action `act` from pose `me`,
// returning the reward and whether the episode ends, and writing the resulting pose to `nm`.
// Shared by the feedforward and recurrent Q-learners so they optimise the SAME problem.
static float qStep(const Maze& m, const Pose& me, const Pose& foe, int act, Pose& nm, bool& terminal) {
  int dr, dc; facingDelta(me.facing, dr, dc);
  int ar = me.row + dr, ac = me.col + dc;
  int oldD = chebyshev(me, foe);
  nm = me; terminal = false; float r = 0.0f;
  if (act == 4) {
    if (foe.row == ar && foe.col == ac) { r = 1.0f; terminal = true; } else r = -0.05f;
  } else if (act == 1) { nm.facing = turnLeft(me.facing);  r = -0.01f; }
  else if (act == 2)   { nm.facing = turnRight(me.facing); r = -0.01f; }
  else if (act == 3) {
    int br = me.row + 2 * dr, bc = me.col + 2 * dc;
    if (!m.inBounds(br, bc) || m.at(br, bc) == PIT) { r = 0.0f; terminal = true; }
    else if (m.at(br, bc) == WALL || (br == foe.row && bc == foe.col)) { r = -0.05f; }
    else { nm.row = (int8_t)br; nm.col = (int8_t)bc; }
  } else {
    if (!m.inBounds(ar, ac) || m.at(ar, ac) == PIT) { r = 0.0f; terminal = true; }
    else if (m.at(ar, ac) == WALL) { r = -0.05f; }
    else if (ar == foe.row && ac == foe.col) { r = -0.02f; }
    else { nm.row = (int8_t)ar; nm.col = (int8_t)ac; }
  }
  if (!terminal) r += 0.03f * (float)(oldD - chebyshev(nm, foe));
  return r;
}

bool qTrainHunterRnn(RNet& brain, uint32_t seed, int episodes, int globalDone, int globalTotal, float epsScale) {
  if (globalTotal <= 0) globalTotal = episodes;
  Rng rng(seed);
  Maze m; Pose s0, s1;
  MazeGen::generateSumoRing(m, seed, s0, s1);
  int rows = m.rows(), cols = m.cols();
  const int NA = 5;            // forward, turnL, turnR, jump, zap
  const float G = 0.9f;
  if (episodes < 1) return false;
  float *rew = gQrew, *qmax = gQmax, *qt = gQt;   // shared scratch (saves DRAM)
  float out[RNET_MAX_OUT];
  for (int e = 0; e < episodes; e++) {
    if (e > 0 && e % 20 == 0) MazeGen::generateSumoRing(m, seed + (uint32_t)e * 2654435761u, s0, s1);
    Pose me; do { me.row = (int8_t)rng.below(rows); me.col = (int8_t)rng.below(cols); } while (m.at(me.row, me.col) == WALL);
    me.facing = (Facing)rng.below(4);
    Pose foe; bool near = (rng.below(2) == 0);
    do {
      if (near) { foe.row = (int8_t)(me.row + (int)rng.below(5) - 2); foe.col = (int8_t)(me.col + (int)rng.below(5) - 2); }
      else      { foe.row = (int8_t)rng.below(rows); foe.col = (int8_t)rng.below(cols); }
    } while (!m.inBounds(foe.row, foe.col) || m.at(foe.row, foe.col) == WALL || (foe.row == me.row && foe.col == me.col));
    EnemyView ev; ev.pose = &foe;
    brain.resetState();
    float eps = 0.05f + 0.30f * epsScale * (1.0f - (float)(globalDone + e) / (float)globalTotal);
    int T = 0; bool lastTerminal = false;
    for (int step = 0; step < 28 && T < RNET_MAX_T; step++) {
      float* x = gEpiX + T * SENSOR_COUNT;
      senseEgo(m, me, &ev, x);
      brain.step(x, out);                              // Q(s_t); advances the recurrent memory
      int bestI = 0; for (int k = 1; k < NA; k++) if (out[k] > out[bestI]) bestI = k;
      int act = (rng.below(1000) < (uint32_t)(eps * 1000.0f)) ? (int)rng.below(NA) : bestI;
      qmax[T] = out[bestI];
      gEpiAct[T] = act;
      Pose nm; bool terminal; rew[T] = qStep(m, me, foe, act, nm, terminal);
      T++; me = nm;
      if (terminal) { lastTerminal = true; break; }
    }
    if (T <= 0) continue;
    for (int t = 0; t < T; t++) {                       // semi-gradient TD targets (bootstrap from next step)
      float target = rew[t] + ((t + 1 < T) ? G * qmax[t + 1] : 0.0f);
      if (target < 0.0f) target = 0.0f; else if (target > 1.0f) target = 1.0f;
      qt[t] = target;
    }
    (void)lastTerminal;
    brain.trainEpisodeQ(gEpiX, gEpiAct, qt, T);
  }
  brain.trained = true;
  return true;
}

bool qTrainMaze(Net& brain, uint32_t seedBase, int levels, int episodes, int globalDone,
                int globalTotal, const Maze* board, const Pose* start, float epsScale) {
  if (globalTotal <= 0) globalTotal = episodes;
  if (levels < 1) levels = 1;
  if (episodes < 1) return false;
  Rng rng(seedBase ^ 0x51ed2701u);
  const Cmd ACT[5] = {CMD_FWD, CMD_TURN_L, CMD_TURN_R, CMD_JUMP, CMD_FIRE};   // zap = no-op in a maze
  const int NA = 5; const float G = 0.92f;
  float in[SENSOR_COUNT], nin[SENSOR_COUNT], out[NET_MAX_OUT], nout[NET_MAX_OUT];
  for (int e = 0; e < episodes; e++) {
    Maze m; Pose p;
    if (board) { m = *board; p = start ? *start : m.startPose(); }
    else { MazeGen::generate(m, seedBase, 1 + (e % levels)); p = m.startPose(); }
    float eps = 0.05f + 0.30f * epsScale * (1.0f - (float)(globalDone + e) / (float)globalTotal);
    int oldD = distanceToGoal(m, p.row, p.col);
    for (int step = 0; step < 48; step++) {
      senseEgo(m, p, nullptr, in);
      brain.forward(in, out);
      int bestI = 0; for (int k = 1; k < NA; k++) if (out[k] > out[bestI]) bestI = k;
      int act = (rng.below(1000) < (uint32_t)(eps * 1000.0f)) ? (int)rng.below(NA) : bestI;
      Pose np = p; Outcome o = reactiveApply(m, np, ACT[act]);
      float r; bool terminal = false;
      if (o == OUT_WIN)       { r = 1.0f; terminal = true; }
      else if (o == OUT_FELL) { r = 0.0f; terminal = true; }
      else {
        int newD = distanceToGoal(m, np.row, np.col);
        r = (o == OUT_BONK) ? -0.05f : (act == 4 ? -0.03f : -0.004f);
        if (oldD >= 0 && newD >= 0) r += 0.05f * (float)(oldD - newD);
        oldD = newD;
      }
      float target = r;
      if (!terminal) {
        senseEgo(m, np, nullptr, nin);
        brain.forward(nin, nout);
        float mx = nout[0]; for (int k = 1; k < NA; k++) if (nout[k] > mx) mx = nout[k];
        target += G * mx;
      }
      if (target < 0.0f) target = 0.0f; else if (target > 1.0f) target = 1.0f;
      float tg[NET_MAX_OUT]; for (int k = 0; k < NET_MAX_OUT; k++) tg[k] = out[k];
      tg[act] = target;
      brain.trainStep(in, tg);
      p = np;
      if (terminal) break;
    }
  }
  return true;
}

bool qTrainMazeRnn(RNet& brain, uint32_t seedBase, int levels, int episodes, int globalDone,
                   int globalTotal, const Maze* board, const Pose* start, float epsScale) {
  if (globalTotal <= 0) globalTotal = episodes;
  if (levels < 1) levels = 1;
  if (episodes < 1) return false;
  Rng rng(seedBase ^ 0x51ed2701u);
  const Cmd ACT[5] = {CMD_FWD, CMD_TURN_L, CMD_TURN_R, CMD_JUMP, CMD_FIRE};  // zap is a no-op in Race
  const int NA = 5; const float G = 0.92f;
  float *rew = gQrew, *qmax = gQmax, *qt = gQt;   // shared scratch (saves DRAM)
  float out[RNET_MAX_OUT];
  for (int e = 0; e < episodes; e++) {
    Maze m; Pose p;
    if (board) { m = *board; p = start ? *start : m.startPose(); }   // train on THE board (wins it)
    else { MazeGen::generate(m, seedBase, 1 + (e % levels)); p = m.startPose(); }
    brain.resetState();
    float eps = 0.05f + 0.30f * epsScale * (1.0f - (float)(globalDone + e) / (float)globalTotal);
    int oldD = distanceToGoal(m, p.row, p.col);
    int T = 0;
    for (int step = 0; step < RNET_MAX_T; step++) {
      float* x = gEpiX + T * SENSOR_COUNT;
      senseEgo(m, p, nullptr, x);                    // includes goal bearing/distance
      brain.step(x, out);
      int bestI = 0; for (int k = 1; k < NA; k++) if (out[k] > out[bestI]) bestI = k;
      int ai = (rng.below(1000) < (uint32_t)(eps * 1000.0f)) ? (int)rng.below(NA) : bestI;
      qmax[T] = out[bestI];
      gEpiAct[T] = ai;
      Outcome o = reactiveApply(m, p, ACT[ai]);      // ai==4 -> CMD_FIRE -> OUT_OK, no move
      float r; bool terminal = false;
      if (o == OUT_WIN)       { r = 1.0f; terminal = true; }     // reached the goal -> WIN
      else if (o == OUT_FELL) { r = 0.0f; terminal = true; }     // fell in a pit  -> LOSE
      else {
        int newD = distanceToGoal(m, p.row, p.col);
        r = (o == OUT_BONK) ? -0.05f : (ai == 4 ? -0.03f : -0.004f);   // bonk/zap waste + tiny step cost
        if (oldD >= 0 && newD >= 0) r += 0.05f * (float)(oldD - newD); // BFS shaping toward the goal
        oldD = newD;
      }
      rew[T] = r; T++;
      if (terminal) break;
    }
    if (T <= 0) continue;
    for (int t = 0; t < T; t++) {
      float target = rew[t] + ((t + 1 < T) ? G * qmax[t + 1] : 0.0f);
      if (target < 0.0f) target = 0.0f; else if (target > 1.0f) target = 1.0f;
      qt[t] = target;
    }
    brain.trainEpisodeQ(gEpiX, gEpiAct, qt, T);
  }
  brain.trained = true;
  return true;
}

}  // namespace gb
