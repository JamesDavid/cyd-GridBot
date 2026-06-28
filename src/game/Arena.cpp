#include "game/Arena.h"

namespace gb {

void Arena::setup(const Maze* maze, const Program* p0, const Program* p1,
                  const Pose& s0, const Pose& s1, MatchType type, int stepCap) {
  _maze = maze;
  _type = type;
  _stepCap = stepCap;
  _ticks = 0;
  _pressure = 0;
  _outcome = ArenaOutcome::RUNNING;
  _hash = 2166136261u;

  _bot[0].start = s0;
  _bot[1].start = s1;
  for (int i = 0; i < 2; i++) {
    _bot[i].alive = true;
    _bot[i].won = false;
    _bot[i].done = false;
    _bot[i].hp = SUMO_HP;
    _bot[i].zapCd = 0;
  }
  _bot[0].it.load(p0, maze, s0, stepCap);
  _bot[1].it.load(p1, maze, s1, stepCap);
  // ENEMY_* conditions read the live opponent pose (SPEC §18.3).
  _bot[0].enemy.pose = &_bot[1].it.pose();
  _bot[1].enemy.pose = &_bot[0].it.pose();
  _bot[0].it.setEnemy(&_bot[0].enemy);
  _bot[1].it.setEnemy(&_bot[1].enemy);
  _goals[0] = _goals[1] = 0;
  _justScored = false;
  foldLog();
}

void Arena::configSoccer(const Pose& ball, const Pose& goal0, const Pose& goal1) {
  _ball = ball;
  _kickoff = ball;            // the ball returns here after each goal
  _goal[0] = goal0;
  _goal[1] = goal1;
  _goals[0] = _goals[1] = 0;
  // Re-aim each bot's senses for soccer: the TARGET (slots 4-6) is the ball, and the "enemy"
  // bearing (slots 7-9) points at the goal it's attacking -- so the brain learns ball + goal.
  for (int i = 0; i < 2; i++) {
    _bot[i].enemy.pose   = &_bot[1 - i].it.pose();  // the RIVAL bot (slots 7-9) -- real opponent sense
    _bot[i].enemy.target = &_ball;                  // the BALL (slots 4-6)
    _bot[i].enemy.net    = &_goal[i];               // the NET to score on (slots 10-11)
  }
  _ballStall = 0;
  foldLog();
}

// A long-stalled ball drifts one tile toward whichever goal it is already NEARER to -- so once a
// a long-stalled ball: drop it at a FRESH spot so both bots have to scramble for it -- never toward
// a goal (the ref must not score for anyone), just a loose ball. The spot is DETERMINISTIC (hashed
// from the tick + positions, no RNG) so a networked Cup still replays byte-identically on every
// device. Picks a walkable interior tile that isn't on a bot or a goal mouth.
void Arena::refDriftBall() {
  int rows = _maze->rows(), cols = _maze->cols();
  uint32_t h = (uint32_t)_ticks * 2654435761u
             ^ ((uint32_t)_ball.row << 6) ^ ((uint32_t)_ball.col << 1)
             ^ ((uint32_t)_bot[0].it.pose().col << 12) ^ ((uint32_t)_bot[1].it.pose().row << 18);
  const Pose& a = _bot[0].it.pose(); const Pose& b = _bot[1].it.pose();
  for (int t = 0; t < 24; t++) {
    h = h * 1664525u + 1013904223u;
    int r = 1 + (int)((h >> 9) % (uint32_t)(rows - 2));    // interior rows 1..rows-2
    int c = 1 + (int)((h >> 17) % (uint32_t)(cols - 2));   // interior cols 1..cols-2
    if (!_maze->isWalkable(r, c)) continue;
    if ((r == a.row && c == a.col) || (r == b.row && c == b.col)) continue;
    if ((c == _goal[0].col) || (c == _goal[1].col)) continue;   // not in a goal mouth
    _ball.row = (int8_t)r; _ball.col = (int8_t)c; return;
  }
  _ball.row = (int8_t)(rows / 2); _ball.col = (int8_t)(cols / 2);  // fallback: kick off from centre
}

// Bot `mover` has stepped onto the ball's tile -- try to shove the ball one tile further in the
// mover's facing. Off-board / wall / pit blocks the shove; landing on a goal tile scores. Returns
// false if the ball can't move (the caller then bounces the bot back -- you can't walk through it).
bool Arena::pushBall(int mover, bool* squeezed) {
  int dr, dc; facingDelta(_bot[mover].it.pose().facing, dr, dc);
  int nr = _ball.row + dr, nc = _ball.col + dc;
  // A goal is the END column +/- one row of the mouth centre (a 3-tile-tall mouth on the pitch;
  // a single tile elsewhere). Anything in the mouth column at those rows counts as a score.
  auto inGoal = [&](int i, int r, int c) { return c == _goal[i].col && inGoalMouth(r, _goal[i].row); };
  // Own-goal guard -> automatic zap-swap. A shove that would put the ball in the net THIS bot DEFENDS
  // never scores; instead the bot turns 180 (now facing its ATTACK net) and the ball pops to the tile
  // it just came from, so the ball ends up directly AHEAD of the bot pointing the RIGHT way. The bot
  // simply "turns around" with the ball -- the same move a striker can do with a manual zap. No own
  // goals are possible, so a bot caught on the wrong side recovers instead of scoring on itself.
  if (inGoal(1 - mover, nr, nc)) {
    int oj = 1 - mover;
    int br = _ball.row - dr, bc = _ball.col - dc;   // one tile back toward the ATTACK net
    bool clear = _maze->inBounds(br, bc) && _maze->isWalkable(br, bc) &&
                 !(_bot[oj].alive && _bot[oj].it.pose().row == br && _bot[oj].it.pose().col == bc);
    if (!clear) return false;                       // can't reposition -> bounce back (still no own goal)
    _ball.row = (int8_t)br; _ball.col = (int8_t)bc;
    Pose bp = _bot[mover].it.pose(); bp.facing = turnAround(bp.facing);
    _bot[mover].it.setPose(bp);
    _ballStall = 0;
    return true;
  }
  auto land = [&](int r, int c) {                   // own net already handled above -> only the attack goal scores
    if (inGoal(0, r, c))      { _goals[0]++; _justScored = true; kickoff(); }
    else if (inGoal(1, r, c)) { _goals[1]++; _justScored = true; kickoff(); }
    else { _ball.row = (int8_t)r; _ball.col = (int8_t)c; }
    return true;
  };

  auto canLand = [&](int r, int c) {
    return inGoal(0, r, c) || inGoal(1, r, c) || (_maze->inBounds(r, c) && _maze->isWalkable(r, c));
  };

  // Clear shove: the tile ahead is open (or a goal) and no bot sits on it -> the ball just rolls.
  int other = 1 - mover;
  const Pose& op = _bot[other].it.pose();
  bool blockedByBot  = (_bot[other].alive && nr == op.row && nc == op.col);
  bool blockedByWall = !canLand(nr, nc);
  if (!blockedByBot && !blockedByWall) return land(nr, nc);

  // Jammed -- the ball is pressed into a WALL or a bot in the lane. Pop it out SIDEWAYS so it never
  // sticks: try the two tiles perpendicular to the push. Bias the side toward the GOAL the pusher is
  // attacking, so a ball shoved into a defender rounds it GOALWARD (an effective diagonal: forward ->
  // deflect goal-side -> forward) -- and a blocked ball drifts away from your OWN net, not into it. A
  // deterministic hash only breaks ties, so it still replays byte-identically. If the goalward side is
  // blocked too, try the other; only if boxed in on both perpendiculars does the ball stay put.
  uint32_t h = (uint32_t)_ticks * 2654435761u
             ^ (uint32_t)_ball.row * 73856093u ^ (uint32_t)_ball.col * 19349663u
             ^ (uint32_t)(dr + 2) * 83492791u ^ (uint32_t)(dc + 2) * 2971215073u;
  const Pose& tgt = _goal[mover];                  // the net `mover` attacks -> deflect toward it
  int rp = _ball.row - dc, cp = _ball.col + dr;    // the s=+1 perpendicular tile
  int rm = _ball.row + dc, cm = _ball.col - dr;    // the s=-1 perpendicular tile
  int dp = (rp - tgt.row) * (rp - tgt.row) + (cp - tgt.col) * (cp - tgt.col);
  int dm = (rm - tgt.row) * (rm - tgt.row) + (cm - tgt.col) * (cm - tgt.col);
  int s = (dp < dm) ? 1 : (dm < dp) ? -1 : ((h & 1u) ? 1 : -1);   // goalward side first; hash breaks ties
  for (int k = 0; k < 2; k++, s = -s) {
    int pr = _ball.row + s * (-dc);                // (-dc, dr) and (dc, -dr) are the two perpendiculars
    int pc = _ball.col + s * (dr);
    if (canLand(pr, pc)) { if (squeezed) *squeezed = true; return land(pr, pc); }
  }
  return false;   // boxed in on three sides -> no push (the bot bounces back)
}

// After a goal: re-kickoff -- both bots back to their starts, and the ball dropped at a FRESH
// (deterministic) spot rather than always the centre. Varying the kickoff means a bot that froze on
// one ball position gets a different situation next time, instead of replaying the same dead start.
void Arena::kickoff() {
  for (int i = 0; i < 2; i++) _bot[i].it.setPose(_bot[i].start);
  refDriftBall();   // scatter the ball to a new spot (avoids the bots + goals; tick-seeded -> varies)
  _ballStall = 0;
}

void Arena::foldLog() {
  // Fingerprint the full per-tick state so the determinism test is exact.
  auto fold = [&](uint32_t v) { _hash = (_hash ^ v) * 16777619u; };
  for (int i = 0; i < 2; i++) {
    fold((uint32_t)_bot[i].it.pose().row);
    fold((uint32_t)_bot[i].it.pose().col);
    fold((uint32_t)_bot[i].it.pose().facing);
    fold((uint32_t)(_bot[i].alive ? 1 : 0));
    fold((uint32_t)_bot[i].hp);
    fold((uint32_t)_bot[i].zapCd);
  }
  if (_type == MatchType::SOCCER) {  // ball + score are live state in soccer
    fold((uint32_t)_ball.row); fold((uint32_t)_ball.col);
    fold((uint32_t)_goals[0]); fold((uint32_t)_goals[1]);
  }
}

ArenaOutcome Arena::tick() {
  if (_outcome != ArenaOutcome::RUNNING) return _outcome;
  _ticks++;
  for (int i = 0; i < 2; i++) if (_bot[i].zapCd > 0) _bot[i].zapCd--;  // recover the zap

  Pose before[2];
  Outcome out[2] = {OUT_OK, OUT_OK};
  bool moved[2] = {false, false};
  bool zapping[2] = {false, false};
  for (int i = 0; i < 2; i++) {
    before[i] = _bot[i].it.pose();
    if (_bot[i].alive && !_bot[i].done && !_bot[i].it.finished()) {
      out[i] = _bot[i].it.step();
      moved[i] = true;
      // "zap" (CMD_FIRE) is the Sumo attack — used by the hunter bot and by a trained
      // brain's 5th output. Read the effective primitive so a NEURO bot's zap counts
      // (its node is N_NEURO, so currentNode() can't reveal the move it picked).
      zapping[i] = (_bot[i].it.lastCmd() == CMD_FIRE);
    }
  }

  // Resolution pass (SPEC §18.1), moves first.
  // 1) Both bots target the SAME tile.
  if (_bot[0].alive && _bot[1].alive) {
    const Pose& a = _bot[0].it.pose();
    const Pose& b = _bot[1].it.pose();
    if (a.row == b.row && a.col == b.col) {
      // In Sumo, walking INTO the enemy is a shove (real sumo): the bot that moved pushes
      // the one that stayed put one tile onward — off-board / into a PIT = out. Head-on
      // (both moved into the tile) just bounces. This makes contact decisive, not a stalemate.
      bool m0 = (before[0].row != a.row || before[0].col != a.col);
      bool m1 = (before[1].row != b.row || before[1].col != b.col);
      int mover = (_type == MatchType::SUMO && (m0 != m1)) ? (m0 ? 0 : 1) : -1;
      if (mover >= 0) {
        int sit = 1 - mover, dr, dc;
        facingDelta(_bot[mover].it.pose().facing, dr, dc);
        int pr = before[sit].row + dr, pc = before[sit].col + dc;
        if (!_maze->inBounds(pr, pc) || _maze->at(pr, pc) == PIT) {
          _bot[sit].alive = false;                 // shoved off the board / into a pit
        } else if (_maze->isWalkable(pr, pc)) {
          Pose np = _bot[sit].it.pose(); np.row = (int8_t)pr; np.col = (int8_t)pc;
          _bot[sit].it.setPose(np);                // shoved back a tile; mover takes its spot
        } else {
          _bot[mover].it.setPose(before[mover]);   // wall behind the enemy -> mover bounces
        }
        if (out[mover] == OUT_WIN) out[mover] = OUT_OK;
      } else if (_type == MatchType::SUMO || _type == MatchType::SOCCER) {
        // Both charged the same tile: let ONE bot (alternating each tick) HOLD it, the other yields,
        // so they don't freeze bouncing off each other forever -- the bot-vs-bot deadlock. Sumo: they
        // end up adjacent to trade zaps; Soccer: one gets through to the ball. Alternating keeps it fair.
        int adv = (int)(_ticks & 1);
        _bot[1 - adv].it.setPose(before[1 - adv]);   // the other yields this tick
        if (out[adv] == OUT_WIN) out[adv] = OUT_OK;
      } else {
        _bot[0].it.setPose(before[0]);             // Race -> both bounce
        _bot[1].it.setPose(before[1]);
        for (int i = 0; i < 2; i++) if (out[i] == OUT_WIN || out[i] == OUT_FELL) out[i] = OUT_OK;
      }
    }
  }

  // 2) Falls / off-board -> out.  Win flag (Race).
  bool win[2] = {false, false};
  for (int i = 0; i < 2; i++) {
    if (!moved[i]) continue;
    if (out[i] == OUT_FELL) { _bot[i].alive = false; }
    else if (out[i] == OUT_WIN) { win[i] = true; }
    else if (out[i] == OUT_DONE_NO_WIN) { _bot[i].done = true; }
  }

  // 2b) SOCCER ball physics: a bot that stepped onto the ball's tile shoves it one tile ahead
  // (its facing); the ball can't be walked THROUGH, so a blocked push bounces the bot back.
  if (_type == MatchType::SOCCER) {
    _justScored = false;
    Pose ballWas = _ball;
    for (int i = 0; i < 2; i++) {
      if (!moved[i] || !_bot[i].alive) continue;
      const Pose& p = _bot[i].it.pose();
      if (p.row == _ball.row && p.col == _ball.col) {
        bool squeezed = false;
        // No push at all -> bounce back (you can't walk through the ball). If the ball squirted out
        // SIDEWAYS (pressed into a wall), step the bot back one tile too, so it doesn't stay jammed
        // on the wall where the ball just was -- it ends up beside the loose ball, free to chase it.
        if (!pushBall(i, &squeezed) || squeezed) _bot[i].it.setPose(before[i]);
      }
    }
    // Loose-ball "referee": a stuck ball is dropped at a fresh spot so both bots scramble for it again
    // (never nudged toward a goal -- the ref never scores). A true midfield loose ball gets a long
    // fuse; but a DEADLOCK -- both bots jammed against the ball (often pinning it on a wall) -- pops
    // free fast so players don't sit stuck waiting for the respawn. (A goal kicks off -> no stall.)
    if (_ball == ballWas) {
      auto onOrNextTo = [&](int i) { int dr = _bot[i].it.pose().row - _ball.row, dc = _bot[i].it.pose().col - _ball.col;
                                     return dr * dr + dc * dc <= 1; };
      bool deadlock = _bot[0].alive && _bot[1].alive && onOrNextTo(0) && onOrNextTo(1);
      if (++_ballStall >= (deadlock ? 8 : 28)) { refDriftBall(); _ballStall = 0; }
    } else _ballStall = 0;
  }

  // 2c) SOCCER zap: fire while FACING the ball -> SWAP places with it AND turn 180. The ball pops to
  // where you stood (one tile "behind" your old facing) and you take its tile; flipping your facing
  // leaves the ball directly AHEAD of you again, so a single forward drives it back the way you came.
  // That's the whole point -- turn a ball you were shoving the wrong way around in one move, instead
  // of slowly circling it. A short cooldown stops spam-oscillating it.
  if (_type == MatchType::SOCCER) {
    for (int i = 0; i < 2; i++) {
      if (!zapping[i] || !_bot[i].alive || _bot[i].zapCd > 0) continue;
      int dr, dc; facingDelta(_bot[i].it.pose().facing, dr, dc);
      Pose bp = _bot[i].it.pose();
      if (bp.row + dr == _ball.row && bp.col + dc == _ball.col) {   // ball is the tile ahead -> swap
        Pose np = bp; np.row = _ball.row; np.col = _ball.col;
        np.facing = turnAround(bp.facing);                         // 180: the ball is now ahead of me
        _bot[i].it.setPose(np);
        _ball.row = bp.row; _ball.col = bp.col;
        _bot[i].zapCd = SUMO_ZAP_COOLDOWN;
        _ballStall = 0;                                           // a swap counts as touching the ball
      }
    }
  }

  // 3) Attacks resolve AFTER moves (SPEC §18.1). A zap shoves the enemy one tile in
  // the zapper's facing; into a PIT / off-board = out (Sumo, SPEC §18.2). Soccer has no SUMO
  // zap (no damage) -- its zap is the ball-swap, resolved in §2c above, so skip this loop.
  for (int i = 0; _type != MatchType::SOCCER && i < 2; i++) {
    if (!zapping[i] || !_bot[i].alive) continue;
    if (_type == MatchType::SUMO && _bot[i].zapCd > 0) continue;  // still recovering -> fizzle
    int j = 1 - i;
    if (!_bot[j].alive) continue;
    int dr, dc; facingDelta(_bot[i].it.pose().facing, dr, dc);
    int ar = _bot[i].it.pose().row + dr, ac = _bot[i].it.pose().col + dc;
    const Pose& ej = _bot[j].it.pose();
    if (ej.row == ar && ej.col == ac) {  // enemy is in the tile ahead -> HIT it
      if (_type == MatchType::SUMO) { _bot[i].zapCd = SUMO_ZAP_COOLDOWN; if (--_bot[j].hp <= 0) _bot[j].alive = false; }
      int dest_r = ar + dr, dest_c = ac + dc;                                    // and shove it back
      if (!_maze->inBounds(dest_r, dest_c) || _maze->at(dest_r, dest_c) == PIT) {
        _bot[j].alive = false;                                                   // shoved off -> instant out
      } else if (_bot[j].alive && _maze->isWalkable(dest_r, dest_c)) {
        Pose np = ej; np.row = (int8_t)dest_r; np.col = (int8_t)dest_c;
        _bot[j].it.setPose(np);
      }
    }
  }

  // Health slowly regenerates, so a Sumo bout is a sustained brawl: you must land hits
  // faster than the foe heals, not just tag it once.
  if (_type == MatchType::SUMO && _ticks % SUMO_REGEN_TICKS == 0)
    for (int i = 0; i < 2; i++)
      if (_bot[i].alive && _bot[i].hp < SUMO_HP) _bot[i].hp++;

  foldLog();

  if (_type == MatchType::SOCCER) {
    // Timed match: never resolves mid-play (a goal just re-kicks off). The cap decides it below.
  } else if (_type == MatchType::RACE) {
    if (win[0] && win[1]) { _outcome = ArenaOutcome::DRAW; }
    else if (win[0]) { _bot[0].won = true; _outcome = ArenaOutcome::BOT0; }
    else if (win[1]) { _bot[1].won = true; _outcome = ArenaOutcome::BOT1; }
  } else {  // SUMO: last bot standing wins (SPEC §18.2)
    bool a0 = _bot[0].alive, a1 = _bot[1].alive;
    if (!a0 && !a1) _outcome = ArenaOutcome::DRAW;
    else if (!a1) { _bot[0].won = true; _outcome = ArenaOutcome::BOT0; }
    else if (!a0) { _bot[1].won = true; _outcome = ArenaOutcome::BOT1; }
  }
  if (_outcome != ArenaOutcome::RUNNING) return _outcome;

  // Soccer tie-break fuel: every tick, credit the side whose target net the ball is nearer to (it's
  // pressing). Summed over the whole match this is almost never an exact tie, so a level scoreline
  // still produces a winner instead of a draw.
  if (_type == MatchType::SOCCER) {
    auto bd = [&](int i) { int dr = _ball.row - _goal[i].row, dc = _ball.col - _goal[i].col;
                           return (dr < 0 ? -dr : dr) + (dc < 0 ? -dc : dc); };
    _pressure += bd(1) - bd(0);   // >0: ball nearer P1's target net all match (P1 pressed). Magnitude,
                                  // not just sign -> a sustained bias almost never cancels to an exact tie.
  }

  // Time's up (or neither can move). Race draws; Sumo breaks the tie by RING CONTROL -- the
  // bot nearer the arena centre wins (real sumo never just stops at a draw). Exact tie -> draw.
  bool active0 = _bot[0].alive && !_bot[0].done && !_bot[0].it.finished();
  bool active1 = _bot[1].alive && !_bot[1].done && !_bot[1].it.finished();
  bool over = (!active0 && !active1) || _ticks >= _stepCap;
  if (over && _outcome == ArenaOutcome::RUNNING) {
    if (_type == MatchType::SOCCER) {
      // Full time: higher SCORE wins. Level on goals -> the side that PRESSED more all match (the
      // accumulated tie-break above); still exactly level -> the final ball position; only then a draw.
      if (_goals[0] != _goals[1]) {
        int w = _goals[0] > _goals[1] ? 0 : 1;
        _bot[w].won = true; _outcome = w ? ArenaOutcome::BOT1 : ArenaOutcome::BOT0;
      } else if (_pressure != 0) {
        int w = _pressure > 0 ? 0 : 1;
        _bot[w].won = true; _outcome = w ? ArenaOutcome::BOT1 : ArenaOutcome::BOT0;
      } else {
        auto bd = [&](int i) {
          int dr = _ball.row - _goal[i].row, dc = _ball.col - _goal[i].col;
          return (dr < 0 ? -dr : dr) + (dc < 0 ? -dc : dc);
        };
        int d0 = bd(0), d1 = bd(1);
        if (d0 < d1) { _bot[0].won = true; _outcome = ArenaOutcome::BOT0; }
        else if (d1 < d0) { _bot[1].won = true; _outcome = ArenaOutcome::BOT1; }
        else _outcome = ArenaOutcome::DRAW;
      }
    } else if (_type == MatchType::SUMO && _bot[0].alive && _bot[1].alive) {
      int cr = _maze->rows() / 2, cc = _maze->cols() / 2;
      auto dist = [&](int i) {
        const Pose& p = _bot[i].it.pose();
        int dr = p.row - cr, dc = p.col - cc;
        return (dr < 0 ? -dr : dr) + (dc < 0 ? -dc : dc);
      };
      int d0 = dist(0), d1 = dist(1);
      if (d0 < d1) { _bot[0].won = true; _outcome = ArenaOutcome::BOT0; }
      else if (d1 < d0) { _bot[1].won = true; _outcome = ArenaOutcome::BOT1; }
      else _outcome = ArenaOutcome::DRAW;
    } else {
      _outcome = ArenaOutcome::DRAW;
    }
  }
  return _outcome;
}

ArenaOutcome Arena::run() {
  while (_outcome == ArenaOutcome::RUNNING) tick();
  return _outcome;
}

}  // namespace gb
