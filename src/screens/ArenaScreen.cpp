#include "screens/ArenaScreen.h"
#include <Arduino.h>   // millis() -> fresh local ring each match (no on-device RNG)
#include <math.h>      // cosf/sinf for the hit-burst rays
#include <algorithm>   // std::sort for the tournament standings
#include "hal/Audio.h"
#include "hal/Led.h"
#include "assets/Assets.h"
#include "game/MazeGen.h"
#include "game/Bots.h"
#include "game/Score.h"
#include "game/Distill.h"

using namespace ui;
using namespace gb;

namespace screens {

// A pre-trained NeuroBot: distil a brain to navigate this board (from `start`), then
// drive with a NEURO node. Reliable + competent, and an actual neural net to battle.
static void buildNeuroBot(Program& p, const Maze& m, const Pose& start, uint32_t seed) {
  p.clear();
  uint8_t idx = p.addBrain(seed);
  Maze mm = m; mm.setStart(start);
  distillSolver(p.brains[idx], mm, true, 500);
  Node loop = Node::repeatUntil(AT_GOAL);
  loop.body.push_back(Node::neuro(idx));
  p.main.push_back(loop);
}

static const Rect R_BACK = {6, (int16_t)(BOTBAR_Y + 2), 100, 26};
// Level 1 — pick the OPPONENT first (who you play against)
static const Rect R_OPP_AI    = {20, 56,  280, 40};
static const Rect R_OPP_HOT   = {20, 104, 280, 40};
static const Rect R_OPP_RADIO = {20, 152, 280, 40};
// Level 2 — pick the GAME (depends on the opponent); vertical slots reused per branch
static const Rect R_G1 = {20, 56,  280, 34};
static const Rect R_G2 = {20, 94,  280, 34};
static const Rect R_G3 = {20, 132, 280, 34};
static const Rect R_G4 = {20, 170, 280, 34};
static const Rect R_READY= {84, 150, 150, 34};
// result-overlay buttons (a kid wants to instantly retry, not dead-end on a loss)
static const Rect R_AGAIN = {62, 129, 96, 26};
static const Rect R_RESULT_EXIT = {170, 129, 86, 26};
// 3-button layout when we also offer "Train >" (vs-Computer, NeuroBot unlocked)
static const Rect R_AGAIN3 = {56, 129, 64, 26};
static const Rect R_TRAIN3 = {124, 129, 66, 26};
static const Rect R_EXIT3  = {194, 129, 64, 26};
// Cup champion screen: full-screen celebration, so its buttons live in the bottom bar (not mid-
// screen like the match-result overlay) -- otherwise they'd land on the champion's avatar/name.
static const Rect R_CUP_AGAIN = {40,  (int16_t)(BOTBAR_Y + 4), 110, 28};
static const Rect R_CUP_EXIT  = {174, (int16_t)(BOTBAR_Y + 4), 106, 28};

static Program dashProgram() {
  Program p;
  Node loop = Node::repeatUntil(AT_GOAL);
  loop.body.push_back(Node::command(CMD_FWD));
  p.main.push_back(loop);
  return p;
}

void ArenaScreen::begin(Profile* profile) {
  _profile = profile;
  _type = MatchType::RACE;
  _hotseat = false;
  _pick0 = _pick1 = -1;
  _phase = Phase::MENU;
  _running = false;
  _cup = false;
  buildCandidates();
}

void ArenaScreen::buildCandidates(bool sumo) {
  _cands.clear();
  // ALL of the kid's OWN saved bots -- a TRAINED NeuroBot fighter OR a hand-CODED battle bot
  // (now that the editor exposes the foe senses + zap, a kid can write their own seeker). The
  // list scrolls; newest are last, so a just-made fighter is always reachable.
  if (_profile) {
    for (auto& e : _profile->library)
      _cands.push_back({e.name, e.program, _profile->avatar, "your bot", false, false});
  }
  // House bots. The pure dashers (Rusty/Bolt) are Race-only filler -- in Battle they'd just
  // lose, so they're hidden. The real FIGHTERS show in both (and one is the Battle opponent).
  if (!sumo) {
    _cands.push_back({"Rusty", alwaysForwardProgram(), 5, "charges blindly", true, false});
    _cands.push_back({"Bolt",  dashProgram(),          6, "fast & straight", true, false});
  }
  _cands.push_back({"Vex",   hunterProgram(),        7, "hunts & zaps",    true, false});
  // Ace solves the board on the fly — a real navigator (program built at match start).
  _cands.push_back({"Ace",   Program{},              3, "solves the maze", true, true});
  // Pre-trained NeuroBots: their brain (a neural net) is trained on the board at match
  // start, then drives them — battle an actual trained brain.
  _cands.push_back({"Neura",  Program{}, 2, "a trained brain",  true, false, true});
  _cands.push_back({"Cortex", Program{}, 4, "an evolved brain", true, false, true});
  // Volt: a trained NEURAL FIGHTER -- the neural counterpart to code-Vex. In Battle its brain is
  // distilled into a hunter (turn-to-foe + zap), so you can pit your bot against a learned hunter.
  _cands.push_back({"Volt",   Program{}, 1, "a trained fighter", true, false, true});
}

int ArenaScreen::houseBotIndex(const char* name) const {
  for (int i = 0; i < (int)_cands.size(); i++)
    if (_cands[i].house && _cands[i].name == name) return i;
  return -1;
}

void ArenaScreen::enter() { drawMenu(); }

// Round-robin battle ladder: every fighter plays every other (home AND away on one fair ring so
// start-position bias cancels), wins are tallied, and the field is ranked. Headless + deterministic.
void ArenaScreen::runTournament() {
  buildCandidates(true);                                  // Sumo roster: your library first, then house
  int nlib = _profile ? (int)_profile->library.size() : 0;
  std::vector<int> parts;
  for (int i = (nlib > 6 ? nlib - 6 : 0); i < nlib; i++) parts.push_back(i);   // up to 6 of your fighters
  int vex = houseBotIndex("Vex"), ace = houseBotIndex("Ace");                  // + two house benchmarks
  if (vex >= 0) parts.push_back(vex);
  if (ace >= 0) parts.push_back(ace);
  int N = (int)parts.size();
  MazeGen::generateSumoRing(_maze, (_profile ? _profile->seedBase : 7u) + 1234u, _s0, _s1);
  for (int p : parts) setupMatchBot(p, _s0, true);        // finalise each fighter's Sumo program
  std::vector<int> w(N, 0), l(N, 0);
  for (int i = 0; i < N; i++)
    for (int j = 0; j < N; j++) {
      if (i == j) continue;
      Arena ar; ar.setup(&_maze, &_cands[parts[i]].prog, &_cands[parts[j]].prog, _s0, _s1, MatchType::SUMO, 150);
      ArenaOutcome o = ar.run();
      if (o == ArenaOutcome::BOT0) { w[i]++; l[j]++; }
      else if (o == ArenaOutcome::BOT1) { w[j]++; l[i]++; }
    }
  _standings.clear();
  for (int i = 0; i < N; i++) _standings.push_back({parts[i], w[i], l[i]});
  std::sort(_standings.begin(), _standings.end(),
            [](const Standing& a, const Standing& b) { return a.w != b.w ? a.w > b.w : a.l < b.l; });
  _phase = Phase::TOURNEY;
  drawTourney();
}

void ArenaScreen::drawTourney() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Tournament", C_ACCENT, textdatum_t::top_left, 2);
  label(g, SCREEN_W - 6, 6, "wins-losses", C_DIM, textdatum_t::top_right);
  int n = (int)_standings.size();
  int rowh = (BAND_H - 8) / (n > 0 ? n : 1); if (rowh > 26) rowh = 26; if (rowh < 14) rowh = 14;
  for (int i = 0; i < n; i++) {
    const Standing& s = _standings[i];
    int y = BAND_Y + 4 + i * rowh;
    uint16_t medal = (i == 0) ? C_ACCENT : (i == 1) ? C_MOVE : (i == 2) ? C_WALL : C_PANEL_HI;
    g.fillRoundRect(8, y, SCREEN_W - 16, rowh - 2, 4, C_PANEL);
    g.drawRoundRect(8, y, SCREEN_W - 16, rowh - 2, 4, medal);
    char row[40]; snprintf(row, sizeof(row), "%d.  %s", i + 1, _cands[s.cand].name.c_str());
    label(g, 16, y + (rowh - 8) / 2, row, i < 3 ? C_GO : C_INK);
    char wl[16]; snprintf(wl, sizeof(wl), "%d - %d", s.w, s.l);
    label(g, SCREEN_W - 16, y + (rowh - 8) / 2, wl, C_DIM, textdatum_t::top_right);
  }
  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
}

// ---- Cup: single-elimination bracket, watched match by match -------------------------------
void ArenaScreen::startCup() {
  buildCandidates(true);                          // Sumo roster: your library leads the list
  int nlib = _profile ? (int)_profile->library.size() : 0;
  _cupPlayers.clear();
  for (int i = 0; i < nlib && (int)_cupPlayers.size() < gb::BRACKET_MAX; i++) _cupPlayers.push_back(i);
  if ((int)_cupPlayers.size() < 4) {              // pad a tiny field with house challengers
    int v = houseBotIndex("Vex"); if (v >= 0) _cupPlayers.push_back(v);
    int a = houseBotIndex("Ace"); if (a >= 0) _cupPlayers.push_back(a);
  }
  _type = MatchType::SUMO;
  _cupBracket.init((int)_cupPlayers.size());
  _cup = true; _cupMatch = -1;
  _phase = Phase::CUP; drawCupCard();             // opening card; tap to start the first match
}

void ArenaScreen::cupAdvanceToNextMatch() {
  while (true) {
    if (_cupBracket.done()) { _cup = false; _cupMatch = -1; _phase = Phase::CUP; drawCupCard(); return; }
    int next = -1;
    for (int m = 0; m < _cupBracket.matchCount(); m++) {
      int a, b; _cupBracket.pair(m, a, b);
      if (b < 0) { if (_cupBracket.win[m] < 0) _cupBracket.reportWinner(m, a); }   // bye auto-advances
      else if (_cupBracket.win[m] < 0 && next < 0) next = m;
    }
    if (next >= 0) {                              // a real match to watch: set it up + play it
      int a, b; _cupBracket.pair(next, a, b);
      _cupMatch = next;
      _pick0 = _cupPlayers[a]; _pick1 = _cupPlayers[b];
      startMatch();                              // animates in Phase::BOARD; _cup routes the result back
      return;
    }
    if (_cupBracket.roundComplete()) _cupBracket.advance(); else return;
  }
}

// The bracket card between matches: this round's matchups with winners ticked, or the champion.
void ArenaScreen::drawCupCard() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  bool done = _cupBracket.done();
  label(g, 6, 3, "Cup", C_ACCENT, textdatum_t::top_left, 2);
  if (done) {
    label(g, SCREEN_W / 2, 64, "CHAMPION", C_ACCENT, textdatum_t::middle_center, 2);
    int champ = _cupBracket.champion();
    if (champ >= 0 && champ < (int)_cupPlayers.size()) {
      assets::drawCharacter(g, SCREEN_W / 2, 116, 44, _cands[_cupPlayers[champ]].avatar, gb::SOUTH);
      label(g, SCREEN_W / 2, 158, _cands[_cupPlayers[champ]].name.c_str(), C_GO, textdatum_t::middle_center, 2);
    }
  } else {
    char hd[24]; snprintf(hd, sizeof(hd), "Round %d", _cupBracket.round + 1);
    label(g, SCREEN_W - 6, 6, hd, C_DIM, textdatum_t::top_right);
    int mc = _cupBracket.matchCount();
    int rowh = (BAND_H - 6) / (mc > 0 ? mc : 1); if (rowh > 22) rowh = 22; if (rowh < 12) rowh = 12;
    for (int m = 0; m < mc; m++) {
      int a, b; _cupBracket.pair(m, a, b);
      int y = BAND_Y + 2 + m * rowh;
      auto nameOf = [&](int pid) { return (pid >= 0 && pid < (int)_cupPlayers.size()) ? _cands[_cupPlayers[pid]].name.c_str() : "bye"; };
      char row[40];
      if (b < 0) snprintf(row, sizeof(row), "%s  (bye)", nameOf(a));
      else       snprintf(row, sizeof(row), "%s  vs  %s", nameOf(a), nameOf(b));
      label(g, 12, y + (rowh - 8) / 2, row, (_cupBracket.win[m] >= 0 || b < 0) ? C_GO : C_INK);
      if (_cupBracket.win[m] >= 0)
        label(g, SCREEN_W - 12, y + (rowh - 8) / 2, nameOf(_cupBracket.win[m]), C_ACCENT, textdatum_t::top_right);
    }
  }
  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  if (done) {
    button(g, R_CUP_AGAIN, "Again", C_GO, C_PANEL);    // bottom bar, clear of the champion art
    button(g, R_CUP_EXIT, "Exit", C_INK, C_PANEL);
  } else {
    button(g, R_AGAIN, _cupMatch < 0 ? "Start >" : "Next >", C_GO, C_PANEL_HI);
    button(g, R_RESULT_EXIT, "Quit", C_INK, C_PANEL_HI);
  }
}

void ArenaScreen::onMatchEnd() {
  if (_cup && _cupMatch >= 0) {
    int a, b; _cupBracket.pair(_cupMatch, a, b);
    int winner = (_arena.outcome() == ArenaOutcome::BOT1) ? b : a;   // BOT0 or DRAW -> 'a' (tiebreak)
    _cupBracket.reportWinner(_cupMatch, winner);
    _phase = Phase::CUP; drawCupCard();              // show the updated bracket; tap to continue
  } else {
    _phase = Phase::DONE; finishOverlay();
  }
}

void ArenaScreen::drawTChoice() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Tournament", C_ACCENT, textdatum_t::top_left, 2);
  label(g, SCREEN_W / 2, 40, "Pick a format", C_INK, textdatum_t::top_center);
  button(g, R_G1, "Ladder - round robin (instant)", C_MOVE, C_PANEL);
  button(g, R_G2, "Cup - bracket, watch it play", C_ACCENT, C_PANEL);
  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
}

void ArenaScreen::beginQuickBattle(Profile* profile, int libIdx, const char* oppName, bool sumo) {
  _profile = profile;
  _hotseat = false;
  _type = sumo ? MatchType::SUMO : MatchType::RACE;
  buildCandidates(sumo);                       // library entries lead the list, so libIdx maps directly
  int nlib = _profile ? (int)_profile->library.size() : 0;
  _pick0 = (libIdx >= 0 && libIdx < nlib) ? libIdx : 0;
  _pick1 = -1;
  for (int i = 0; i < (int)_cands.size(); i++) if (_cands[i].name == oppName) { _pick1 = i; break; }
  if (_pick1 < 0) _pick1 = houseBotIndex("Vex");                 // opponent not in this roster -> Vex
  if (_pick1 < 0 || _pick1 == _pick0) _pick1 = houseBotIndex("Ace");
  if (_pick1 < 0) _pick1 = (_pick0 == 0 && _cands.size() > 1) ? 1 : 0;
  startMatch();                                // sets up the board + starts animating (BOARD phase)
}

// ---- menus ----------------------------------------------------------------
// Two levels: pick WHO you play (Computer / Hotseat / Radio), then WHICH game.
void ArenaScreen::drawMenu() {
  _phase = Phase::MENU;
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Arena", C_ACCENT, textdatum_t::top_left, 2);
  label(g, SCREEN_W / 2, 38, "Who do you want to play?", C_INK, textdatum_t::top_center);
  button(g, R_OPP_AI,    "vs Computer", C_GO, C_PANEL);
  button(g, R_OPP_HOT,   "Hotseat - 2 players, 1 device", C_FUNC, C_PANEL);
  button(g, R_OPP_RADIO, "Radio - a friend nearby", C_MOVE, C_PANEL);
  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
}

// Level 2: the games available for the chosen opponent (_hotseat picks the branch).
void ArenaScreen::drawGameType() {
  _phase = Phase::GAMETYPE;
  _cup = false;                 // leaving any Cup in progress
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, _hotseat ? "Hotseat" : "vs Computer", C_ACCENT, textdatum_t::top_left, 2);
  label(g, SCREEN_W / 2, 38, "Pick a game", C_INK, textdatum_t::top_center);
  // Battle unlocks at SENSE (Lv 15): once you have IF + the foe senses + zap, you can hand-CODE a
  // seeker, so Battle no longer needs NeuroBot -- that stays for training NEURAL fighters (Lv 28).
  bool neuro  = _profile && _profile->unlocks.neuro;
  bool battle = _profile && _profile->unlocks.sense;
  if (_hotseat) {
    button(g, R_G1, "Race - first to the goal", C_GO, C_PANEL);
    button(g, R_G2, battle ? "Battle" : "Battle - needs Sense (Lv15)",
           battle ? C_ACCENT : C_LOCK, C_PANEL);
    button(g, R_G3, "Puzzle Race - beat the clock", C_LOOP, C_PANEL);
    button(g, R_G4, "Seed Challenge - same board", C_SENSE, C_PANEL);
  } else {
    button(g, R_G1, "Race - first to the goal", C_GO, C_PANEL);
    button(g, R_G2, battle ? "Battle" : "Battle - needs Sense (Lv15)",
           battle ? C_ACCENT : C_LOCK, C_PANEL);
    if (neuro)
      button(g, R_G3, "Train a fighter (NeuroBot)", ui::rgb(120, 230, 245), C_PANEL);
    // Tournament: a battle ladder of your own fighters -- needs at least two saved bots.
    if (neuro && _profile && _profile->library.size() >= 2)
      button(g, R_G4, "Tournament - fighter ladder", C_MOVE, C_PANEL);
  }
  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Opponents", C_INK, C_PANEL);
}

static constexpr int PICK_ROWH = 26, PICK_TOP = BAND_Y + 4;
int ArenaScreen::pickVisible() const { return (BAND_H - 8) / PICK_ROWH; }

ui::Rect ArenaScreen::pickRowRect(int i) const {
  // screen rect for candidate i given the current scroll (off-screen if not in view)
  return {6, (int16_t)(PICK_TOP + (i - _pickScroll) * PICK_ROWH), 300, 24};
}

void ArenaScreen::drawPick(int player) {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  // Hotseat picks P1 then P2; vs-Computer picks YOUR bot, then WHO you fight.
  char t[28];
  if (_hotseat)         snprintf(t, sizeof(t), "Player %d: pick a bot", player + 1);
  else if (player == 1) snprintf(t, sizeof(t), "Pick your FOE");
  else                  snprintf(t, sizeof(t), "Pick YOUR bot");
  label(g, 6, 3, t, C_ACCENT, textdatum_t::top_left, 2);
  if (!_hotseat) label(g, SCREEN_W - 6, 6, "vs Computer", C_DIM, textdatum_t::top_right);
  int n = (int)_cands.size(), vis = pickVisible();
  if (_pickScroll > n - vis) _pickScroll = (n > vis) ? n - vis : 0;
  if (_pickScroll < 0) _pickScroll = 0;
  for (int i = _pickScroll; i < n && i < _pickScroll + vis; i++) {
    Rect r = pickRowRect(i);
    const Candidate& c = _cands[i];
    g.fillRoundRect(r.x, r.y, r.w, r.h, 4, C_PANEL_HI);  // all rows equally pickable (none greyed)
    uint16_t outline = c.house ? C_PANEL_HI : C_GO;      // your bots outlined green
    if (!_hotseat && player == 1 && i == _pick1) outline = C_ACCENT;  // the suggested foe (tap to accept)
    g.drawRoundRect(r.x, r.y, r.w, r.h, 4, outline);
    assets::drawCharacter(g, r.x + 13, r.cy(), 17, c.avatar, gb::SOUTH);
    label(g, r.x + 30, r.y + 2, c.name.c_str(), c.neuro ? ui::rgb(120, 230, 245) : c.house ? C_INK : C_GO);
    label(g, r.x + 110, r.y + 5, c.style.c_str(), C_DIM);
  }
  // scrollbar (tap top/bottom half to page) when the roster overflows
  if (n > vis) {
    int trackX = SCREEN_W - 8, trackH = vis * PICK_ROWH;
    g.fillRect(trackX, PICK_TOP, 4, trackH, C_PANEL_HI);
    int thumbH = trackH * vis / n; if (thumbH < 10) thumbH = 10;
    int thumbY = PICK_TOP + (trackH - thumbH) * _pickScroll / (n - vis);
    g.fillRect(trackX, thumbY, 4, thumbH, C_ACCENT);
  }
}

bool ArenaScreen::pickScrollTap(int x, int y) {
  int n = (int)_cands.size(), vis = pickVisible();
  if (n <= vis || x < SCREEN_W - 14) return false;
  _pickScroll += (y < PICK_TOP + vis * PICK_ROWH / 2) ? -(vis - 1) : (vis - 1);
  if (_pickScroll > n - vis) _pickScroll = n - vis;
  if (_pickScroll < 0) _pickScroll = 0;
  hal::audio.blip();
  return true;
}

void ArenaScreen::drawHandoff() {
  _phase = Phase::HANDOFF;
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  label(g, SCREEN_W / 2, 60, "Player 1 locked in!", C_GO, textdatum_t::top_center, 2);
  label(g, SCREEN_W / 2, 100, "Pass the device to Player 2", C_INK, textdatum_t::top_center);
  button(g, R_READY, "Ready >", C_ACCENT, C_PANEL);
}

// ---- match ----------------------------------------------------------------
// A kid's SAVED NeuroBot fighter is over-fit to the board it was trained on, so it flails
// on a different battle board. Fine-tune its brain on THIS match board (keeping the kid's
// brain as the starting point — transfer learning at battle time) so it actually competes.
static void adaptNeuroBot(Program& p, const Maze& m, const Pose& start) {
  if (p.brains.empty()) return;
  Maze mm = m; mm.setStart(start);
  distillSolver(p.brains[0], mm, true, 500);
}

// Set up one bot's program for the match. RACE: navigate to the goal (solve/distill/adapt).
// SUMO: there is no goal to race to, so bots must HUNT each other — give every bot the
// hunter (forward + zap-when-enemy-ahead), except a kid's own saved NeuroBot fighter, whose
// trained brain we keep so its Sumo training actually drives it.
void ArenaScreen::setupMatchBot(int pick, const Pose& start, bool sumo) {
  Candidate& c = _cands[pick];
  if (sumo) {
    // Keep each bot's OWN behaviour so matchups stay asymmetric (Rusty charges, Bolt dashes, Vex
    // hunts...). The NEURAL fighters (Neura/Cortex/Volt) get a distilled HUNTER BRAIN -- a real
    // trained net, the neural counterpart to code-Vex -- so battling them shows a learned hunter.
    if (c.neuro && c.prog.brains.empty()) {
      c.prog.clear();
      uint8_t bi = c.prog.addBrain(7u + (uint32_t)c.avatar * 13u);
      distillHunter(c.prog.brains[bi], 7u + (uint32_t)c.avatar * 13u, 2000);
      Node loop = Node::repeatUntil(AT_GOAL); loop.body.push_back(Node::neuro(bi)); c.prog.main.push_back(loop);
    } else if (c.prog.main.empty() && c.prog.brains.empty()) {
      c.prog = hunterProgram();   // Ace / empties: a code hunter so they engage
    }
    return;
  }
  if (c.smart) solveMazeFrom(_maze, start, true, c.prog);
  else if (c.neuro) buildNeuroBot(c.prog, _maze, start, 7u + (uint32_t)c.avatar * 13u);
  else adaptNeuroBot(c.prog, _maze, start);
}

void ArenaScreen::startMatch() {
  hal::audio.stopMusic();  // the board uses step-tick SFX; silence the battle theme
  bool sumo = (_type == MatchType::SUMO);
  if (sumo) {
    // A big, open ring that's fresh EVERY match -- mix the clock so it's not the same series
    // each boot. Local play (vs-Computer / Hotseat) only, so millis() is fine; a network Sumo
    // would pass the SHARED seed instead and both devices would build the identical ring.
    uint32_t seed = (_profile ? _profile->seedBase : 7u) + (uint32_t)millis() + 101u * (++_sumoNonce);
    MazeGen::generateSumoRing(_maze, seed, _s0, _s1);
  } else {
    MazeGen::generateArena(_maze, _profile ? _profile->seedBase + 7u : 7u, _s0, _s1);
  }
  setupMatchBot(_pick0, _s0, sumo);
  setupMatchBot(_pick1, _s1, sumo);
  const Program& p0 = _cands[_pick0].prog;
  const Program& p1 = _cands[_pick1].prog;
  // Sumo has no goal, so it'd otherwise run the full 300 ticks. The seeker bots now brawl and
  // usually KO each other well before this; the cap is a safety net (then ring-control decides).
  // Cup matches use a shorter cap so a stalemate between two weak fighters resolves quickly (by
  // ring-control) instead of dragging a spectator bracket out -- a single battle keeps the full 150.
  int cap = sumo ? (_cup ? 90 : 150) : gb::ARENA_STEP_CAP;
  _arena.setup(&_maze, &p0, &p1, _s0, _s1, _type, cap);
  _phase = Phase::BOARD;
  _running = true;
  _last = 0;
  drawBoard();
}

void ArenaScreen::mazeGeometry(int& tile, int& ox, int& oy) {
  int cols = _maze.cols(), rows = _maze.rows();
  tile = SCREEN_W / cols;
  int th = BAND_H / rows;
  if (th < tile) tile = th;
  ox = (SCREEN_W - tile * cols) / 2;
  oy = BAND_Y + (BAND_H - tile * rows) / 2;
}

void ArenaScreen::drawCell(int r, int c) {
  auto& g = hal::display.gfx();
  int tile, ox, oy; mazeGeometry(tile, ox, oy);
  int x = ox + c * tile, y = oy + r * tile;
  Tile t = _maze.at(r, c);
  uint16_t bg = ((r + c) & 1) ? C_FLOOR : C_FLOOR2;
  if (t == WALL) bg = C_WALL; else if (t == PIT) bg = C_PIT;
  g.fillRect(x, y, tile - 1, tile - 1, bg);
  if (t == PIT) g.fillCircle(x + tile / 2, y + tile / 2, tile / 3, C_BG);
  if (_type == MatchType::RACE && _maze.isGoal(r, c)) {
    g.fillCircle(x + tile / 2, y + tile / 2, tile / 3, C_ACCENT);
    label(g, x + tile / 2, y + tile / 2, "G", C_BG, textdatum_t::middle_center);
  }
}

// Erase a bot that just left cell (r,c). The robot's facing arrow + antenna overhang into the
// neighbouring tiles and the inter-tile gaps, so we clear the bot's whole footprint to the grid
// colour, then repaint this cell and its 8 neighbours -- no leftover yellow specks as it moves.
void ArenaScreen::eraseBotAt(int r, int c) {
  auto& g = hal::display.gfx();
  int tile, ox, oy; mazeGeometry(tile, ox, oy);
  int x = ox + c * tile, y = oy + r * tile;
  g.fillRect(x - tile / 2, y - tile / 2, tile * 2, tile * 2, C_BG);   // wipe footprint + overhang + gaps
  for (int dr = -1; dr <= 1; dr++)
    for (int dc = -1; dc <= 1; dc++) {
      int rr = r + dr, cc = c + dc;
      if (rr >= 0 && rr < _maze.rows() && cc >= 0 && cc < _maze.cols()) drawCell(rr, cc);
    }
}

void ArenaScreen::drawBot(int i, const Pose& p, int avatar) {
  auto& g = hal::display.gfx();
  int tile, ox, oy; mazeGeometry(tile, ox, oy);
  int cx = ox + p.col * tile + tile / 2, cy = oy + p.row * tile + tile / 2;
  assets::drawCharacter(g, cx, cy, tile, avatar, p.facing);
  // Sumo health bar above the bot: a pip per HP, green = left, grey = knocked off.
  if (_type == MatchType::SUMO) {
    int hp = _arena.hp(i), pw = (tile - 6) / SUMO_HP, hy = cy - tile / 2 + 1;
    if (pw < 3) pw = 3;
    int total = SUMO_HP * (pw + 1) - 1, hx = cx - total / 2;
    for (int k = 0; k < SUMO_HP; k++)
      g.fillRect(hx + k * (pw + 1), hy, pw, 3, k < hp ? C_GO : C_LOCK);
  }
}

// Big HP/score strip across the top bar so the brawl is easy to read at a glance: your name +
// a row of large pips (green = HP left) on the left, the foe's pips (red) + name on the right.
void ArenaScreen::drawScore() {
  auto& g = hal::display.gfx();
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  if (_type != MatchType::SUMO) {
    char hdr[40];
    snprintf(hdr, sizeof(hdr), "Race: %s vs %s", _cands[_pick0].name.c_str(), _cands[_pick1].name.c_str());
    label(g, 6, 4, hdr, C_ACCENT);
    return;
  }
  const char* n0 = _hotseat ? "P1" : "You";
  const char* n1 = _hotseat ? "P2" : _cands[_pick1].name.c_str();
  const int pw = 13, ph = 11, gap = 3, py = (TOPBAR_H - ph) / 2;
  label(g, 4, 6, n0, C_GO);
  int lx = 40;
  for (int k = 0; k < SUMO_HP; k++)
    g.fillRoundRect(lx + k * (pw + gap), py, pw, ph, 2, k < _arena.hp(0) ? C_GO : C_LOCK);
  int rw = SUMO_HP * (pw + gap) - gap;
  int rx = SCREEN_W - 4 - 36 - rw;
  for (int k = 0; k < SUMO_HP; k++)
    g.fillRoundRect(rx + k * (pw + gap), py, pw, ph, 2, k < _arena.hp(1) ? C_BAD : C_LOCK);
  label(g, SCREEN_W - 4, 6, n1, C_BAD, textdatum_t::top_right);
}

// A short burst (expanding ring + word) over a bot that just took a hit, so a 6yo sees WHO got
// zapped/knocked. Drawn for one animation frame; the next tick's drawCell/drawBot paints over it.
void ArenaScreen::drawHit(int i, const char* txt, uint16_t col) {
  auto& g = hal::display.gfx();
  int tile, ox, oy; mazeGeometry(tile, ox, oy);
  Pose p = _arena.pose(i);
  int cx = ox + p.col * tile + tile / 2, cy = oy + p.row * tile + tile / 2;
  g.drawCircle(cx, cy, tile / 2, col);
  g.drawCircle(cx, cy, tile / 2 + 3, col);
  for (int a = 0; a < 8; a++) {                 // spark rays
    float ang = a * 0.785398f;
    g.drawLine(cx, cy, cx + (int)(cosf(ang) * (tile / 2 + 5)), cy + (int)(sinf(ang) * (tile / 2 + 5)), col);
  }
  label(g, cx, cy - tile / 2 - 8, txt, col, textdatum_t::bottom_center);
}

void ArenaScreen::drawBoard() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  drawScore();
  for (int r = 0; r < _maze.rows(); r++)
    for (int c = 0; c < _maze.cols(); c++) drawCell(r, c);
  drawBot(0, _arena.pose(0), _cands[_pick0].avatar);
  drawBot(1, _arena.pose(1), _cands[_pick1].avatar == _cands[_pick0].avatar
                                ? (_cands[_pick1].avatar + 4) % 8 : _cands[_pick1].avatar);
  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
}

void ArenaScreen::finishOverlay() {
  auto& g = hal::display.gfx();
  const char* msg = "Draw!"; uint16_t col = C_ACCENT;
  // vs-Computer: bot 0 is the player, bot 1 is the AI -> "You / Computer"; Hotseat: P1/P2.
  switch (_arena.outcome()) {
    case ArenaOutcome::BOT0: msg = _hotseat ? "Player 1 wins!" : "You win!"; col = C_GO; hal::led.green(); hal::audio.win();
      if (_profile) _profile->stats.arenaWins++; break;
    case ArenaOutcome::BOT1: msg = _hotseat ? "Player 2 wins!" : "Computer wins!"; col = C_FUNC; hal::led.red(); hal::audio.fail(); break;
    default: break;
  }
  int w = 220, h = 78, x = (SCREEN_W - w) / 2, y = (SCREEN_H - h) / 2;
  g.fillRoundRect(x, y, w, h, 10, C_PANEL);
  g.drawRoundRect(x, y, w, h, 10, col);
  label(g, SCREEN_W / 2, y + 22, msg, col, textdatum_t::middle_center, 2);
  // vs-Computer + NeuroBot: offer to retrain your fighter for this matchup, not just bounce out.
  bool canTrain = !_hotseat && _profile && _profile->unlocks.neuro;
  if (canTrain) {
    button(g, R_AGAIN3, "Again", C_GO, C_PANEL_HI);          // replay the SAME matchup
    button(g, R_TRAIN3, "Train >", ui::rgb(120, 230, 245), C_PANEL_HI);  // go improve your fighter
    button(g, R_EXIT3, "Exit", C_INK, C_PANEL_HI);
  } else {
    button(g, R_AGAIN, "Play again", C_GO, C_PANEL_HI);
    button(g, R_RESULT_EXIT, "Exit", C_INK, C_PANEL_HI);
  }
}

// ---- input ----------------------------------------------------------------
void ArenaScreen::debugStep() {
  if (_phase != Phase::BOARD) return;
  _running = false;  // pause auto so frames can be captured one tick at a time
  Pose b0 = _arena.pose(0), b1 = _arena.pose(1);
  ArenaOutcome o = _arena.tick();
  eraseBotAt(b0.row, b0.col); eraseBotAt(b1.row, b1.col);
  if (_arena.alive(0)) drawBot(0, _arena.pose(0), _cands[_pick0].avatar);
  if (_arena.alive(1)) drawBot(1, _arena.pose(1), _cands[_pick1].avatar);
  if (o != ArenaOutcome::RUNNING) onMatchEnd();
}

app::Signal ArenaScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  bool tap = _tap.tapped(tp, now, tx, ty);

  if (_phase == Phase::BOARD && _running && _last == 0) {
    _last = now;  // arm the timer; the first tick is one interval later (lets capture start at tick 0)
  } else if (_phase == Phase::BOARD && _running && now - _last >= (uint32_t)(_cup ? 90 : 250)) {
    _last = now;
    Pose b0 = _arena.pose(0), b1 = _arena.pose(1);
    int hp0 = _arena.hp(0), hp1 = _arena.hp(1);
    bool al0 = _arena.alive(0), al1 = _arena.alive(1);
    ArenaOutcome o = _arena.tick();
    eraseBotAt(b0.row, b0.col); eraseBotAt(b1.row, b1.col);
    if (_arena.alive(0)) drawBot(0, _arena.pose(0), _cands[_pick0].avatar);
    if (_arena.alive(1)) drawBot(1, _arena.pose(1), _cands[_pick1].avatar);
    if (_type == MatchType::SUMO) {
      drawScore();                                  // refresh the big HP strip (it just changed)
      // pop a burst over whoever took a hit this tick: knocked off the ring -> OUT!, else ZAP!
      bool hit = false;
      for (int i = 0; i < 2; i++) {
        bool wasAlive = i ? al1 : al0; int wasHp = i ? hp1 : hp0;
        if (wasAlive && !_arena.alive(i))      { drawHit(i, "OUT!", C_FUNC); hit = true; }
        else if (_arena.hp(i) < wasHp)         { drawHit(i, "ZAP!", C_BAD);  hit = true; }
      }
      hit ? hal::audio.fail() : hal::audio.tick();  // a zap/knock gets its own sting
    } else {
      hal::audio.tick();
    }
    if (o != ArenaOutcome::RUNNING) { _running = false; onMatchEnd(); }
  }

  if (!tap) return app::Signal::NONE;

  switch (_phase) {
    case Phase::MENU:
      if (R_BACK.contains(tx, ty)) return app::Signal::BACK;
      if (R_OPP_RADIO.contains(tx, ty)) return app::Signal::GOTO_RADIO;  // radio screen has its own battle/trade
      else if (R_OPP_AI.contains(tx, ty))  { _hotseat = false; drawGameType(); }
      else if (R_OPP_HOT.contains(tx, ty)) { _hotseat = true;  drawGameType(); }
      break;
    case Phase::GAMETYPE:
      if (R_BACK.contains(tx, ty)) { drawMenu(); break; }  // < Opponents
      if (R_G1.contains(tx, ty)) {  // Race (both branches)
        _type = MatchType::RACE; buildCandidates(false); _pickScroll = 0; _phase = Phase::PICK1; drawPick(0);
      } else if (R_G2.contains(tx, ty)) {  // Battle — unlocks at Sense (code a seeker) / NeuroBot (train one)
        if (!(_profile && _profile->unlocks.sense)) { hal::audio.fail(); }
        else if (!_profile->library.empty()) {  // pick a trained OR coded battle bot
          _type = MatchType::SUMO; buildCandidates(true);
          _pickScroll = 0; _phase = Phase::PICK1; drawPick(0);
        } else if (_profile->unlocks.neuro) {    // no bot yet, but can train one -> the NeuroBot trainer
          return app::Signal::GOTO_ARENA_TRAIN;
        } else {                                 // pre-NeuroBot: make a coded seeker first
          hal::audio.fail();
          auto& g = hal::display.gfx();
          label(g, SCREEN_W / 2, BOTBAR_Y - 16, "Code a fighter in CodeLab, then Save to My Bots!",
                C_ACCENT, textdatum_t::bottom_center);
        }
      } else if (_hotseat && R_G3.contains(tx, ty)) {
        return app::Signal::GOTO_PUZZLE;
      } else if (_hotseat && R_G4.contains(tx, ty)) {
        return app::Signal::GOTO_CHALLENGE;
      } else if (!_hotseat && _profile && _profile->unlocks.neuro && R_G3.contains(tx, ty)) {
        return app::Signal::GOTO_ARENA_TRAIN;
      } else if (!_hotseat && _profile && _profile->unlocks.neuro && _profile->library.size() >= 2
                 && R_G4.contains(tx, ty)) {
        _phase = Phase::TCHOICE; drawTChoice();        // pick Ladder (round-robin) or Cup (bracket)
      }
      break;
    case Phase::TCHOICE:
      if (R_BACK.contains(tx, ty)) { drawGameType(); }
      else if (R_G1.contains(tx, ty)) {                // Ladder: instant round-robin leaderboard
        auto& g = hal::display.gfx();
        label(g, SCREEN_W / 2, SCREEN_H / 2, "Running tournament...", C_ACCENT, textdatum_t::middle_center);
        runTournament();
      } else if (R_G2.contains(tx, ty)) {              // Cup: watchable single-elimination bracket
        startCup();
      }
      break;
    case Phase::TOURNEY:
      if (R_BACK.contains(tx, ty)) drawGameType();
      break;
    case Phase::CUP:
      if (_cupBracket.done()) {                        // champion shown (bottom-bar buttons)
        if (R_CUP_AGAIN.contains(tx, ty)) startCup();  // run it again
        else if (R_CUP_EXIT.contains(tx, ty) || R_BACK.contains(tx, ty)) drawGameType();
      } else {
        if (R_AGAIN.contains(tx, ty)) cupAdvanceToNextMatch();   // Start / Next match
        else if (R_RESULT_EXIT.contains(tx, ty)) { _cup = false; drawGameType(); }  // Quit
      }
      break;
    case Phase::PICK1:
      if (pickScrollTap(tx, ty)) { drawPick(0); break; }
      for (int i = 0; i < (int)_cands.size(); i++) {
        if (pickRowRect(i).contains(tx, ty)) {
          _pick0 = i; hal::audio.blip();
          // Hotseat: hand off to Player 2. vs-Computer: now choose WHO to fight (was auto-Vex) --
          // a default lands on Vex/Bolt so it's still one extra tap, not a chore.
          if (_hotseat) { drawHandoff(); }
          else {
            bool sumo = (_type == MatchType::SUMO);
            int opp = houseBotIndex(sumo ? "Vex" : "Bolt");
            if (opp == _pick0) opp = houseBotIndex(sumo ? "Ace" : "Rusty");
            _pick1 = (opp >= 0) ? opp : 0;        // sensible default, highlighted on the foe-pick list
            _pickScroll = 0; _phase = Phase::PICK2; drawPick(1);
          }
          return app::Signal::NONE;
        }
      }
      break;
    case Phase::HANDOFF:
      if (R_READY.contains(tx, ty)) { _pickScroll = 0; _phase = Phase::PICK2; drawPick(1); }
      break;
    case Phase::PICK2:
      if (pickScrollTap(tx, ty)) { drawPick(1); break; }
      for (int i = 0; i < (int)_cands.size(); i++) {
        if (pickRowRect(i).contains(tx, ty)) { _pick1 = i; hal::audio.blip(); startMatch(); return app::Signal::NONE; }
      }
      break;
    case Phase::BOARD:
      if (R_BACK.contains(tx, ty)) { hal::led.off(); return app::Signal::BACK; }
      break;
    case Phase::DONE: {
      bool canTrain = !_hotseat && _profile && _profile->unlocks.neuro;
      if (canTrain && R_TRAIN3.contains(tx, ty)) {  // go retrain a better fighter, then come back
        hal::led.off(); return app::Signal::GOTO_ARENA_TRAIN;
      } else if ((canTrain ? R_AGAIN3 : R_AGAIN).contains(tx, ty)) {  // replay the SAME matchup
        hal::led.off();
        if (_pick0 >= 0 && _pick1 >= 0) startMatch();
        else { _pickScroll = 0; _phase = Phase::PICK1; drawPick(0); }
      } else if ((canTrain ? R_EXIT3 : R_RESULT_EXIT).contains(tx, ty) || R_BACK.contains(tx, ty)) {
        hal::led.off(); return app::Signal::BACK;
      }
      break;
    }
  }
  return app::Signal::NONE;
}

}  // namespace screens
