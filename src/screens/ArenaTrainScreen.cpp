#include "screens/ArenaTrainScreen.h"
#include "hal/Audio.h"
#include "assets/Assets.h"
#include "game/MazeGen.h"
#include "game/Interpreter.h"
#include "game/Arena.h"
#include "game/Score.h"
#include "game/Distill.h"
#include "game/Bots.h"

using namespace ui;
using namespace gb;

namespace screens {

static const Rect R_OPP   = {6, (int16_t)(BAND_Y + 2), 200, 18};  // tap to change sparring partner
static const Rect R_TEACH = {6,   (int16_t)(BOTBAR_Y + 2), 70, 32};
static const Rect R_EVO   = {80,  (int16_t)(BOTBAR_Y + 2), 78, 32};
static const Rect R_SAVE  = {162, (int16_t)(BOTBAR_Y + 2), 88, 32};
static const Rect R_BACK  = {254, (int16_t)(BOTBAR_Y + 2), 60, 32};
static constexpr int N_HOUSE_OPP = 5;  // Bolt, Coil, Spin, Vex, Ace (easy -> hard)

// The sparring roster = house bots + every bot in your library (incl. radio-traded
// friends' bots and fighters you saved). Train against code OR neuro opponents.
int ArenaTrainScreen::oppCount() const {
  return N_HOUSE_OPP + (_profile ? (int)_profile->library.size() : 0);
}

void ArenaTrainScreen::buildOpponent(int idx) {
  _ai.clear();
  // Ordered EASY -> HARD so a kid's first Teach reliably beats the default (Bolt), for the
  // satisfying "I taught it and it WINS!"; cycle up through navigators and a hunter to Ace
  // (a perfect solver) for a real challenge — five named partners before your own library.
  if (idx == 0) {                  // "Bolt": a blind dasher (easy first win)
    _oppName = "Bolt"; _ai = alwaysForwardProgram();
  } else if (idx == 1) {           // "Coil": hugs the LEFT wall (a real, beatable navigator)
    _oppName = "Coil"; _ai = wallFollowerProgram();
  } else if (idx == 2) {           // "Spin": hugs the RIGHT wall (a different route)
    _oppName = "Spin"; _ai = wallFollowerRightProgram();
  } else if (idx == 3) {           // "Vex": hunts and zaps (a Sumo-style aggressor)
    _oppName = "Vex"; _ai = hunterProgram();
  } else if (idx == 4) {           // "Ace": a navigator that solves from its own start (hard)
    _oppName = "Ace";
    if (!solveMazeFrom(_maze, _s1, true, _ai)) {
      _ai.clear();
      Node loop = Node::repeatUntil(AT_GOAL); loop.body.push_back(Node::command(CMD_FWD)); _ai.main.push_back(loop);
    }
  } else {                         // your library: code OR neuro, incl. radio-traded bots
    int li = idx - N_HOUSE_OPP;
    if (_profile && li >= 0 && li < (int)_profile->library.size()) {
      _oppName = _profile->library[li].name;
      _ai = _profile->library[li].program;
    } else { _oppName = "Ace"; }
  }
}

void ArenaTrainScreen::begin(Profile* profile) {
  _profile = profile;
  MazeGen::generateArena(_maze, profile ? profile->seedBase + 31u : 31u, _s0, _s1);
  _maze.setStart(_s0);  // the brain is bot 0, starting at s0
  _oppIdx = 0;
  buildOpponent(_oppIdx);
  _evo.init(SENSOR_COUNT_FOR_BRAIN, 8, 5, 23);
  _taught = false; _saved = false;
  evaluateAndTrace();
}

void ArenaTrainScreen::enter() { draw(); }

void ArenaTrainScreen::evaluateAndTrace() {
  if (!_taught) { _evo.evaluateArena(_maze, _s0, _s1, _ai, 200); _brain = _evo.best(); }
  // trace the brain's solo path
  Program bp; bp.brains.push_back(_brain);
  Node loop = Node::repeatUntil(AT_GOAL); loop.body.push_back(Node::neuro(0)); bp.main.push_back(loop);
  Interpreter it; it.load(&bp, &_maze, _s0, 64);
  _pathLen = 0;
  for (int s = 0; s < 64; s++) {
    Pose p = it.pose();
    if (_pathLen < 64) _path[_pathLen++] = (uint8_t)(p.row * _maze.cols() + p.col);
    if (it.finished()) break;
    it.step();
  }
  // a real match vs the AI for the verdict
  Program bp2; bp2.brains.push_back(_brain);
  Node l2 = Node::repeatUntil(AT_GOAL); l2.body.push_back(Node::neuro(0)); bp2.main.push_back(l2);
  Arena ar; ar.setup(&_maze, &bp2, &_ai, _s0, _s1, MatchType::RACE); ar.run();
  _beatsAI = (ar.outcome() == ArenaOutcome::BOT0);
}

void ArenaTrainScreen::mazeGeom(int& tile, int& ox, int& oy) const {
  const int reserve = 24;  // strip under the topbar for the opponent chip
  int cols = _maze.cols(), rows = _maze.rows();
  tile = (SCREEN_W - 20) / cols;
  int th = (BAND_H - 8 - reserve) / rows;
  if (th < tile) tile = th;
  ox = (SCREEN_W - tile * cols) / 2;
  oy = BAND_Y + reserve + (BAND_H - 8 - reserve - tile * rows) / 2;
}

void ArenaTrainScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Train a fighter", C_FUNC, textdatum_t::top_left, 2);
  char hd[28];
  if (_taught) snprintf(hd, sizeof(hd), "taught  %s", _beatsAI ? "wins!" : "vs");
  else         snprintf(hd, sizeof(hd), "gen %d  %s", _evo.gen, _beatsAI ? "wins!" : "vs");
  label(g, SCREEN_W - 6, 6, hd, _beatsAI ? C_GO : C_DIM, textdatum_t::top_right);

  // tappable sparring-partner chip: cycle through the roster (house bots + library)
  char chip[40]; snprintf(chip, sizeof(chip), "spar vs: %s  >", _oppName.c_str());
  button(g, R_OPP, chip, ui::rgb(120, 230, 245), C_PANEL);

  int tile, ox, oy; mazeGeom(tile, ox, oy);
  for (int r = 0; r < _maze.rows(); r++)
    for (int c = 0; c < _maze.cols(); c++) {
      int x = ox + c * tile, y = oy + r * tile;
      Tile t = _maze.at(r, c);
      uint16_t col = ((r + c) & 1) ? C_FLOOR : C_FLOOR2;
      if (t == WALL) col = C_WALL; else if (t == PIT) col = C_BG;
      g.fillRect(x, y, tile - 1, tile - 1, col);
      if (_maze.isGoal(r, c)) assets::drawGoalToken(g, x + tile / 2, y + tile / 2, tile, 0);
    }
  // the AI opponent's start (red) and your brain's path (blue/green)
  g.drawRect(ox + _s1.col * tile, oy + _s1.row * tile, tile - 1, tile - 1, C_BAD);
  for (int i = 0; i < _pathLen; i++) {
    int r = _path[i] / _maze.cols(), c = _path[i] % _maze.cols();
    g.fillCircle(ox + c * tile + tile / 2, oy + r * tile + tile / 2, tile / 6 + 1,
                 _beatsAI ? C_GO : C_MOVE);
  }

  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_TEACH, "Teach", C_GO, C_PANEL);
  button(g, R_EVO, "Evolve", C_FUNC, C_PANEL);
  button(g, R_SAVE, _saved ? "saved!" : "Save fighter", _saved ? C_DIM : C_ACCENT, C_PANEL);
  button(g, R_BACK, "Back", C_INK, C_PANEL);
}

app::Signal ArenaTrainScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  if (R_OPP.contains(tx, ty)) {
    _oppIdx = (_oppIdx + 1) % oppCount();  // next sparring partner
    buildOpponent(_oppIdx);
    _saved = false; evaluateAndTrace(); hal::audio.blip(); draw();
  } else if (R_TEACH.contains(tx, ty)) {
    distillSolver(_brain, _maze, true, 700);  // a strong racer beats most AIs
    _taught = true; _saved = false; evaluateAndTrace(); hal::audio.blip(); draw();
  } else if (R_EVO.contains(tx, ty)) {
    for (int gg = 0; gg < 3; gg++) { _evo.breed(); _evo.evaluateArena(_maze, _s0, _s1, _ai, 200); }
    _taught = false; _saved = false; evaluateAndTrace(); hal::audio.blip(); draw();
  } else if (R_SAVE.contains(tx, ty)) {
    if (_profile && _profile->library.size() < (size_t)LIBRARY_MAX) {
      LibEntry e; e.name = "Brainy";
      e.program.brains.push_back(_brain);
      Node loop = Node::repeatUntil(AT_GOAL); loop.body.push_back(Node::neuro(0)); e.program.main.push_back(loop);
      _profile->library.push_back(e);
      _saved = true; hal::audio.badge(); draw();
    } else { hal::audio.fail(); }
  } else if (R_BACK.contains(tx, ty)) {
    return app::Signal::BACK;
  }
  return app::Signal::NONE;
}

}  // namespace screens
