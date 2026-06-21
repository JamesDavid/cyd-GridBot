#include "screens/NeuroTrainScreen.h"
#include "hal/Audio.h"
#include "assets/Assets.h"
#include "game/Interpreter.h"
#include "game/Distill.h"
#include "game/Gauntlet.h"
#include "game/Pilot.h"
#include "game/Achievements.h"

using namespace ui;
using namespace gb;

namespace screens {

static const Rect R_BASE  = {6,   (int16_t)(BAND_Y + 2), 118, 18};  // load a brain to fine-tune
static const Rect R_GAUNT = {128, (int16_t)(BAND_Y + 2), 90,  18};  // Generalist challenge trainer
static const Rect R_SAVEV = {222, (int16_t)(BAND_Y + 2), 92,  18};  // save a versioned copy
// normal toolbar (6): distill solver / draw a path / neuroevolve / planner+follow / save / leave
static const Rect R_TEACH = {4,   (int16_t)(BOTBAR_Y + 2), 48, 26};
static const Rect R_DRAW  = {54,  (int16_t)(BOTBAR_Y + 2), 44, 26};
static const Rect R_TRAIN = {100, (int16_t)(BOTBAR_Y + 2), 52, 26};
static const Rect R_PILOT = {154, (int16_t)(BOTBAR_Y + 2), 48, 26};
static const Rect R_USE   = {204, (int16_t)(BOTBAR_Y + 2), 44, 26};
static const Rect R_BACK  = {250, (int16_t)(BOTBAR_Y + 2), 64, 26};
// draw-mode toolbar: learn from the drawn path / clear it / leave draw mode
static const Rect R_LEARN  = {6,   (int16_t)(BOTBAR_Y + 2), 110, 26};
static const Rect R_DCLEAR = {122, (int16_t)(BOTBAR_Y + 2), 90, 26};
static const Rect R_DCANCEL= {218, (int16_t)(BOTBAR_Y + 2), 96, 26};

void NeuroTrainScreen::rebuildBrainLibs() {
  _brainLibs.clear();
  if (!_profile) return;
  for (int i = 0; i < (int)_profile->library.size(); i++)
    if (!_profile->library[i].program.brains.empty()) _brainLibs.push_back(i);
}

// Smallest free N for a "Brain vN" name so each saved copy gets a fresh version number.
int NeuroTrainScreen::nextVersion() const {
  int hi = 0;
  if (_profile)
    for (auto& e : _profile->library) {
      int v = 0;
      if (sscanf(e.name.c_str(), "Brain v%d", &v) == 1 && v > hi) hi = v;
    }
  return hi + 1;
}

// Load base _baseIdx into the working brain and seed evolution from it (transfer learning).
void NeuroTrainScreen::applyBase() {
  Net base;
  if (_baseIdx == 0) {  // the brain currently in the editor's block
    _baseName = "block";
    if (_prog && _idx < (int)_prog->brains.size()) base = _prog->brains[_idx];
    else base.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 17);
  } else {              // a saved library brain -> fine-tune on top of it
    int li = _brainLibs[_baseIdx - 1];
    _baseName = _profile->library[li].name;
    base = _profile->library[li].program.brains[0];
  }
  _evo.init(SENSOR_COUNT_FOR_BRAIN, 8, 5, 17);
  _evo.pop[0] = base;           // evolution continues FROM the base
  _brain = base;                // and Teach/trace start from it too
  _taught = false; _saved = false; _savedCopy = false; _gauntletScore = -1;
  tracePath();
}

void NeuroTrainScreen::begin(Profile* profile, Program* prog, int brainIdx, Maze* maze) {
  _profile = profile; _prog = prog; _idx = brainIdx; _maze = maze;
  rebuildBrainLibs();
  _baseIdx = 0;
  _pilotMode = false;   // reflect the program's existing brain block
  if (_prog) {
    NodeList* lists[3] = {&_prog->main, &_prog->f1, &_prog->f2};
    for (NodeList* lst : lists)
      for (Node& n : *lst) {
        if (n.type == N_NEURO && n.brainIdx == _idx && n.pilot) _pilotMode = true;
        for (Node& c : n.body)
          if (c.type == N_NEURO && c.brainIdx == _idx && c.pilot) _pilotMode = true;
      }
  }
  applyBase();
}

void NeuroTrainScreen::enter() { draw(); }

void NeuroTrainScreen::setNodePilot(bool on) {
  if (!_prog) return;
  NodeList* lists[3] = {&_prog->main, &_prog->f1, &_prog->f2};
  // walk each list (one level of nesting covers the brain's repeat-until wrapper)
  for (NodeList* lst : lists)
    for (Node& n : *lst) {
      if (n.type == N_NEURO && n.brainIdx == _idx) n.pilot = on;
      for (Node& c : n.body)
        if (c.type == N_NEURO && c.brainIdx == _idx) c.pilot = on;
    }
}

void NeuroTrainScreen::tracePath() {
  Program prog; prog.brains.push_back(_brain);
  Node loop = Node::repeatUntil(AT_GOAL);
  loop.body.push_back(_pilotMode ? Node::pilotBrain(0) : Node::neuro(0));
  prog.main.push_back(loop);
  Interpreter it; it.load(&prog, _maze, _maze->startPose(), 64);
  _pathLen = 0; _won = false;
  for (int s = 0; s < 64; s++) {
    Pose p = it.pose();
    if (_pathLen < 64) _path[_pathLen++] = (uint8_t)(p.row * _maze->cols() + p.col);
    if (it.finished()) { _won = (it.lastOutcome() == OUT_WIN); break; }
    it.step();
  }
}

void NeuroTrainScreen::mazeGeom(int& tile, int& ox, int& oy) const {
  const int reserve = 24;  // strip under the topbar for the base / save chips
  int cols = _maze->cols(), rows = _maze->rows();
  tile = (SCREEN_W - 20) / cols;
  int th = (BAND_H - 8 - reserve) / rows;
  if (th < tile) tile = th;
  ox = (SCREEN_W - tile * cols) / 2;
  oy = BAND_Y + reserve + (BAND_H - 8 - reserve - tile * rows) / 2;
}

void NeuroTrainScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);

  if (_drawMode) {
    label(g, 6, 3, "Draw the path", C_ACCENT, textdatum_t::top_left, 2);
    label(g, SCREEN_W - 6, 6, "tap start->goal", C_DIM, textdatum_t::top_right);
    label(g, 6, BAND_Y + 4, "tap tiles to walk/jump; tap last to undo", C_DIM);
  } else {
    label(g, 6, 3, "Train the brain", ui::rgb(120, 230, 245), textdatum_t::top_left, 2);
    char hd[36];
    if (_gauntletScore >= 0) {  // the Generalist challenge result takes over the header line
      bool won = _gauntletScore >= GAUNTLET_MAZES;
      snprintf(hd, sizeof(hd), "%s %d/%d", won ? "GENERALIST!" : "gauntlet",
               _gauntletScore, GAUNTLET_MAZES);
      label(g, SCREEN_W - 6, 6, hd, won ? C_GO : C_ACCENT, textdatum_t::top_right);
    } else if (_pilotMode) {
      snprintf(hd, sizeof(hd), "pilot  %s", _won ? "solves!" : "...");
      label(g, SCREEN_W - 6, 6, hd, _won ? C_GO : ui::rgb(255, 170, 60), textdatum_t::top_right);
    } else {
      if (_taught) snprintf(hd, sizeof(hd), "taught  %s", _won ? "solves!" : "...");
      else snprintf(hd, sizeof(hd), "gen %d  %s", _evo.gen, _won ? "solves!" : "...");
      label(g, SCREEN_W - 6, 6, hd, _won ? C_GO : C_DIM, textdatum_t::top_right);
    }

    // transfer-learning chips: pick a base brain to fine-tune, save a versioned copy
    char base[40]; snprintf(base, sizeof(base), "base: %s >", _baseName.c_str());
    button(g, R_BASE, base, ui::rgb(120, 230, 245), C_PANEL);
    button(g, R_GAUNT, "Generalize", C_ACCENT, C_PANEL);  // train+test for the Generalist prize
    char sv[20];
    if (_savedCopy) snprintf(sv, sizeof(sv), "saved!");
    else            snprintf(sv, sizeof(sv), "save v%d >", nextVersion());
    button(g, R_SAVEV, sv, _savedCopy ? C_DIM : C_ACCENT, C_PANEL);
  }

  int tile, ox, oy; mazeGeom(tile, ox, oy);
  for (int r = 0; r < _maze->rows(); r++)
    for (int c = 0; c < _maze->cols(); c++) {
      int x = ox + c * tile, y = oy + r * tile;
      Tile t = _maze->at(r, c);
      uint16_t col = ((r + c) & 1) ? C_FLOOR : C_FLOOR2;
      if (t == WALL) col = C_WALL; else if (t == PIT) col = C_BG;
      g.fillRect(x, y, tile - 1, tile - 1, col);
      if (_maze->isGoal(r, c)) assets::drawGoalToken(g, x + tile / 2, y + tile / 2, tile, 0);
    }

  if (_drawMode) {
    // the hand-drawn route: connected squares, numbered, start tile brightest
    for (int i = 0; i < _drawLen; i++) {
      int r = _drawPath[i] / _maze->cols(), c = _drawPath[i] % _maze->cols();
      int x = ox + c * tile, y = oy + r * tile;
      uint16_t col = (i == 0) ? C_GO : C_ACCENT;
      g.fillRoundRect(x + 2, y + 2, tile - 5, tile - 5, 3, col);
      if (i > 0 && tile >= 16) {
        char num[4]; snprintf(num, sizeof(num), "%d", i);
        label(g, x + tile / 2, y + tile / 2, num, C_BG, textdatum_t::middle_center);
      }
    }
  } else {
    for (int i = 0; i < _pathLen; i++) {
      int r = _path[i] / _maze->cols(), c = _path[i] % _maze->cols();
      g.fillCircle(ox + c * tile + tile / 2, oy + r * tile + tile / 2, tile / 6 + 1,
                   _won ? C_GO : C_MOVE);
    }
  }

  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  if (_drawMode) {
    button(g, R_LEARN, "Learn it", C_GO, C_PANEL);       // distill from the drawn path
    button(g, R_DCLEAR, "Clear", C_ACCENT, C_PANEL);
    button(g, R_DCANCEL, "Cancel", C_INK, C_PANEL);
  } else {
    button(g, R_TEACH, "Teach", C_GO, C_PANEL);          // distill the solver (reliable)
    button(g, R_DRAW, "Draw", C_ACCENT, C_PANEL);        // imitation learning from a drawn path
    button(g, R_TRAIN, "Evolve", C_FUNC, C_PANEL);       // neuroevolution (no teacher)
    button(g, R_PILOT, "Pilot", _pilotMode ? C_GO : ui::rgb(255, 170, 60), C_PANEL);  // planner + follower
    button(g, R_USE, _saved ? "saved!" : "Use it", _saved ? C_DIM : ui::rgb(120, 230, 245), C_PANEL);
    button(g, R_BACK, "Back", C_INK, C_PANEL);
  }
}

void NeuroTrainScreen::seedDrawStart() {
  Pose p = _maze->startPose();
  _drawLen = 0;
  _drawPath[_drawLen++] = (uint8_t)(p.row * _maze->cols() + p.col);
}

bool NeuroTrainScreen::tileAtPixel(int x, int y, int& r, int& c) const {
  int tile, ox, oy; mazeGeom(tile, ox, oy);
  if (x < ox || y < oy) return false;
  c = (x - ox) / tile; r = (y - oy) / tile;
  return r >= 0 && r < _maze->rows() && c >= 0 && c < _maze->cols();
}

void NeuroTrainScreen::handleDrawTap(int r, int c) {
  int t = r * _maze->cols() + c;
  if (_drawLen == 0) { seedDrawStart(); }
  if (_drawPath[_drawLen - 1] == t) {           // tap the tip again to undo (keep the start)
    if (_drawLen > 1) { _drawLen--; hal::audio.blip(); draw(); }
    return;
  }
  int last = _drawPath[_drawLen - 1];
  int lr = last / _maze->cols(), lc = last % _maze->cols();
  int dr = r - lr, dc = c - lc;
  if (dr != 0 && dc != 0) { hal::audio.fail(); return; }     // no diagonals
  int dist = (dr ? (dr < 0 ? -dr : dr) : (dc < 0 ? -dc : dc));
  Tile dest = _maze->at(r, c);
  bool ok = false;
  if (dist == 1) {
    ok = (dest != WALL && dest != PIT);                       // step onto safe ground
  } else if (dist == 2) {
    Tile mid = _maze->at(lr + dr / 2, lc + dc / 2);
    ok = (mid != WALL && dest != WALL && dest != PIT);        // jump: clear wall, land safe
  }
  if (!ok) { hal::audio.fail(); return; }
  if (_drawLen < (int)sizeof(_drawPath)) { _drawPath[_drawLen++] = (uint8_t)t; hal::audio.blip(); draw(); }
}

app::Signal NeuroTrainScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;

  if (_drawMode) {
    if (R_LEARN.contains(tx, ty)) {                 // distill the brain to copy the drawn path
      if (distillPath(_brain, *_maze, _drawPath, _drawLen, 700)) {
        _taught = true; _saved = false; _savedCopy = false; _gauntletScore = -1;
        _pilotMode = false; setNodePilot(false);
        _drawMode = false; tracePath(); hal::audio.badge();
      } else { hal::audio.fail(); }
      draw();
    } else if (R_DCLEAR.contains(tx, ty)) {
      seedDrawStart(); hal::audio.blip(); draw();
    } else if (R_DCANCEL.contains(tx, ty)) {
      _drawMode = false; hal::audio.blip(); draw();
    } else {
      int r, c;
      if (tileAtPixel(tx, ty, r, c)) handleDrawTap(r, c);
    }
    return app::Signal::NONE;
  }

  if (R_BASE.contains(tx, ty)) {  // cycle the transfer base: block brain -> saved brains
    _baseIdx = (_baseIdx + 1) % (1 + (int)_brainLibs.size());
    applyBase(); hal::audio.blip(); draw();
  } else if (R_GAUNT.contains(tx, ty)) {  // Generalist: train across the gauntlet, then test it
    gauntletTrain(_brain, 20);            // one batch (tap again to keep improving)
    _gauntletScore = gauntletRun(_brain); // score the FROZEN brain on the held-out test mazes
    _taught = false; _saved = false; _savedCopy = false;  // brain changed -> re-Use to save it
    _pilotMode = false; setNodePilot(false);
    if (_profile) {
      if ((uint8_t)_gauntletScore > _profile->stats.gauntletBest)
        _profile->stats.gauntletBest = (uint8_t)_gauntletScore;
      uint32_t before = _profile->achievements;
      _profile->achievements |= evaluateAchievements(*_profile);
      if ((_profile->achievements & ~before) & A_GENERALIST) hal::audio.badge();  // new prize!
      else hal::audio.blip();
    } else hal::audio.blip();
    tracePath(); draw();
  } else if (R_SAVEV.contains(tx, ty)) {  // save the fine-tuned brain as a new version
    if (_profile && (int)_profile->library.size() < LIBRARY_MAX) {
      LibEntry e;
      char nm[16]; snprintf(nm, sizeof(nm), "Brain v%d", nextVersion());
      e.name = nm;
      e.program.brains.push_back(_brain);
      Node loop = Node::repeatUntil(AT_GOAL); loop.body.push_back(Node::neuro(0)); e.program.main.push_back(loop);
      _profile->library.push_back(e);
      rebuildBrainLibs();  // the new copy is now loadable as a base
      _savedCopy = true; hal::audio.badge(); draw();
    } else { hal::audio.fail(); }
  } else if (R_TEACH.contains(tx, ty)) {  // distill the optimal solver into the brain (reliable)
    distillSolver(_brain, *_maze, true, 700);
    _taught = true; _saved = false; _savedCopy = false; _gauntletScore = -1;
    _pilotMode = false; setNodePilot(false);
    tracePath(); hal::audio.blip(); draw();
  } else if (R_DRAW.contains(tx, ty)) {   // hand-draw a path to learn from (imitation learning)
    _drawMode = true; seedDrawStart(); hal::audio.blip(); draw();
  } else if (R_PILOT.contains(tx, ty)) {  // planner + follower: learn to steer to waypoints
    pilotTrain(_brain, _profile ? _profile->seedBase : 0u, 14, 18);
    setNodePilot(true);                   // the program's brain block becomes a pilot
    _pilotMode = true; _taught = false; _saved = false; _savedCopy = false; _gauntletScore = -1;
    tracePath(); hal::audio.blip(); draw();
  } else if (R_TRAIN.contains(tx, ty)) {
    for (int gg = 0; gg < 5; gg++) { _evo.breed(); _evo.evaluate(*_maze, nullptr, 110); }
    _brain = _evo.best(); _taught = false; _saved = false; _savedCopy = false; _gauntletScore = -1;
    _pilotMode = false; setNodePilot(false);
    tracePath(); hal::audio.blip(); draw();
  } else if (R_USE.contains(tx, ty)) {
    if (_prog && _idx < (int)_prog->brains.size()) _prog->brains[_idx] = _brain;
    _saved = true; hal::audio.badge(); draw();
  } else if (R_BACK.contains(tx, ty)) {
    return app::Signal::BACK;
  }
  return app::Signal::NONE;
}

}  // namespace screens
