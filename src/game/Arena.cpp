#include "game/Arena.h"

namespace gb {

void Arena::setup(const Maze* maze, const Program* p0, const Program* p1,
                  const Pose& s0, const Pose& s1, MatchType type, int stepCap) {
  _maze = maze;
  _type = type;
  _stepCap = stepCap;
  _ticks = 0;
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
  _scored[0] = _scored[1] = false;
  foldLog();
}

void Arena::configSoccer(const Pose& ball, const Pose& goal0, const Pose& goal1) {
  _ball = ball;
  _goal[0] = goal0;
  _goal[1] = goal1;
  // Re-aim each bot's senses for soccer: the TARGET (slots 4-6) is the ball, and the "enemy"
  // bearing (slots 7-9) points at the goal it's attacking -- so the brain learns ball + goal.
  for (int i = 0; i < 2; i++) {
    _bot[i].enemy.pose = &_goal[i];
    _bot[i].enemy.target = &_ball;
  }
  foldLog();
}

// Bot `mover` has stepped onto the ball's tile -- try to shove the ball one tile further in the
// mover's facing. Off-board / wall / pit blocks the shove; landing on a goal tile scores. Returns
// false if the ball can't move (the caller then bounces the bot back -- you can't walk through it).
bool Arena::pushBall(int mover) {
  int dr, dc; facingDelta(_bot[mover].it.pose().facing, dr, dc);
  int nr = _ball.row + dr, nc = _ball.col + dc;
  bool intoGoal0 = (nr == _goal[0].row && nc == _goal[0].col);
  bool intoGoal1 = (nr == _goal[1].row && nc == _goal[1].col);
  // A goal tile is always scorable; otherwise the ball can only roll onto walkable floor.
  if (!intoGoal0 && !intoGoal1 && (!_maze->inBounds(nr, nc) || !_maze->isWalkable(nr, nc)))
    return false;
  _ball.row = (int8_t)nr; _ball.col = (int8_t)nc;
  if (intoGoal0) _scored[0] = true;
  else if (intoGoal1) _scored[1] = true;
  return true;
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
    fold((uint32_t)(_scored[0] ? 1 : 0)); fold((uint32_t)(_scored[1] ? 1 : 0));
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
      } else if (_type == MatchType::SUMO) {
        // Head-on (both charged the same tile): let one bot (alternating each tick) HOLD the
        // tile so they end up ADJACENT and start trading zaps next tick, instead of freezing a
        // tile apart forever. The alternation keeps it fair.
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
    for (int i = 0; i < 2; i++) {
      if (!moved[i] || !_bot[i].alive) continue;
      const Pose& p = _bot[i].it.pose();
      if (p.row == _ball.row && p.col == _ball.col)
        if (!pushBall(i)) _bot[i].it.setPose(before[i]);
    }
  }

  // 3) Attacks resolve AFTER moves (SPEC §18.1). A zap shoves the enemy one tile in
  // the zapper's facing; into a PIT / off-board = out (Sumo, SPEC §18.2). Soccer has no zap.
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

  if (_type == MatchType::SOCCER) {  // first bot to shove the ball into its goal wins
    if (_scored[0]) { _bot[0].won = true; _outcome = ArenaOutcome::BOT0; }
    else if (_scored[1]) { _bot[1].won = true; _outcome = ArenaOutcome::BOT1; }
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

  // Time's up (or neither can move). Race draws; Sumo breaks the tie by RING CONTROL -- the
  // bot nearer the arena centre wins (real sumo never just stops at a draw). Exact tie -> draw.
  bool active0 = _bot[0].alive && !_bot[0].done && !_bot[0].it.finished();
  bool active1 = _bot[1].alive && !_bot[1].done && !_bot[1].it.finished();
  bool over = (!active0 && !active1) || _ticks >= _stepCap;
  if (over && _outcome == ArenaOutcome::RUNNING) {
    if (_type == MatchType::SOCCER) {
      // Nobody scored in time -> the side whose goal the ball is nearer to wins (it was pressing).
      auto bd = [&](int i) {
        int dr = _ball.row - _goal[i].row, dc = _ball.col - _goal[i].col;
        return (dr < 0 ? -dr : dr) + (dc < 0 ? -dc : dc);
      };
      int d0 = bd(0), d1 = bd(1);
      if (d0 < d1) { _bot[0].won = true; _outcome = ArenaOutcome::BOT0; }
      else if (d1 < d0) { _bot[1].won = true; _outcome = ArenaOutcome::BOT1; }
      else _outcome = ArenaOutcome::DRAW;
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
