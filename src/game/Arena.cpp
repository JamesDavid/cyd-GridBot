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
  }
  _bot[0].it.load(p0, maze, s0, stepCap);
  _bot[1].it.load(p1, maze, s1, stepCap);
  // ENEMY_* conditions read the live opponent pose (SPEC §18.3).
  _bot[0].enemy.pose = &_bot[1].it.pose();
  _bot[1].enemy.pose = &_bot[0].it.pose();
  _bot[0].it.setEnemy(&_bot[0].enemy);
  _bot[1].it.setEnemy(&_bot[1].enemy);
  foldLog();
}

void Arena::foldLog() {
  // Fingerprint the full per-tick state so the determinism test is exact.
  auto fold = [&](uint32_t v) { _hash = (_hash ^ v) * 16777619u; };
  for (int i = 0; i < 2; i++) {
    fold((uint32_t)_bot[i].it.pose().row);
    fold((uint32_t)_bot[i].it.pose().col);
    fold((uint32_t)_bot[i].it.pose().facing);
    fold((uint32_t)(_bot[i].alive ? 1 : 0));
  }
}

ArenaOutcome Arena::tick() {
  if (_outcome != ArenaOutcome::RUNNING) return _outcome;
  _ticks++;

  Pose before[2];
  Outcome out[2] = {OUT_OK, OUT_OK};
  bool moved[2] = {false, false};
  bool pushing[2] = {false, false};
  for (int i = 0; i < 2; i++) {
    before[i] = _bot[i].it.pose();
    if (_bot[i].alive && !_bot[i].done && !_bot[i].it.finished()) {
      out[i] = _bot[i].it.step();
      moved[i] = true;
      const Node* n = _bot[i].it.currentNode();
      pushing[i] = (n && n->type == N_CMD && n->cmd == CMD_PUSH);
    }
  }

  // Resolution pass (SPEC §18.1), moves first.
  // 1) Both bots target the SAME tile -> both bounce back.
  if (_bot[0].alive && _bot[1].alive) {
    const Pose& a = _bot[0].it.pose();
    const Pose& b = _bot[1].it.pose();
    if (a.row == b.row && a.col == b.col) {
      _bot[0].it.setPose(before[0]);
      _bot[1].it.setPose(before[1]);
      for (int i = 0; i < 2; i++) if (out[i] == OUT_WIN || out[i] == OUT_FELL) out[i] = OUT_OK;
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

  // 3) Attacks resolve AFTER moves (SPEC §18.1). PUSH shoves the enemy one tile in
  // the pusher's facing; into a PIT / off-board = out (Sumo, SPEC §18.2).
  for (int i = 0; i < 2; i++) {
    if (!pushing[i] || !_bot[i].alive) continue;
    int j = 1 - i;
    if (!_bot[j].alive) continue;
    int dr, dc; facingDelta(_bot[i].it.pose().facing, dr, dc);
    int ar = _bot[i].it.pose().row + dr, ac = _bot[i].it.pose().col + dc;
    const Pose& ej = _bot[j].it.pose();
    if (ej.row == ar && ej.col == ac) {  // enemy is in the tile ahead -> shove it
      int dest_r = ar + dr, dest_c = ac + dc;
      if (!_maze->inBounds(dest_r, dest_c) || _maze->at(dest_r, dest_c) == PIT) {
        _bot[j].alive = false;
      } else if (_maze->isWalkable(dest_r, dest_c)) {
        Pose np = ej; np.row = (int8_t)dest_r; np.col = (int8_t)dest_c;
        _bot[j].it.setPose(np);
      }
    }
  }

  foldLog();

  if (_type == MatchType::RACE) {
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

  // Both unable to continue -> draw (step cap or both out/done).
  bool active0 = _bot[0].alive && !_bot[0].done && !_bot[0].it.finished();
  bool active1 = _bot[1].alive && !_bot[1].done && !_bot[1].it.finished();
  if (!active0 && !active1) _outcome = ArenaOutcome::DRAW;
  if (_ticks >= _stepCap) _outcome = ArenaOutcome::DRAW;
  return _outcome;
}

ArenaOutcome Arena::run() {
  while (_outcome == ArenaOutcome::RUNNING) tick();
  return _outcome;
}

}  // namespace gb
