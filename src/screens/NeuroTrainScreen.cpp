#include "screens/NeuroTrainScreen.h"
#include "hal/Audio.h"
#include "assets/Assets.h"
#include "game/Interpreter.h"
#include "game/Distill.h"

using namespace ui;
using namespace gb;

namespace screens {

static const Rect R_BASE  = {6,   (int16_t)(BAND_Y + 2), 176, 18};  // load a brain to fine-tune
static const Rect R_SAVEV = {186, (int16_t)(BAND_Y + 2), 128, 18};  // save a versioned copy
static const Rect R_TEACH = {6,   (int16_t)(BOTBAR_Y + 2), 72, 26};
static const Rect R_TRAIN = {82,  (int16_t)(BOTBAR_Y + 2), 80, 26};
static const Rect R_USE   = {166, (int16_t)(BOTBAR_Y + 2), 80, 26};
static const Rect R_BACK  = {250, (int16_t)(BOTBAR_Y + 2), 64, 26};

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
  _taught = false; _saved = false; _savedCopy = false;
  tracePath();
}

void NeuroTrainScreen::begin(Profile* profile, Program* prog, int brainIdx, Maze* maze) {
  _profile = profile; _prog = prog; _idx = brainIdx; _maze = maze;
  rebuildBrainLibs();
  _baseIdx = 0;
  applyBase();
}

void NeuroTrainScreen::enter() { draw(); }

void NeuroTrainScreen::tracePath() {
  Program prog; prog.brains.push_back(_brain);
  Node loop = Node::repeatUntil(AT_GOAL);
  loop.body.push_back(Node::neuro(0));
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
  label(g, 6, 3, "Train the brain", ui::rgb(120, 230, 245), textdatum_t::top_left, 2);
  char hd[28];
  if (_taught) snprintf(hd, sizeof(hd), "taught  %s", _won ? "solves!" : "...");
  else snprintf(hd, sizeof(hd), "gen %d  %s", _evo.gen, _won ? "solves!" : "...");
  label(g, SCREEN_W - 6, 6, hd, _won ? C_GO : C_DIM, textdatum_t::top_right);

  // transfer-learning chips: pick a base brain to fine-tune, save a versioned copy
  char base[40]; snprintf(base, sizeof(base), "base: %s  >", _baseName.c_str());
  button(g, R_BASE, base, ui::rgb(120, 230, 245), C_PANEL);
  char sv[20];
  if (_savedCopy) snprintf(sv, sizeof(sv), "saved!");
  else            snprintf(sv, sizeof(sv), "save v%d  >", nextVersion());
  button(g, R_SAVEV, sv, _savedCopy ? C_DIM : C_ACCENT, C_PANEL);

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
  for (int i = 0; i < _pathLen; i++) {
    int r = _path[i] / _maze->cols(), c = _path[i] % _maze->cols();
    g.fillCircle(ox + c * tile + tile / 2, oy + r * tile + tile / 2, tile / 6 + 1,
                 _won ? C_GO : C_MOVE);
  }

  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_TEACH, "Teach", C_GO, C_PANEL);       // distill the solver (reliable)
  button(g, R_TRAIN, "Evolve", C_FUNC, C_PANEL);    // neuroevolution (no teacher)
  button(g, R_USE, _saved ? "saved!" : "Use it", _saved ? C_DIM : ui::rgb(120, 230, 245), C_PANEL);
  button(g, R_BACK, "Back", C_INK, C_PANEL);
}

app::Signal NeuroTrainScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  if (R_BASE.contains(tx, ty)) {  // cycle the transfer base: block brain -> saved brains
    _baseIdx = (_baseIdx + 1) % (1 + (int)_brainLibs.size());
    applyBase(); hal::audio.blip(); draw();
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
    _taught = true; _saved = false; _savedCopy = false;
    tracePath(); hal::audio.blip(); draw();
  } else if (R_TRAIN.contains(tx, ty)) {
    for (int gg = 0; gg < 5; gg++) { _evo.breed(); _evo.evaluate(*_maze, nullptr, 110); }
    _brain = _evo.best(); _taught = false; _saved = false; _savedCopy = false;
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
