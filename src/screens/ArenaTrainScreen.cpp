#include "screens/ArenaTrainScreen.h"
#include "app/Log.h"
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

static const Rect R_OPP   = {6,   (int16_t)(BAND_Y + 2), 84,  18}; // tap to change sparring partner
static const Rect R_MODE  = {92,  (int16_t)(BAND_Y + 2), 52,  18}; // Race <-> Sumo
static const Rect R_BTYPE = {146, (int16_t)(BAND_Y + 2), 40,  18}; // brain type: feedforward <-> RNN
static const Rect R_VIEW  = {188, (int16_t)(BAND_Y + 2), 44,  18}; // toggle arena <-> brain view
static const Rect R_SCRAM = {234, (int16_t)(BAND_Y + 2), 42,  18}; // scramble: a fresh random brain (explicit reset)
static const Rect R_KNOBS = {278, (int16_t)(BAND_Y + 2), 38,  18}; // open the advanced knobs overlay
// Three training engines side by side (Teach = imitate, Evolve = select, Q-Learn = reward),
// then Save + Back. Compact so all five fit the 320px bar.
static const Rect R_TEACH = {4,   (int16_t)(BOTBAR_Y + 2), 52, 32};
static const Rect R_EVO   = {58,  (int16_t)(BOTBAR_Y + 2), 56, 32};
static const Rect R_QLRN  = {116, (int16_t)(BOTBAR_Y + 2), 64, 32};
static const Rect R_SAVE  = {182, (int16_t)(BOTBAR_Y + 2), 70, 32};
static const Rect R_BACK  = {254, (int16_t)(BOTBAR_Y + 2), 60, 32};
static constexpr int N_HOUSE_OPP = 8;  // Bolt, Coil, Spin, Vex, Ace, Neura, Cortex, Self (self-play)
static const char* const HOUSE_OPP[N_HOUSE_OPP] = {"Bolt", "Coil", "Spin", "Vex", "Ace", "Neura", "Cortex", "Self"};
static constexpr int OPP_SELF = 7;     // spar your own evolving champion (self-play)

// The sparring roster = house bots + every bot in your library (incl. radio-traded
// friends' bots and fighters you saved). Train against code OR neuro opponents.
int ArenaTrainScreen::oppCount() const {
  return N_HOUSE_OPP + (_profile ? (int)_profile->library.size() : 0);
}

std::string ArenaTrainScreen::oppNameFor(int idx) const {
  if (idx >= 0 && idx < N_HOUSE_OPP) return HOUSE_OPP[idx];
  int li = idx - N_HOUSE_OPP;
  if (_profile && li >= 0 && li < (int)_profile->library.size()) return _profile->library[li].name;
  return "Ace";
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
  } else if (idx == 5 || idx == 6) {   // "Neura"/"Cortex": NEURAL opponents (a trained brain)
    // In Battle they're trained hunters (distilled); in Race they're maze solvers -- so a kid can
    // spar a real neural net, not just code bots. Two seeds -> two distinct brains.
    _oppName = HOUSE_OPP[idx];
    uint8_t bi = _ai.addBrain((idx == 5 ? 11u : 29u) + (_profile ? _profile->seedBase : 0u));
    if (_matchType == MatchType::SUMO) {
      distillHunter(_ai.brains[bi], (idx == 5 ? 11u : 29u), 2000, true);
    } else {
      Maze mm = _maze; mm.setStart(_s1);
      distillSolver(_ai.brains[bi], mm, true, 500);
    }
    Node loop = Node::repeatUntil(AT_GOAL); loop.body.push_back(Node::neuro(bi)); _ai.main.push_back(loop);
  } else if (idx == OPP_SELF) {    // "Self": spar a copy of YOUR current brain (self-play). Evolve
    _oppName = "Self";             // refreshes this to the champion each generation (an arms race).
    setSelfOpponent(_brain);
  } else {                         // your library: code OR neuro, incl. radio-traded bots
    int li = idx - N_HOUSE_OPP;
    if (_profile && li >= 0 && li < (int)_profile->library.size()) {
      _oppName = _profile->library[li].name;
      _ai = _profile->library[li].program;
    } else { _oppName = "Ace"; }
  }
}

void ArenaTrainScreen::setSelfOpponent(const gb::Net& b) {
  _ai.clear();
  uint8_t bi = _ai.addBrain(1);
  _ai.brains[bi] = b;                              // a frozen copy of the current champion
  Node loop = Node::repeatUntil(AT_GOAL); loop.body.push_back(Node::neuro(bi)); _ai.main.push_back(loop);
}

ui::Rect ArenaTrainScreen::oppRowRect(int i) const {
  int n = oppCount(); if (n < 1) n = 1;
  int top = BAND_Y + 4, avail = BAND_H - 8;
  int rowh = avail / n; if (rowh > 24) rowh = 24; if (rowh < 12) rowh = 12;
  return {8, (int16_t)(top + i * rowh), (int16_t)(SCREEN_W - 16), (int16_t)(rowh - 2)};
}

// The tap-to-pick opponent menu (replaces one-at-a-time cycling): house bots (code + neural),
// then your own library fighters. The current pick is highlighted; tap any row to spar it.
void ArenaTrainScreen::drawOppList() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Pick opponent", C_ACCENT, textdatum_t::top_left, 2);
  int n = oppCount();
  for (int i = 0; i < n; i++) {
    Rect r = oppRowRect(i);
    bool sel = (i == _oppIdx), house = (i < N_HOUSE_OPP);
    g.fillRoundRect(r.x, r.y, r.w, r.h, 4, sel ? C_PANEL_HI : C_PANEL);
    g.drawRoundRect(r.x, r.y, r.w, r.h, 4, sel ? C_ACCENT : (house ? C_PANEL_HI : C_GO));
    int ty = r.y + (r.h - 8) / 2;
    label(g, r.x + 8, ty, oppNameFor(i).c_str(), house ? C_INK : C_GO);
    const char* tag = (i <= 2) ? "code" : (i == 3) ? "hunts & zaps" : (i == 4) ? "maze solver"
                      : (i == 5 || i == 6) ? "neural brain" : (i == OPP_SELF) ? "self-play (Evolve)" : "your bot";
    label(g, r.x + r.w - 8, ty, tag, C_DIM, textdatum_t::top_right);
  }
  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "Cancel", C_INK, C_PANEL);
}

void ArenaTrainScreen::setupBoard() {
  if (_matchType == MatchType::SOCCER) {
    // A walled pitch with two goal mouths; the brain (bot 0) attacks the right mouth.
    MazeGen::generateSoccerPitch(_maze, _profile ? _profile->seedBase : 31u, _s0, _s1, _ball, _goal0, _goal1);
  } else if (_matchType == MatchType::SUMO) {
    // Practice on the SAME open ring you'll battle on, varied per session so the fighter generalises.
    uint32_t seed = (_profile ? _profile->seedBase : 31u) + (uint32_t)millis();
    MazeGen::generateSumoRing(_maze, seed, _s0, _s1);
  } else {
    MazeGen::generateArena(_maze, _profile ? _profile->seedBase + 31u : 31u, _s0, _s1);
  }
  _maze.setStart(_s0);  // the brain is bot 0, starting at s0
}

void ArenaTrainScreen::begin(Profile* profile) {
  _profile = profile;
  // Nothing saved yet? Start in BATTLE mode AND spar Vex (idx 3) -- the exact opponent they'll
  // face in a real Battle -- so a "wins!" in training means they win their first battle. The
  // hint banner walks them through it (Evolve -> Save).
  _editLink = false; _editProg = nullptr; _editIdx = -1;
  bool first = _profile && _profile->library.empty();
  _matchType = first ? MatchType::SUMO : MatchType::RACE;
  setupBoard();
  _oppIdx = first ? 3 : 0;   // 3 = Vex (the seeker that's the vs-Computer Battle opponent)
  buildOpponent(_oppIdx);
  _evo.init(SENSOR_COUNT_FOR_BRAIN, 8, 5, 23);
  _rnn = false; _qLearning = false;   // RNN brain (rbrain()) is allocated lazily on first toggle
  _taught = false; _saved = false; _savedIdx = -1; _curveLen = 0; _didTrain = false;
  evaluateAndTrace();
}

// Seed the trainer with an existing program brain (from the editor's "train brain >" -> Arena),
// and remember to write the trained result back into that node when we leave.
void ArenaTrainScreen::beginEditBrain(Profile* profile, Program* prog, int brainIdx) {
  _profile = profile;
  _editProg = prog; _editIdx = brainIdx; _editLink = true;
  // detect whether this brain block is recurrent (rnn) by finding its N_NEURO node
  bool isRnn = false;
  if (prog) {
    NodeList* lists[3] = {&prog->main, &prog->f1, &prog->f2};
    for (NodeList* lst : lists)
      for (Node& n : *lst) {
        if (n.type == N_NEURO && n.brainIdx == brainIdx && n.rnn) isRnn = true;
        for (Node& c : n.body)
          if (c.type == N_NEURO && c.brainIdx == brainIdx && c.rnn) isRnn = true;
      }
  }
  _matchType = MatchType::SUMO;     // fight training
  setupBoard();
  _oppIdx = 3;                      // spar Vex (the default Battle foe)
  buildOpponent(_oppIdx);
  _evo.init(SENSOR_COUNT_FOR_BRAIN, 8, 5, 23);
  _rnn = isRnn; _qLearning = false; _animating = false;
  _saved = false; _savedIdx = -1; _curveLen = 0; _didTrain = false;
  if (isRnn) {
    if (brainIdx < (int)prog->rbrains.size()) { rbrain() = prog->rbrains[brainIdx]; rbrain().trained = true; }
    _taught = false;                // rnn path trains rbrain(); evaluateAndTrace won't touch _brain
  } else {
    if (prog && brainIdx < (int)prog->brains.size()) _brain = prog->brains[brainIdx];
    _taught = true;                 // seeded FF brain -> don't let evaluateAndTrace overwrite it
  }
  evaluateAndTrace();
}

// Copy the trained brain back into the editing program's neuro node.
void ArenaTrainScreen::writeBackEditBrain() {
  if (!_editLink || !_editProg || _editIdx < 0) return;
  if (_rnn) { if (_editIdx < (int)_editProg->rbrains.size()) _editProg->rbrains[_editIdx] = rbrain(); }
  else      { if (_editIdx < (int)_editProg->brains.size())  _editProg->brains[_editIdx]  = _brain; }
}

// Switch Race <-> Sumo: different board (Sumo has no goal), different fitness, fresh training.
void ArenaTrainScreen::setMode(MatchType t) {
  _matchType = t;
  _animating = false; _qLearning = false;
  setupBoard();
  buildOpponent(_oppIdx);
  _evo.init(SENSOR_COUNT_FOR_BRAIN, 8, 5, 23);
  if (_rnn) rbrain().config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 23);  // fresh brain of the current type
  _taught = false; _saved = false; _savedIdx = -1; _curveLen = 0; _didTrain = false;
  evaluateAndTrace();
}

void ArenaTrainScreen::enter() { draw(); }

void ArenaTrainScreen::evaluateAndTrace() {
  // Evolve is feedforward-only: refresh _brain from the population unless we're in RNN mode or the
  // brain was directly trained (Teach/Q-Learn set _taught so their result isn't overwritten).
  if (!_rnn && !_taught) {
    if (_matchType == MatchType::SOCCER)
      _evo.evaluateArena(_maze, _s0, _s1, _ai, 200, _matchType, &_ball, &_goal0, &_goal1);
    else
      _evo.evaluateArena(_maze, _s0, _s1, _ai, 200, _matchType);
    _brain = _evo.best();
  }
  Program bp;
  Node loop = Node::repeatUntil(AT_GOAL);
  if (_rnn) { bp.rbrains.push_back(rbrain()); loop.body.push_back(Node::rnnBrain(0)); }
  else      { bp.brains.push_back(_brain);   loop.body.push_back(Node::neuro(0)); }
  bp.main.push_back(loop);
  int cols = _maze.cols();

  if (_matchType == MatchType::SOCCER) {
    // Trace the dribble: step both bots so you watch the brain chase the ball and shove it goalward.
    const int CAP = 200;
    Arena ar; ar.setup(&_maze, &bp, &_ai, _s0, _s1, MatchType::SOCCER, CAP);
    ar.configSoccer(_ball, _goal0, _goal1);   // _ball is the fixed kickoff spot (not mutated here)
    int d0 = abs(_ball.row - _goal0.row) + abs(_ball.col - _goal0.col);
    _pathLen = 0; _oppLen = 0;
    for (int s = 0; s < CAP; s++) {
      Pose a = ar.pose(0), b = ar.pose(1);
      if (_pathLen < PATH_N) _path[_pathLen++] = (uint8_t)(a.row * cols + a.col);
      if (_oppLen  < PATH_N) _oppPath[_oppLen++] = (uint8_t)(b.row * cols + b.col);
      if (ar.tick() != ArenaOutcome::RUNNING) break;
    }
    ArenaOutcome o = ar.outcome();
    _beatsAI = (o == ArenaOutcome::BOT0);
    int dEnd = abs(ar.ball().row - _goal0.row) + abs(ar.ball().col - _goal0.col);
    // score 0..1: scored = 1, else how much closer to our goal the ball got (progress / start dist)
    _score = (o == ArenaOutcome::BOT0) ? 1.0f : (d0 > 0 ? (float)(d0 - dEnd) / (float)d0 : 0.5f);
    if (_score < 0) _score = 0; else if (_score > 1) _score = 1;
  } else if (_matchType == MatchType::SUMO) {
    // Trace the REAL fight: step both bots so you see them hunt and shove (not solo wandering).
    // Use the SAME 150-tick cap as a real Battle so the trainer's "wins!" predicts the actual
    // outcome -- a slower hunter that KOs the foe at tick ~100 would falsely read as a loss at 80.
    const int CAP = 150;
    Arena ar; ar.setup(&_maze, &bp, &_ai, _s0, _s1, MatchType::SUMO, CAP);
    _pathLen = 0; _oppLen = 0;
    for (int s = 0; s < CAP; s++) {
      Pose a = ar.pose(0), b = ar.pose(1);
      if (_pathLen < PATH_N) _path[_pathLen++] = (uint8_t)(a.row * cols + a.col);
      if (_oppLen  < PATH_N) _oppPath[_oppLen++] = (uint8_t)(b.row * cols + b.col);
      if (ar.tick() != ArenaOutcome::RUNNING) break;
    }
    ArenaOutcome o = ar.outcome();
    _beatsAI = (o == ArenaOutcome::BOT0);
    // score 0..1: a clean win = 1, a loss = 0, an unresolved bout by the HP margin around 0.5
    _score = (o == ArenaOutcome::BOT0) ? 1.0f : (o == ArenaOutcome::BOT1) ? 0.0f
             : 0.5f + 0.5f * (float)(ar.hp(0) - ar.hp(1)) / (float)SUMO_HP;
    if (_score < 0) _score = 0; else if (_score > 1) _score = 1;
  } else {
    // RACE: the brain's solo run to the goal + the opponent's solo run, then a verdict match.
    Interpreter it; it.load(&bp, &_maze, _s0, 64);
    _pathLen = 0;
    for (int s = 0; s < 64; s++) {
      Pose p = it.pose();
      if (_pathLen < PATH_N) _path[_pathLen++] = (uint8_t)(p.row * cols + p.col);
      if (it.finished()) break;
      it.step();
    }
    Interpreter oit; oit.load(&_ai, &_maze, _s1, 64);
    _oppLen = 0;
    for (int s = 0; s < 64; s++) {
      Pose p = oit.pose();
      if (_oppLen < PATH_N) _oppPath[_oppLen++] = (uint8_t)(p.row * cols + p.col);
      if (oit.finished()) break;
      oit.step();
    }
    Pose fin = it.pose();                              // the brain's final position this trace
    Arena ar; ar.setup(&_maze, &bp, &_ai, _s0, _s1, MatchType::RACE); ar.run();
    _beatsAI = (ar.outcome() == ArenaOutcome::BOT0);
    // score 0..1: beat the opponent to the goal = 1, else how close it got (BFS distance)
    int dist = distanceToGoal(_maze, fin.row, fin.col);
    int far = _maze.rows() + _maze.cols();
    _score = _beatsAI ? 1.0f : (dist <= 0) ? 0.85f : 1.0f - (float)dist / (float)(far > 0 ? far : 1);
    if (_score < 0) _score = 0; else if (_score > 0.95f && !_beatsAI) _score = 0.95f;
  }
  inferBrain();   // refresh the network-graph activations to match the (re)trained brain
}

void ArenaTrainScreen::pushCurve() {
  if (_curveLen < CURVE_N) _curve[_curveLen++] = _score;
  else { for (int i = 1; i < CURVE_N; i++) _curve[i - 1] = _curve[i]; _curve[CURVE_N - 1] = _score; }
}

// A little sparkline of the brain's score climbing as it trains -- the "is it actually learning?"
// metric. Baseline at 0.5 (a coin-flip), the curve rides above it as the brain improves.
void ArenaTrainScreen::drawCurve(int x, int y, int w, int h) {
  auto& g = hal::display.gfx();
  g.fillRoundRect(x, y, w, h, 3, C_PANEL);
  g.drawFastHLine(x + 2, y + h - 1 - (int)((h - 4) * 0.5f), w - 4, C_FLOOR2);  // 0.5 baseline
  label(g, x + 3, y + 1, "learning", C_DIM);
  if (_curveLen < 2) return;
  int px = x + 2, py = 0;
  for (int i = 0; i < _curveLen; i++) {
    float v = _curve[i]; if (v < 0) v = 0; else if (v > 1) v = 1;
    int cx = x + 2 + (w - 4) * i / (_curveLen - 1);
    int cy = y + h - 2 - (int)((h - 10) * v);
    if (i > 0) g.drawLine(px, py, cx, cy, C_GO);
    g.fillCircle(cx, cy, 1, C_ACCENT);
    px = cx; py = cy;
  }
}

gb::RNet& ArenaTrainScreen::rbrain() {
  if (!_rbrain) { _rbrain = new gb::RNet(); _rbrain->config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 23); }
  return *_rbrain;
}

void ArenaTrainScreen::inferBrain() {
  if (_matchType == gb::MatchType::SOCCER)
    senseSoccer(_maze, _s0, &_ball, &_s1, &_goal0, _in);  // ball + rival(_s1) + own net(_goal0)
  else
    senseEgo(_maze, _s0, nullptr, _in);
  if (_rnn) {
    rbrain().resetState();
    rbrain().step(_in, _out);
    for (int j = 0; j < gb::NET_MAX_HID; j++) _hid[j] = (j < rbrain().nHid) ? rbrain().h[j] : 0.0f;
  } else {
    _brain.forward(_in, _out, _hid);
  }
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
  if (_advanced) { _knobs.draw(); return; }  // knobs overlay takes over the whole screen
  if (_oppPick) { drawOppList(); return; }   // the opponent picker takes over the whole screen
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  const char* title = _matchType == MatchType::SOCCER ? "Train a soccer bot"
                    : _matchType == MatchType::RACE   ? "Train a racer" : "Train a fighter";
  label(g, 6, 3, title, C_FUNC, textdatum_t::top_left, 2);
  char hd[28];
  if (_animating && _qLearning) snprintf(hd, sizeof(hd), "Q-learn %d/%d", _qChunks - _animLeft, _qChunks);
  else if (_taught) snprintf(hd, sizeof(hd), "taught  %s", _beatsAI ? "wins!" : "vs");
  else              snprintf(hd, sizeof(hd), "gen %d  %s", _evo.gen, _beatsAI ? "wins!" : "vs");
  label(g, SCREEN_W - 6 - SOUND_ICON_W, 6, hd, (_animating && _qLearning) ? C_MOVE : (_beatsAI ? C_GO : C_DIM), textdatum_t::top_right);  // clear the sound icon

  // tappable sparring-partner chip, shown in BOTH views (the mini-map sits in the row below it):
  // switching opponents keeps the same brain, so you can transfer-learn against foe after foe.
  char chip[32]; snprintf(chip, sizeof(chip), "vs %s >", _oppName.c_str());
  button(g, R_OPP, chip, ui::rgb(120, 230, 245), C_PANEL);
  button(g, R_MODE, _matchType == MatchType::SUMO ? "Battle >" :
                    _matchType == MatchType::SOCCER ? "Soccer >" : "Race >", C_FUNC, C_PANEL);
  button(g, R_BTYPE, _rnn ? "RNN >" : "FF >", _rnn ? C_MOVE : C_DIM, C_PANEL);  // brain type to train
  button(g, R_VIEW, _netView ? "arena" : "brain", C_ACCENT, C_PANEL);
  button(g, R_SCRAM, "Fresh", C_BAD, C_PANEL);   // explicit scramble back to a random brain
  // "Knobs" lights up (accent) when any hyperparameter is off its default, so it's clear they're active
  button(g, R_KNOBS, "Knobs", _knobs.isDefault() ? C_DIM : C_ACCENT, C_PANEL);

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
        if (_matchType != MatchType::SOCCER && _maze.isGoal(r, c))
          assets::drawGoalToken(g, x + tile / 2, y + tile / 2, tile, 0);
      }
    if (_matchType == MatchType::SOCCER) {
      // The two goal MOUTHS (4 tiles tall): your target is green, the opponent's red. Tint the
      // mouth tiles so the open gaps in the wall read clearly as goals; then draw the ball (white).
      auto mouth = [&](const Pose& gp, uint16_t col) {
        for (int dr = SOCCER_MOUTH_LO; dr <= SOCCER_MOUTH_HI; dr++) {
          int x = ox + gp.col * tile, y = oy + (gp.row + dr) * tile;
          g.fillRect(x, y, tile - 1, tile - 1, col);
          g.drawRect(x, y, tile - 1, tile - 1, C_INK);
        }
      };
      mouth(_goal0, ui::rgb(20, 90, 50));   // your net (green)
      mouth(_goal1, ui::rgb(110, 30, 30));  // their net (red)
      g.fillCircle(ox + _ball.col * tile + tile / 2, oy + _ball.row * tile + tile / 2, tile / 4 + 1, C_INK);
      g.drawCircle(ox + _ball.col * tile + tile / 2, oy + _ball.row * tile + tile / 2, tile / 4 + 1, C_BG);
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
    char leg[48];
    if (_saved && _savedIdx >= 0 && _savedIdx < (int)_profile->library.size())
      snprintf(leg, sizeof(leg), "saved as \"%s\" -> My Bots", _profile->library[_savedIdx].name.c_str());
    else if (firstFighter() && !_animating)
      snprintf(leg, sizeof(leg), "Tap EVOLVE to train, then SAVE to %s!",
               _matchType == gb::MatchType::SOCCER ? "play" : _matchType == gb::MatchType::RACE ? "race" : "battle");
    else snprintf(leg, sizeof(leg), "you  vs  %s", _oppName.c_str());
    label(g, SCREEN_W / 2, BOTBAR_Y - 9, leg,
          _saved ? C_GO : (firstFighter() ? C_ACCENT : C_DIM), textdatum_t::bottom_center);
    if (_curveLen >= 1 && ox > 50) drawCurve(4, oy, ox - 8, 44);   // learning curve in the left margin
  }

  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  bool sumo = (_matchType == MatchType::SUMO);
  button(g, R_TEACH, "Teach", C_GO, C_PANEL);  // Race: distil a solver; Battle: distil a hunter (FF or RNN)
  // Evolve and Q-Learn are feedforward-only engines -> greyed when the RNN brain type is selected.
  button(g, R_EVO, (_animating && !_qLearning) ? "..." : "Evolve", _rnn ? C_DIM : C_FUNC, C_PANEL);
  // Q-Learn (reward): FF hunts in Battle; RNN learns mazes in Race (memory helps) and CAN be tried
  // on Battle too (where it flounders -- a deliberate "match the method to the problem" lesson).
  bool qOk = sumo || _rnn || _matchType == MatchType::SOCCER;   // only FF+Race has no Q engine
  button(g, R_QLRN, (_animating && _qLearning) ? "..." : "Q-Learn", qOk ? C_MOVE : C_DIM, C_PANEL);
  // edit-link (opened from the editor): "< Editor" bails out (no change), "Use it >" writes the
  // brain back into the program. From the Arena menu: "Save" to library, then launch a match --
  // the launch verb matches the game (Play! soccer, Race! a race, Fight! a battle).
  if (_editLink) button(g, R_SAVE, "< Editor", C_INK, C_PANEL);
  else           button(g, R_SAVE, _saved ? "saved!" : "Save", _saved ? C_DIM : C_ACCENT, C_PANEL);
  const char* go = _matchType == gb::MatchType::SOCCER ? "Play! >"
                 : _matchType == gb::MatchType::RACE   ? "Race! >" : "Fight! >";
  button(g, R_BACK, _editLink ? "Use it >" : go, C_GO, C_PANEL);
}

// The "watch it learn" panel: the brain's network graph (weights recolour as it trains) plus
// a small arena mini-map so the opponent (red) and your path (green) are visible alongside it.
void ArenaTrainScreen::drawNet() {
  auto& g = hal::display.gfx();
  drawBrainGraph(g, &_brain, &rbrain(), _rnn, _in, _hid, _out, _action, -1, 18,  // FF or RNN; shifted to centre
                 _matchType == gb::MatchType::SOCCER);  // soccer relabels inputs (ball/net/rival) + outputs

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
  // reveal the hunt step-by-step here too (bright head + trail), in sync with the arena view
  int show = _animating ? _animFrame : 9999;
  for (int i = 0; i < _oppLen && i <= show; i++) {
    int r = _oppPath[i] / cols, c = _oppPath[i] % cols;
    bool head = (_animating && i == show);
    g.fillCircle(ox + c * mt + mt / 2, oy + r * mt + mt / 2, head ? mt / 3 + 1 : 1, C_BAD);
  }
  for (int i = 0; i < _pathLen && i <= show; i++) {
    int r = _path[i] / cols, c = _path[i] % cols;
    bool head = (_animating && i == show);
    g.fillCircle(ox + c * mt + mt / 2, oy + r * mt + mt / 2, head ? mt / 3 + 1 : 1, _beatsAI ? C_GO : C_MOVE);
  }

  drawCurve(mx0, my0 + box + 5, box + 6, 38);   // learning curve, just below the mini-map

  // status strip just above the toolbar
  g.fillRect(0, BOTBAR_Y - 14, SCREEN_W, 12, C_BG);
  char st[48];
  const char* verb = _matchType == MatchType::SUMO ? (_beatsAI ? "K.O.!" : "hunting")
                                                   : (_beatsAI ? "WINS!" : "racing");
  if (_saved && _savedIdx >= 0 && _savedIdx < (int)_profile->library.size())
    snprintf(st, sizeof(st), "saved as \"%s\" -> My Bots", _profile->library[_savedIdx].name.c_str());
  else if (_animating && _qLearning && _matchType == MatchType::SOCCER) snprintf(st, sizeof(st), "Q-learning... (reward: shove the ball into the goal)");
  else if (_animating && _qLearning) snprintf(st, sizeof(st), "Q-learning... (reward: hunt %s, then zap)", _oppName.c_str());
  else if (_animating) snprintf(st, sizeof(st), "learning... gen %d  (you green vs %s red)", _evo.gen, _oppName.c_str());
  else snprintf(st, sizeof(st), "you(green) vs %s(red): %s", _oppName.c_str(), verb);
  label(g, 6, BOTBAR_Y - 13, st, _saved ? C_GO : (_beatsAI ? C_GO : C_DIM));
}

// Persist the current brain (feedforward _brain or recurrent rbrain) as a library fighter.
// Updates this session's entry in place if already saved; else appends a new "Fighter vN".
// Returns the library index, or -1 if it can't be saved.
int ArenaTrainScreen::saveFighterToLibrary() {
  if (!_profile) return -1;
  Program prog;
  Node loop = Node::repeatUntil(AT_GOAL);
  if (_rnn) { prog.rbrains.push_back(rbrain()); loop.body.push_back(Node::rnnBrain(0)); }
  else      { prog.brains.push_back(_brain);    loop.body.push_back(Node::neuro(0)); }
  prog.main.push_back(loop);
  if (_savedIdx >= 0 && _savedIdx < (int)_profile->library.size()) {
    _profile->library[_savedIdx].program = prog;   // update this session's fighter (no dupe)
  } else if (_profile->library.size() < (size_t)LIBRARY_MAX) {
    // Name the saved bot for its game: Race -> "runner", Battle -> "fighter", Soccer -> "player".
    const char* noun = _matchType == MatchType::SOCCER ? "player"
                     : _matchType == MatchType::SUMO   ? "fighter" : "runner";
    LibEntry e; e.source = LIB_ARENA; e.name = autoLibName(*_profile, LIB_ARENA, 0, noun); e.program = prog;
    _profile->library.push_back(e);
    _savedIdx = (int)_profile->library.size() - 1;
  } else return -1;
  store::profiles.save(*_profile);                  // persist NOW, not just on the way out
  _saved = true;
  applog::event("trained + saved a %s%s",
                _rnn ? "memory " : "",
                _matchType == MatchType::SOCCER ? "player" : _matchType == MatchType::RACE ? "runner" : "fighter");
  return _savedIdx;
}

app::Signal ArenaTrainScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  // Animated Evolve. The hunt plays back FAST (a bright bot head advancing along its route,
  // looping) so you SEE the fighters chasing and shoving; meanwhile a generation evolves in
  // the background every so often, so the path + network visibly improve as it learns.
  if (_animating) {
    bool redraw = false;
    if (now - _genAt >= 110) {                 // background: one learning step
      _genAt = now;
      _didTrain = true;                        // Evolve/Q-Learn is actively training -> counts as a trained brain
      if (_qLearning) {
        // one chunk of Q-learning; exploration decays GLOBALLY across all chunks so it converges.
        // FF: feedforward hunt (Battle). RNN: recurrent maze Q (Race) -- memory escapes dead-ends.
        // The brain is trained directly, so keep _taught=true (don't let evaluateAndTrace overwrite).
        uint32_t base = _profile ? _profile->seedBase : 7u;
        int done = (_qChunks - _animLeft) * 1000, total = _qChunks * 1000;
        if (_matchType == MatchType::SOCCER) {
          // reward-driven dribble: get behind the ball, shove it into the goal (no teacher). FF and
          // RNN share the same MDP -- the recurrent one is noisier (BPTT), the FF more reliable.
          if (_rnn) qTrainSoccerRnn(rbrain(), base + 101u * (uint32_t)_animLeft, 1000, done, total, _knobs.explore);
          else      qTrainSoccer(_brain,     base + 101u * (uint32_t)_animLeft, 1000, done, total, _knobs.explore, &_ai);
        } else if (_rnn && _matchType == MatchType::SUMO) {
          // recurrent Q on the reactive BATTLE hunt -- intentionally available so a kid can SEE it
          // flounder (memory is noise here); the contrast with FF Q-Learn is the lesson.
          qTrainHunterRnn(rbrain(), base + 101u * (uint32_t)_animLeft, 1000, done, total, _knobs.explore);
        } else if (_rnn) {
          // recurrent maze Q -- train on THIS race board (with its start) so it wins the verdict,
          // like the feedforward Teach does. Memory lets it escape dead-ends -- here it WORKS.
          qTrainMazeRnn(rbrain(), base, 1, 1000, done, total, &_maze, &_s0, _knobs.explore);
        } else {
          qTrainHunter(_brain, base + 101u * (uint32_t)_animLeft, 1000, done, total, _knobs.explore, &_ai);
        }
        _taught = true;
      } else {
        // SELF-PLAY: refresh the sparring partner to the current champion BEFORE scoring, so each
        // generation must beat the best-so-far -- an arms race that bootstraps without a fixed foe.
        if (_oppIdx == OPP_SELF) setSelfOpponent(_evo.best());
        // Explore knob raises mutation scale (more variety, less stable) -- the GA's "temperature".
        _evo.breed(0.15f, 0.6f * _knobs.explore);
        if (_matchType == MatchType::SOCCER) {
          // VARIED BOARDS: move the kickoff ball to a fresh interior tile every generation, so the
          // brain is scored from the ball in many places (just like the match's random kickoffs) and
          // generalises instead of memorising one pitch. The starts/goals stay fixed (a fair pitch).
          _soccerGen++;
          uint32_t h = (uint32_t)_soccerGen * 2654435761u;
          int rows = _maze.rows(), cols = _maze.cols();
          _ball.row = (int8_t)(1 + (int)(h % (uint32_t)(rows - 2)));
          _ball.col = (int8_t)(2 + (int)((h >> 9) % (uint32_t)(cols - 4)));   // keep clear of the goal columns
          _evo.evaluateArena(_maze, _s0, _s1, _ai, 200, _matchType, &_ball, &_goal0, &_goal1);
        } else
          _evo.evaluateArena(_maze, _s0, _s1, _ai, 200, _matchType);
        _brain = _evo.best();
        _taught = false;
      }
      _saved = false; evaluateAndTrace();        // recompute the hunt for the new brain
      pushCurve();                               // record this step's score onto the learning curve
      if (--_animLeft <= 0) {
        _animating = false; _qLearning = false; _animFrame = 9999;  // done -> full trail
        if (_matchType == MatchType::SOCCER) { setupBoard(); evaluateAndTrace(); }  // ball back to centre kickoff
      }
      redraw = true;
    }
    if (_animating && now - _animAt >= 38) {   // foreground: reveal one more hunt step (sped up)
      _animAt = now;
      int maxlen = _pathLen > _oppLen ? _pathLen : _oppLen;
      if (++_animFrame > maxlen) _animFrame = 0;   // loop the hunt while it keeps learning
      redraw = true;
    }
    if (redraw && !_advanced) draw();   // don't paint over the knobs overlay if it's open
  }
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  // Opponent picker overlay: tap a row to spar it (keeps the trained brain -> transfer learning),
  // or Cancel/tap-away to close.
  if (_oppPick) {
    for (int i = 0; i < oppCount(); i++) {
      if (oppRowRect(i).contains(tx, ty)) {
        _oppIdx = i; buildOpponent(_oppIdx);
        _saved = false; _curveLen = 0; evaluateAndTrace(); _oppPick = false; hal::audio.blip(); draw();
        return app::Signal::NONE;
      }
    }
    _oppPick = false; hal::audio.blip(); draw();   // Cancel / tapped away
    return app::Signal::NONE;
  }
  // Advanced knobs overlay (shared ui::Knobs): steppers adjust the live hyperparameters; Done closes.
  if (_advanced) {
    if (_knobs.handleTap(tx, ty)) { _advanced = false; draw(); }  // Done -> back to the trainer
    else _knobs.draw();                                           // a knob stepped -> refresh its value
    return app::Signal::NONE;
  }
  if (R_KNOBS.contains(tx, ty)) {
    _advanced = true; hal::audio.blip(); draw();
  } else if (R_SCRAM.contains(tx, ty)) {
    // explicit "scramble": throw away the current brain and start from random noise. Training is
    // transfer-learning by default, so this is the ONLY way to deliberately restart from scratch.
    uint32_t s = (_profile ? _profile->seedBase : 7u) + (uint32_t)now;
    if (_rnn) rbrain().config(SENSOR_COUNT_FOR_BRAIN, 8, 5, s);
    else      _evo.init(SENSOR_COUNT_FOR_BRAIN, 8, 5, s);
    _taught = false; _saved = false; _savedIdx = -1; _qLearning = false; _animating = false; _curveLen = 0;
    evaluateAndTrace(); hal::audio.fail(); draw();
  } else if (R_VIEW.contains(tx, ty)) {
    _netView = !_netView; hal::audio.blip(); draw();
  } else if (R_MODE.contains(tx, ty)) {  // Race -> Sumo -> Soccer -> Race
    MatchType nxt = _matchType == MatchType::RACE ? MatchType::SUMO
                  : _matchType == MatchType::SUMO ? MatchType::SOCCER : MatchType::RACE;
    setMode(nxt);
    hal::audio.blip(); draw();
  } else if (R_OPP.contains(tx, ty)) {
    _oppPick = true; hal::audio.blip(); draw();   // open the pick-list (replaces cycling)
  } else if (R_TEACH.contains(tx, ty)) {
    _animating = false;
    uint32_t seed = _profile ? _profile->seedBase : 7u;
    if (_matchType == MatchType::SOCCER) {
      if (_rnn) { rbrain().lr = _knobs.lr * 0.33f; distillSoccerRnn(rbrain(), seed, 1200 * _knobs.rounds); rbrain().trained = true; }
      else      { _brain.lr = _knobs.lr; distillSoccer(_brain, seed, 5000 * _knobs.rounds); }  // dribble into the goal
    } else if (_rnn) {
      // recurrent brain: BPTT over chase episodes (Battle) or maze runs (Race) -- a memory fighter.
      // RNN BPTT is touchier than FF, so the LR knob is damped to a third (its tuned default is 0.1).
      rbrain().lr = _knobs.lr * 0.33f;
      if (_matchType == MatchType::SUMO) distillHunterRnn(rbrain(), seed, 1500 * _knobs.rounds);
      else rnnTrainGeneral(rbrain(), seed, _profile ? (_profile->level < 8 ? _profile->level : 8) : 6, 4 * _knobs.rounds);
      rbrain().trained = true;
    } else if (_matchType == MatchType::SUMO) {
      _brain.lr = _knobs.lr;
      distillHunter(_brain, seed, 2000 * _knobs.rounds, true);    // instant hunt-and-zap fighter (tracks a moving foe)
    } else {
      _brain.lr = _knobs.lr;
      distillSolver(_brain, _maze, true, 700 * _knobs.rounds);    // a strong racer beats most AIs
    }
    _taught = true; _didTrain = true; _saved = false; _curveLen = 0; evaluateAndTrace(); pushCurve(); hal::audio.blip(); draw();
  } else if (R_BTYPE.contains(tx, ty)) {
    // flip feedforward <-> RNN: it's a fresh brain of the new type, so clear training state
    _rnn = !_rnn;
    _animating = false; _qLearning = false; _curveLen = 0;
    _taught = false; _saved = false; _savedIdx = -1;
    if (_rnn) rbrain().config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 23);
    else      _evo.init(SENSOR_COUNT_FOR_BRAIN, 8, 5, 23);
    evaluateAndTrace(); hal::audio.blip(); draw();
  } else if (R_EVO.contains(tx, ty)) {
    if (_rnn) { hal::audio.fail(); }  // Evolve is feedforward-only
    else {
      _qLearning = false; _curveLen = 0;
      // Evolve REFINES the current brain: seed the whole population from it (transfer learning is the
      // default -- whether you just Taught it, loaded a saved fighter, or evolved already). Tap
      // Scramble first for a from-scratch run.
      _evo.seedFrom(_brain, 0.2f);
      _animating = true; _animLeft = 16 * _knobs.rounds; _animAt = now; _genAt = now; _animFrame = 0;  // watch it hunt & learn
      hal::audio.blip();
    }
  } else if (R_QLRN.contains(tx, ty)) {
    bool noQ = (_matchType == MatchType::RACE && !_rnn);        // only FF race has no Q engine
    if (noQ) { hal::audio.fail(); }
    else {
      // reward-driven training, animated in chunks so the kid watches it learn without a long freeze.
      // It REFINES the current brain (transfer learning by default -- e.g. Teach a dribbler, then
      // Q-Learn to sharpen it); no reset, so tap Scramble first for a from-scratch run. FF = 8 chunks,
      // any RNN = 4 (BPTT is heavier); the Rounds knob multiplies the chunk count.
      if (_rnn) { rbrain().lr = _knobs.lr * 0.33f; _qChunks = 4 * _knobs.rounds; }
      else      { _brain.lr = _knobs.lr;          _qChunks = 8 * _knobs.rounds; }
      _qLearning = true; _curveLen = 0;
      _animating = true; _animLeft = _qChunks; _animAt = now; _genAt = now; _animFrame = 0;
      hal::audio.blip();
    }
  } else if (R_SAVE.contains(tx, ty)) {
    if (_editLink) { hal::audio.blip(); return app::Signal::BACK; }  // "< Editor": bail out, no change applied
    if (saveFighterToLibrary() >= 0) { hal::audio.badge(); draw(); } else hal::audio.fail();
  } else if (R_BACK.contains(tx, ty)) {
    if (_editLink) {   // "Use it >": write the trained brain back into the program, return to editor
      writeBackEditBrain(); hal::audio.blip();
      return app::Signal::BACK;
    }
    // "Fight! >": stash the fighter and jump straight into a match vs the current opponent.
    if (_profile && saveFighterToLibrary() >= 0) { _launchFight = true; hal::audio.blip(); }
    return app::Signal::BACK;
  }
  return app::Signal::NONE;
}

}  // namespace screens
