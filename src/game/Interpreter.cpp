#include "game/Interpreter.h"
#include "core/Util.h"
#include "game/Sensors.h"
#include "game/Net.h"
#include "game/Pilot.h"

namespace gb {

// Brain output index -> primitive command (the 5-action set; zap is arena-only).
static const Cmd kBrainAction[5] = {CMD_FWD, CMD_TURN_L, CMD_TURN_R, CMD_JUMP, CMD_FIRE};

void Interpreter::load(const Program* prog, const Maze* maze, const Pose& start,
                       int stepCap) {
  _prog = prog;
  _maze = maze;
  _pose = start;
  _stepCap = stepCap;
  _prim = 0;
  _finished = false;
  _last = OUT_OK;
  _current = nullptr;
  _wpPlanned = false; _wpN = 0; _wpIdx = 0;
  if (prog)  // clear any recurrent brain's memory so each run starts fresh
    for (size_t i = 0; i < prog->rbrains.size(); i++) prog->rbrains[i].resetState();
  _stack.clear();
  if (prog && !prog->main.empty()) push(F_SEQ, &prog->main);
  // An empty program that doesn't start on the goal will end as DONE_NO_WIN.
}

void Interpreter::push(FrameKind k, const NodeList* body, uint8_t reps, Cond cond) {
  Frame f;
  f.body = body;
  f.ip = 0;
  f.kind = k;
  f.repsLeft = reps;
  f.cond = cond;
  _stack.push_back(f);
}

bool Interpreter::evalCond(Cond c) const {
  int dr, dc;
  facingDelta(_pose.facing, dr, dc);
  int ar = _pose.row + dr, ac = _pose.col + dc;
  switch (c) {
    case WALL_AHEAD:  return !_maze->inBounds(ar, ac) || _maze->at(ar, ac) == WALL;
    case PIT_AHEAD:   return _maze->at(ar, ac) == PIT;
    case AT_GOAL:     return _maze->isGoal(_pose.row, _pose.col);
    case ENEMY_AHEAD: return _enemy && _enemy->pose &&
                             _enemy->pose->row == ar && _enemy->pose->col == ac;
    case ENEMY_NEAR:  return _enemy && _enemy->pose &&
                             (absv(_enemy->pose->row - _pose.row) +
                              absv(_enemy->pose->col - _pose.col)) <= _enemy->nearDist;
    case BLOCKED_AHEAD: return !_maze->inBounds(ar, ac) ||
                               _maze->at(ar, ac) == WALL || _maze->at(ar, ac) == PIT;
    case ENEMY_LEFT:
    case ENEMY_RIGHT: {
      if (!_enemy || !_enemy->pose) return false;
      // ego-relative bearing: right vector = facing rotated 90° CW = (dc, -dr)
      int er = _enemy->pose->row - _pose.row, ec = _enemy->pose->col - _pose.col;
      int side = er * dc - ec * dr;        // >0 => foe to the right, <0 => to the left
      return (c == ENEMY_RIGHT) ? side > 0 : side < 0;
    }
  }
  return false;
}

Outcome Interpreter::move(int dr, int dc) {
  int nr = _pose.row + dr, nc = _pose.col + dc;
  if (!_maze->inBounds(nr, nc)) return OUT_BONK;
  Tile t = _maze->at(nr, nc);
  if (t == WALL) return OUT_BONK;
  if (t == PIT) return OUT_FELL;
  _pose.row = (int8_t)nr;
  _pose.col = (int8_t)nc;
  if (_maze->isGoal(nr, nc)) return OUT_WIN;
  return OUT_OK;
}

Outcome Interpreter::jump() {
  int dr, dc;
  facingDelta(_pose.facing, dr, dc);
  int mr = _pose.row + dr, mc = _pose.col + dc;            // intermediate
  int lr = _pose.row + 2 * dr, lc = _pose.col + 2 * dc;    // landing
  if (!_maze->inBounds(lr, lc)) return OUT_BONK;
  if (_maze->at(mr, mc) == WALL) return OUT_BONK;          // can't jump through a wall
  Tile land = _maze->at(lr, lc);
  if (land == WALL) return OUT_BONK;
  if (land == PIT) return OUT_FELL;
  _pose.row = (int8_t)lr;
  _pose.col = (int8_t)lc;
  if (_maze->isGoal(lr, lc)) return OUT_WIN;
  return OUT_OK;
}

Outcome Interpreter::execCmd(Cmd cmd) {
  int dr, dc;
  switch (cmd) {
    case CMD_TURN_L: _pose.facing = turnLeft(_pose.facing);  return OUT_OK;
    case CMD_TURN_R: _pose.facing = turnRight(_pose.facing); return OUT_OK;
    case CMD_FWD:    facingDelta(_pose.facing, dr, dc); return move(dr, dc);
    case CMD_BACK:   facingDelta(_pose.facing, dr, dc); return move(-dr, -dc);
    case CMD_JUMP:   return jump();
    case CMD_FIRE:   return OUT_OK;  // "zap" — arena verb, resolved by the match engine (§18)
  }
  return OUT_OK;
}

Outcome Interpreter::step() {
  if (_finished) return _last;
  _lastCmd = CMD_FWD;  // default: a step that runs no primitive is "not an attack"

  // Internal guard: a single step() must terminate even if blocks push frames with
  // no primitive (e.g. REPEAT 0, IF false). Bounds the per-step frame churn.
  int guard = 0;
  while (true) {
    if (++guard > 200000) { finish(OUT_DONE_NO_WIN); return OUT_DONE_NO_WIN; }

    if (_stack.empty()) {
      Outcome o = _maze->isGoal(_pose.row, _pose.col) ? OUT_WIN : OUT_DONE_NO_WIN;
      finish(o);
      return o;
    }

    Frame& fr = _stack.back();
    if (fr.ip >= fr.body->size()) {
      // Body finished — handle loop continuation, else pop.
      if (fr.kind == F_REPEAT) {
        if (fr.repsLeft > 1) { fr.repsLeft--; fr.ip = 0; continue; }
      } else if (fr.kind == F_REPEAT_UNTIL) {
        if (!evalCond(fr.cond)) { fr.ip = 0; continue; }
      }
      _stack.pop_back();
      continue;
    }

    const Node& n = (*fr.body)[fr.ip];
    switch (n.type) {
      case N_CMD: {
        fr.ip++;  // consume before executing (fr may dangle after this loop turn)
        if (++_prim > _stepCap) { finish(OUT_DONE_NO_WIN); return OUT_DONE_NO_WIN; }
        _current = &n;
        _lastCmd = n.cmd;
        Outcome o = execCmd(n.cmd);
        if (o != OUT_OK) { finish(o); return o; }
        return OUT_OK;  // exactly one primitive executed
      }
      case N_REPEAT: {
        fr.ip++;
        if (n.count > 0 && !n.body.empty()) push(F_REPEAT, &n.body, n.count);
        continue;
      }
      case N_REPEAT_UNTIL: {
        Cond c = n.cond;
        const NodeList* body = &n.body;
        fr.ip++;
        if (!body->empty() && !evalCond(c)) push(F_REPEAT_UNTIL, body, 0, c);
        continue;
      }
      case N_IF: {
        Cond c = n.cond;
        const NodeList* body = &n.body;
        fr.ip++;
        if (!body->empty() && evalCond(c)) push(F_SEQ, body);
        continue;
      }
      case N_CALL: {
        uint8_t f = n.func;
        fr.ip++;
        const NodeList* body = (f == 1) ? &_prog->f1 : &_prog->f2;
        // Skip the call once nesting gets pathological (recursive F1/F2) so the
        // stack can't blow the heap; the step cap then ends the run cleanly.
        if (!body->empty() && (int)_stack.size() < MAX_FRAME_DEPTH) push(F_SEQ, body);
        continue;
      }
      case N_NEURO: {
        // a learned reactive policy: sense -> brain -> argmax -> one action
        fr.ip++;
        if (++_prim > _stepCap) { finish(OUT_DONE_NO_WIN); return OUT_DONE_NO_WIN; }
        _current = &n;
        // pick the brain store: recurrent (memory) or feedforward
        bool useRnn = n.rnn && _prog && n.brainIdx < _prog->rbrains.size();
        if (!_prog || (!useRnn && n.brainIdx >= _prog->brains.size())) return OUT_OK;  // none: no-op
        float s[SENSOR_COUNT];
        if (n.pilot) {
          // Planner + follower: get the route once, then steer toward the next corner. The kid's
          // hand-placed waypoints win; only if there are none do we auto-plan the BFS route.
          if (!_wpPlanned) {
            if (_prog && !_prog->waypoints.empty()) {
              _wpN = (int)_prog->waypoints.size(); if (_wpN > 40) _wpN = 40;
              for (int i = 0; i < _wpN; i++) _wp[i] = _prog->waypoints[i];
            } else {
              _wpN = planWaypoints(*_maze, _pose, _wp, 40);
            }
            _wpIdx = 0; _wpPlanned = true;
          }
          int cur = _pose.row * _maze->cols() + _pose.col;
          while (_wpIdx < _wpN && _wp[_wpIdx] == cur) _wpIdx++;   // reached this waypoint
          int tr = _maze->goalRow(), tc = _maze->goalCol();
          if (_wpIdx < _wpN) { tr = _wp[_wpIdx] / _maze->cols(); tc = _wp[_wpIdx] % _maze->cols(); }
          senseEgoTo(*_maze, _pose, _enemy, tr, tc, s);
        } else {
          senseEgo(*_maze, _pose, _enemy, s);
        }
        int act = useRnn ? _prog->rbrains[n.brainIdx].argmaxStep(s)
                         : _prog->brains[n.brainIdx].argmax(s);
        _lastCmd = kBrainAction[act];
        Outcome o = execCmd(_lastCmd);
        if (o != OUT_OK) { finish(o); return o; }
        return OUT_OK;  // exactly one primitive executed
      }
    }
  }
}

Outcome Interpreter::runToEnd() {
  while (!_finished) step();
  return _last;
}

}  // namespace gb
