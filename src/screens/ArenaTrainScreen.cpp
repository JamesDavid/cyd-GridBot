#include "screens/ArenaTrainScreen.h"
#include <Arduino.h>   // millis() -> vary the practice ring per session
#include "hal/Audio.h"
#include "assets/Assets.h"
#include "game/MazeGen.h"
#include "game/Interpreter.h"
#include "game/Arena.h"
#include "game/Score.h"
#include "game/Distill.h"
#include "game/Bots.h"
#include "game/Sensors.h"
#include "store/ProfileStore.h"
#include "screens/BrainGraph.h"

using namespace ui;
using namespace gb;

namespace screens {

static const Rect R_OPP   = {6,   (int16_t)(BAND_Y + 2), 120, 18}; // tap to change sparring partner
static const Rect R_MODE  = {130, (int16_t)(BAND_Y + 2), 76,  18}; // Race <-> Sumo
static const Rect R_VIEW  = {212, (int16_t)(BAND_Y + 2), 102, 18}; // toggle arena <-> brain view
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

std::string ArenaTrainScreen::nextFighterName() const {
  int hi = 0;
  if (_profile)
    for (auto& e : _profile->library) { int v = 0; if (sscanf(e.name.c_str(), "Fighter v%d", &v) == 1 && v > hi) hi = v; }
  char nm[16]; snprintf(nm, sizeof(nm), "Fighter v%d", hi + 1);
  return nm;
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

void ArenaTrainScreen::setupBoard() {
  if (_matchType == MatchType::SUMO) {
    // Practice on the SAME kind of ring you'll battle on (the big open Sumo ring), and vary it
    // per session so the fighter learns to HUNT in general rather than over-fitting one board
    // -- otherwise a "trained winner" loses on the random battle ring.
    uint32_t seed = (_profile ? _profile->seedBase : 31u) + (uint32_t)millis();
    MazeGen::generateSumoRing(_maze, seed, _s0, _s1);
  } else {
    MazeGen::generateArena(_maze, _profile ? _profile->seedBase + 31u : 31u, _s0, _s1);
  }
  _maze.setStart(_s0);  // the brain is bot 0, starting at s0
}

void ArenaTrainScreen::begin(Profile* profile) {
  _profile = profile;
  _matchType = MatchType::RACE;
  setupBoard();
  _oppIdx = 0;
  buildOpponent(_oppIdx);
  _evo.init(SENSOR_COUNT_FOR_BRAIN, 8, 5, 23);
  _taught = false; _saved = false; _savedIdx = -1;
  evaluateAndTrace();
}

// Switch Race <-> Sumo: different board (Sumo has no goal), different fitness, fresh training.
void ArenaTrainScreen::setMode(MatchType t) {
  _matchType = t;
  _animating = false;
  setupBoard();
  buildOpponent(_oppIdx);
  _evo.init(SENSOR_COUNT_FOR_BRAIN, 8, 5, 23);
  _taught = false; _saved = false; _savedIdx = -1;
  evaluateAndTrace();
}

void ArenaTrainScreen::enter() { draw(); }

void ArenaTrainScreen::evaluateAndTrace() {
  if (!_taught) { _evo.evaluateArena(_maze, _s0, _s1, _ai, 200, _matchType); _brain = _evo.best(); }
  Program bp; bp.brains.push_back(_brain);
  Node loop = Node::repeatUntil(AT_GOAL); loop.body.push_back(Node::neuro(0)); bp.main.push_back(loop);
  int cols = _maze.cols();

  if (_matchType == MatchType::SUMO) {
    // Trace the REAL fight: step both bots so you see them hunt and shove (not solo wandering).
    Arena ar; ar.setup(&_maze, &bp, &_ai, _s0, _s1, MatchType::SUMO, 80);
    _pathLen = 0; _oppLen = 0;
    for (int s = 0; s < 80; s++) {
      Pose a = ar.pose(0), b = ar.pose(1);
      if (_pathLen < 64) _path[_pathLen++] = (uint8_t)(a.row * cols + a.col);
      if (_oppLen  < 64) _oppPath[_oppLen++] = (uint8_t)(b.row * cols + b.col);
      if (ar.tick() != ArenaOutcome::RUNNING) break;
    }
    _beatsAI = (ar.outcome() == ArenaOutcome::BOT0);
  } else {
    // RACE: the brain's solo run to the goal + the opponent's solo run, then a verdict match.
    Interpreter it; it.load(&bp, &_maze, _s0, 64);
    _pathLen = 0;
    for (int s = 0; s < 64; s++) {
      Pose p = it.pose();
      if (_pathLen < 64) _path[_pathLen++] = (uint8_t)(p.row * cols + p.col);
      if (it.finished()) break;
      it.step();
    }
    Interpreter oit; oit.load(&_ai, &_maze, _s1, 64);
    _oppLen = 0;
    for (int s = 0; s < 64; s++) {
      Pose p = oit.pose();
      if (_oppLen < 64) _oppPath[_oppLen++] = (uint8_t)(p.row * cols + p.col);
      if (oit.finished()) break;
      oit.step();
    }
    Arena ar; ar.setup(&_maze, &bp, &_ai, _s0, _s1, MatchType::RACE); ar.run();
    _beatsAI = (ar.outcome() == ArenaOutcome::BOT0);
  }
  inferBrain();   // refresh the network-graph activations to match the (re)trained brain
}

void ArenaTrainScreen::inferBrain() {
  senseEgo(_maze, _s0, nullptr, _in);
  _brain.forward(_in, _out, _hid);
  _action = 0;
  for (int k = 1; k < 5; k++) if (_out[k] > _out[_action]) _action = k;
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

  // tappable sparring-partner chip (arena view only — the mini-map takes that space in net view)
  if (!_netView) {
    char chip[32]; snprintf(chip, sizeof(chip), "vs %s >", _oppName.c_str());
    button(g, R_OPP, chip, ui::rgb(120, 230, 245), C_PANEL);
  }
  button(g, R_MODE, _matchType == MatchType::SUMO ? "Sumo >" : "Race >", C_FUNC, C_PANEL);
  button(g, R_VIEW, _netView ? "show arena >" : "show brain >", C_ACCENT, C_PANEL);

  if (_netView) {
    drawNet();
  } else {
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
    // the opponent runs ITS code from its start (red route); your brain runs from its start
    // (blue/green route). While training we REVEAL the route step-by-step (a bright bot head
    // leading a trail) so you watch them hunt; otherwise the whole route is shown.
    g.drawRect(ox + _s1.col * tile, oy + _s1.row * tile, tile - 1, tile - 1, C_BAD);
    int show = _animating ? _animFrame : 9999;
    for (int i = 0; i < _oppLen && i <= show; i++) {
      int r = _oppPath[i] / _maze.cols(), c = _oppPath[i] % _maze.cols();
      bool head = (_animating && i == show);
      g.fillCircle(ox + c * tile + tile / 2, oy + r * tile + tile / 2, head ? tile / 3 : tile / 7 + 1, C_BAD);
    }
    for (int i = 0; i < _pathLen && i <= show; i++) {
      int r = _path[i] / _maze.cols(), c = _path[i] % _maze.cols();
      bool head = (_animating && i == show);
      g.fillCircle(ox + c * tile + tile / 2, oy + r * tile + tile / 2,
                   head ? tile / 3 : tile / 6 + 1, _beatsAI ? C_GO : C_MOVE);
    }
    char leg[40];
    if (_saved && _savedIdx >= 0 && _savedIdx < (int)_profile->library.size())
      snprintf(leg, sizeof(leg), "saved as \"%s\" -> My Bots", _profile->library[_savedIdx].name.c_str());
    else snprintf(leg, sizeof(leg), "you  vs  %s", _oppName.c_str());
    label(g, SCREEN_W / 2, BOTBAR_Y - 9, leg, _saved ? C_GO : C_DIM, textdatum_t::bottom_center);
  }

  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_TEACH, "Teach", _matchType == MatchType::SUMO ? C_DIM : C_GO, C_PANEL);  // race-only
  button(g, R_EVO, _animating ? "..." : "Evolve", C_FUNC, C_PANEL);
  button(g, R_SAVE, _saved ? "saved!" : "Save fighter", _saved ? C_DIM : C_ACCENT, C_PANEL);
  button(g, R_BACK, "Back", C_INK, C_PANEL);
}

// The "watch it learn" panel: the brain's network graph (weights recolour as it trains) plus
// a small arena mini-map so the opponent (red) and your path (green) are visible alongside it.
void ArenaTrainScreen::drawNet() {
  auto& g = hal::display.gfx();
  drawBrainGraph(g, &_brain, nullptr, false, _in, _hid, _out, _action, -1, 18);  // shifted down to centre

  // arena mini-map, below the view chip and left of the input labels (so nothing overlaps)
  int cols = _maze.cols(), rows = _maze.rows();
  int box = 50, mx0 = 2, my0 = BAND_Y + 22;
  g.fillRect(mx0, my0, box, box + 2, C_BG);
  int mt = (box - 2) / cols; if ((box - 2) / rows < mt) mt = (box - 2) / rows; if (mt < 3) mt = 3;
  int ox = mx0 + 1, oy = my0 + 1;
  for (int r = 0; r < rows; r++)
    for (int c = 0; c < cols; c++) {
      int x = ox + c * mt, y = oy + r * mt;
      Tile t = _maze.at(r, c);
      uint16_t col = ((r + c) & 1) ? C_FLOOR : C_FLOOR2;
      if (t == WALL) col = C_WALL; else if (t == PIT) col = C_BG;
      g.fillRect(x, y, mt - 1, mt - 1, col);
      if (_maze.isGoal(r, c)) g.fillCircle(x + mt / 2, y + mt / 2, mt / 3 + 1, C_ACCENT);
    }
  for (int i = 0; i < _oppLen; i++) {
    int r = _oppPath[i] / cols, c = _oppPath[i] % cols;
    g.fillCircle(ox + c * mt + mt / 2, oy + r * mt + mt / 2, 1, C_BAD);
  }
  for (int i = 0; i < _pathLen; i++) {
    int r = _path[i] / cols, c = _path[i] % cols;
    g.fillCircle(ox + c * mt + mt / 2, oy + r * mt + mt / 2, 1, _beatsAI ? C_GO : C_MOVE);
  }

  // status strip just above the toolbar
  g.fillRect(0, BOTBAR_Y - 14, SCREEN_W, 12, C_BG);
  char st[48];
  const char* verb = _matchType == MatchType::SUMO ? (_beatsAI ? "K.O.!" : "hunting")
                                                   : (_beatsAI ? "WINS!" : "racing");
  if (_saved && _savedIdx >= 0 && _savedIdx < (int)_profile->library.size())
    snprintf(st, sizeof(st), "saved as \"%s\" -> My Bots", _profile->library[_savedIdx].name.c_str());
  else if (_animating) snprintf(st, sizeof(st), "learning... gen %d  (you green vs %s red)", _evo.gen, _oppName.c_str());
  else snprintf(st, sizeof(st), "you(green) vs %s(red): %s", _oppName.c_str(), verb);
  label(g, 6, BOTBAR_Y - 13, st, _saved ? C_GO : (_beatsAI ? C_GO : C_DIM));
}

app::Signal ArenaTrainScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  // Animated Evolve. The hunt plays back FAST (a bright bot head advancing along its route,
  // looping) so you SEE the fighters chasing and shoving; meanwhile a generation evolves in
  // the background every so often, so the path + network visibly improve as it learns.
  if (_animating) {
    bool redraw = false;
    if (now - _genAt >= 110) {                 // background: one generation
      _genAt = now;
      _evo.breed(); _evo.evaluateArena(_maze, _s0, _s1, _ai, 200, _matchType); _brain = _evo.best();
      _taught = false; _saved = false; evaluateAndTrace();   // recompute the hunt for the new brain
      if (--_animLeft <= 0) { _animating = false; _animFrame = 9999; }  // done -> show the full trail
      redraw = true;
    }
    if (_animating && now - _animAt >= 38) {   // foreground: reveal one more hunt step (sped up)
      _animAt = now;
      int maxlen = _pathLen > _oppLen ? _pathLen : _oppLen;
      if (++_animFrame > maxlen) _animFrame = 0;   // loop the hunt while it keeps learning
      redraw = true;
    }
    if (redraw) draw();
  }
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  if (R_VIEW.contains(tx, ty)) {
    _netView = !_netView; hal::audio.blip(); draw();
  } else if (R_MODE.contains(tx, ty)) {  // Race <-> Sumo
    setMode(_matchType == MatchType::RACE ? MatchType::SUMO : MatchType::RACE);
    hal::audio.blip(); draw();
  } else if (!_netView && R_OPP.contains(tx, ty)) {
    _oppIdx = (_oppIdx + 1) % oppCount();  // next sparring partner
    buildOpponent(_oppIdx);
    _saved = false; evaluateAndTrace(); hal::audio.blip(); draw();
  } else if (R_TEACH.contains(tx, ty)) {
    if (_matchType == MatchType::SUMO) { hal::audio.fail(); }  // no race teacher for combat; Evolve
    else {
      _animating = false;
      distillSolver(_brain, _maze, true, 700);  // a strong racer beats most AIs
      _taught = true; _saved = false; evaluateAndTrace(); hal::audio.blip(); draw();
    }
  } else if (R_EVO.contains(tx, ty)) {
    _animating = true; _animLeft = 16; _animAt = now; _genAt = now; _animFrame = 0;  // watch it hunt & learn
    hal::audio.blip();
  } else if (R_SAVE.contains(tx, ty)) {
    if (!_profile) { hal::audio.fail(); }
    else {
      Program prog; prog.brains.push_back(_brain);
      Node loop = Node::repeatUntil(AT_GOAL); loop.body.push_back(Node::neuro(0)); prog.main.push_back(loop);
      if (_savedIdx >= 0 && _savedIdx < (int)_profile->library.size()) {
        _profile->library[_savedIdx].program = prog;   // update this session's fighter (no dupe)
        store::profiles.save(*_profile);                // persist NOW, not just on the way out
        _saved = true; hal::audio.badge(); draw();
      } else if (_profile->library.size() < (size_t)LIBRARY_MAX) {
        LibEntry e; e.source = LIB_ARENA; e.name = autoLibName(*_profile, LIB_ARENA, 0); e.program = prog;
        _profile->library.push_back(e);
        _savedIdx = (int)_profile->library.size() - 1;
        store::profiles.save(*_profile);                // persist NOW, not just on the way out
        _saved = true; hal::audio.badge(); draw();
      } else { hal::audio.fail(); }
    }
  } else if (R_BACK.contains(tx, ty)) {
    return app::Signal::BACK;
  }
  return app::Signal::NONE;
}

}  // namespace screens
