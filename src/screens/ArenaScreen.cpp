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
#include "game/ProgramJson.h"
#include "net/TourneyNet.h"
#include <ArduinoJson.h>

using namespace ui;
using namespace gb;

namespace screens {

// AST <-> JSON for putting a fighter on the wire (same shape as the radio trade).
static String programToJsonString(const Program& p) {
  JsonDocument doc; programToJson(p, doc.to<JsonObject>());
  String s; serializeJson(doc, s); return s;
}
static Program programFromJsonString(const String& s) {
  Program p; JsonDocument doc;
  if (deserializeJson(doc, s) == DeserializationError::Ok) programFromJson(doc.as<JsonObjectConst>(), p);
  return p;
}

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
static const Rect R_OPP_AI    = {20, 46,  280, 34};
static const Rect R_OPP_HOT   = {20, 86,  280, 34};
static const Rect R_OPP_RADIO = {20, 126, 280, 34};
static const Rect R_OPP_ROOM  = {20, 166, 280, 34};   // multi-device tournament room
// net lobby
static const Rect R_ROOM_HOST  = {40, 72,  240, 38};
static const Rect R_ROOM_JOIN  = {40, 122, 240, 38};
static const Rect R_ROOM_START = {112, (int16_t)(BOTBAR_Y + 2), 96, 26};
static const Rect R_ROOM_DISC  = {212, (int16_t)(BOTBAR_Y + 2), 102, 26};  // host: cycle the room discipline
// Level 2 — pick the GAME (depends on the opponent); vertical slots reused per branch. Tightened
// to fit FIVE rows (Race / Battle / Soccer / + two branch-specific) above the bottom bar.
static const Rect R_G1 = {20, 46,  280, 28};
static const Rect R_G2 = {20, 77,  280, 28};
static const Rect R_G3 = {20, 108, 280, 28};
static const Rect R_G4 = {20, 139, 280, 28};
static const Rect R_G5 = {20, 170, 280, 28};
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
// participant-picker bottom bar: Select all + Format >
static const Rect R_TALL  = {116, (int16_t)(BOTBAR_Y + 2), 64,  26};
static const Rect R_TNEXT = {188, (int16_t)(BOTBAR_Y + 2), 126, 26};
// standings / breakdown pager (5 per page, like the lessons hub)
static const Rect R_SPREV = {150, (int16_t)(BOTBAR_Y + 2), 78, 26};
static const Rect R_SNEXT = {234, (int16_t)(BOTBAR_Y + 2), 80, 26};
static constexpr int TPER_PAGE = 5;
static constexpr int PICK_ROWH = 26, PICK_TOP = BAND_Y + 4;  // pick-list geometry (shared by PICK + TPICK)

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
  // SOCCER house team: soccer-player-style names, each a pre-trained dribbler (setupMatchBot
  // distills a soccer brain seeded by the avatar, so they play well out of the box and differ).
  if (_type == MatchType::SOCCER) {
    _cands.push_back({"Strika",  Program{}, 7, "the striker",     true, false, true});
    _cands.push_back({"Dribbla", Program{}, 2, "tricky feet",     true, false, true});
    _cands.push_back({"Volley",  Program{}, 4, "the big boot",    true, false, true});
    _cands.push_back({"Nutmeg",  Program{}, 1, "skill-move star", true, false, true});
    _cands.push_back({"Boots",   Program{}, 3, "speedy winger",   true, false, true});
    return;
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

// Jump straight into the multi-device Room lobby (the Radio screen hands off here via GOTO_ROOM).
void ArenaScreen::enterRoom() {
  net::tourney().begin(); net::tourney().leave();
  _roomN = -1; _type = MatchType::SUMO; _phase = Phase::NETLOBBY;
  drawNetLobby();
}

// Round-robin battle ladder: every fighter plays every other (home AND away on one fair ring so
// start-position bias cancels), wins are tallied, and the field is ranked. Headless + deterministic.
void ArenaScreen::runTournament(const std::vector<int>& parts) {
  int N = (int)parts.size();
  bool sumo = (_type == MatchType::SUMO), soccer = (_type == MatchType::SOCCER);
  uint32_t base = _profile ? _profile->seedBase : 7u;
  genMatchBoard(soccer ? base + 55u : sumo ? base + 1234u : base + 31u);
  MatchType mt = _type;
  int cap = soccer ? 300 : sumo ? 150 : 200;
  // A race is start-dependent (a solver builds its path from where it stands), so prepare each
  // fighter's program for BOTH start poses; sumo/soccer programs are start-agnostic (reuse one).
  bool startAgnostic = (sumo || soccer);
  std::vector<Program> pa(N), pb(N);
  for (int k = 0; k < N; k++) {
    setupMatchBot(parts[k], _s0); pa[k] = _cands[parts[k]].prog;
    if (startAgnostic) { pb[k] = pa[k]; }
    else { setupMatchBot(parts[k], _s1); pb[k] = _cands[parts[k]].prog; }
  }
  std::vector<int> w(N, 0), l(N, 0);
  _tN = N; _tOrder.assign(parts.begin(), parts.end());
  _tWin.assign((size_t)N * N, 0);
  for (int i = 0; i < N; i++)
    for (int j = 0; j < N; j++) {
      if (i == j) continue;
      Arena ar; ar.setup(&_maze, &pa[i], &pb[j], _s0, _s1, mt, cap);
      if (soccer) ar.configSoccer(_ball, _goal0, _goal1);
      ArenaOutcome o = ar.run();
      // _tWin[a*N+b] accumulates how many times a beat b (each pair plays home AND away -> 0..2)
      if (o == ArenaOutcome::BOT0) { w[i]++; l[j]++; _tWin[i * N + j]++; }
      else if (o == ArenaOutcome::BOT1) { w[j]++; l[i]++; _tWin[j * N + i]++; }
    }
  _standings.clear();
  for (int i = 0; i < N; i++) _standings.push_back({parts[i], w[i], l[i], i});
  std::sort(_standings.begin(), _standings.end(),
            [](const Standing& a, const Standing& b) { return a.w != b.w ? a.w > b.w : a.l < b.l; });
  _standPage = 0; _breakdownMi = -1;
  _phase = Phase::TOURNEY;
  drawTourney();
}

void ArenaScreen::drawTourney() {
  if (_breakdownMi >= 0) { drawBreakdown(); return; }   // a row was tapped -> its record
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Standings", C_ACCENT, textdatum_t::top_left, 2);
  int n = (int)_standings.size();
  int npages = (n + TPER_PAGE - 1) / TPER_PAGE; if (npages < 1) npages = 1;
  if (_standPage >= npages) _standPage = npages - 1;
  char hd[28]; snprintf(hd, sizeof(hd), "wins-losses  (%d/%d)", _standPage + 1, npages);
  label(g, SCREEN_W - 6, 6, hd, C_DIM, textdatum_t::top_right);
  int rowh = 30, top = BAND_Y + 6;
  for (int r = 0; r < TPER_PAGE; r++) {
    int i = _standPage * TPER_PAGE + r;
    if (i >= n) break;
    const Standing& s = _standings[i];
    int y = top + r * rowh;
    uint16_t medal = (i == 0) ? C_ACCENT : (i == 1) ? C_MOVE : (i == 2) ? C_WALL : C_PANEL_HI;
    g.fillRoundRect(8, y, SCREEN_W - 16, rowh - 4, 4, C_PANEL);
    g.drawRoundRect(8, y, SCREEN_W - 16, rowh - 4, 4, medal);
    char row[40]; snprintf(row, sizeof(row), "%d.  %s", i + 1, _cands[s.cand].name.c_str());
    label(g, 16, y + 7, row, i < 3 ? C_GO : C_INK);
    char wl[16]; snprintf(wl, sizeof(wl), "%d - %d  >", s.w, s.l);
    label(g, SCREEN_W - 16, y + 7, wl, C_DIM, textdatum_t::top_right);
  }
  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
  button(g, R_SPREV, "< Prev", _standPage > 0 ? C_ACCENT : C_LOCK, C_PANEL);
  button(g, R_SNEXT, "Next >", _standPage < npages - 1 ? C_ACCENT : C_LOCK, C_PANEL);
}

// One fighter's record: who it beat and who beat it (tap a standing to open). Paginated.
void ArenaScreen::drawBreakdown() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  int mi = _breakdownMi;
  int self = (mi >= 0 && mi < (int)_tOrder.size()) ? _tOrder[mi] : 0;
  char ttl[28]; snprintf(ttl, sizeof(ttl), "%.14s", _cands[self].name.c_str());
  label(g, 6, 3, ttl, C_ACCENT, textdatum_t::top_left, 2);
  // collect opponents (skip self), count W/L
  std::vector<int> opp;
  for (int j = 0; j < _tN; j++) if (j != mi) opp.push_back(j);
  int w = 0, l = 0;
  for (int j : opp) { w += _tWin[mi * _tN + j]; l += _tWin[j * _tN + mi]; }
  char hd[20]; snprintf(hd, sizeof(hd), "%d - %d", w, l);
  label(g, SCREEN_W - 6, 6, hd, C_GO, textdatum_t::top_right);
  int n = (int)opp.size();
  int npages = (n + TPER_PAGE - 1) / TPER_PAGE; if (npages < 1) npages = 1;
  if (_bdPage >= npages) _bdPage = npages - 1;
  int rowh = 30, top = BAND_Y + 6;
  for (int r = 0; r < TPER_PAGE; r++) {
    int k = _bdPage * TPER_PAGE + r;
    if (k >= n) break;
    int j = opp[k]; int mw = _tWin[mi * _tN + j], ml = _tWin[j * _tN + mi];
    int y = top + r * rowh;
    g.fillRoundRect(8, y, SCREEN_W - 16, rowh - 4, 4, C_PANEL);
    const char* verb = mw > ml ? "beat" : mw < ml ? "lost to" : "split";
    uint16_t col = mw > ml ? C_GO : mw < ml ? C_BAD : C_DIM;
    char row[40]; snprintf(row, sizeof(row), "%s  %.12s", verb, _cands[_tOrder[j]].name.c_str());
    label(g, 16, y + 7, row, col);
    char rec[8]; snprintf(rec, sizeof(rec), "%d-%d", mw, ml);
    label(g, SCREEN_W - 16, y + 7, rec, C_DIM, textdatum_t::top_right);
  }
  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Standings", C_INK, C_PANEL);
  button(g, R_SPREV, "< Prev", _bdPage > 0 ? C_ACCENT : C_LOCK, C_PANEL);
  button(g, R_SNEXT, "Next >", _bdPage < npages - 1 ? C_ACCENT : C_LOCK, C_PANEL);
}

// ---- Cup: single-elimination bracket, watched match by match -------------------------------
void ArenaScreen::startCup(const std::vector<int>& parts) {
  _cupPlayers.clear();
  for (int p : parts) { if ((int)_cupPlayers.size() < gb::BRACKET_MAX) _cupPlayers.push_back(p); }
  // _type (Race / Sumo / Soccer) is set by the discipline chooser (or buildRosterField for net Cups).
  _cupBracket.init((int)_cupPlayers.size());
  _cupLog.clear();
  _cup = true; _cupMatch = -1; _netCup = false; _cupAutoAt = 0;   // local by default; buildRosterField sets net
  _phase = Phase::CUP; drawCupCard();             // opening card; tap to start the first match
}

void ArenaScreen::cupAdvanceToNextMatch() {
  while (true) {
    if (_cupBracket.done()) { _cup = false; _cupMatch = -1; _phase = Phase::CUP; drawCupCard(); return; }
    int next = -1;
    for (int m = 0; m < _cupBracket.matchCount(); m++) {
      int a, b; _cupBracket.pair(m, a, b);
      if (b < 0) { if (_cupBracket.win[m] < 0) { _cupBracket.reportWinner(m, a);   // bye auto-advances
                     _cupLog.push_back({(int8_t)_cupBracket.round, (int8_t)a, (int8_t)-1, (int8_t)a, 0, 0}); } }
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
    drawBracket();   // the whole bracket as round-columns (history + the current round)
  }
  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  if (done) {
    button(g, R_CUP_AGAIN, "Again", C_GO, C_PANEL);    // bottom bar, clear of the champion art
    button(g, R_CUP_EXIT, "Exit", C_INK, C_PANEL);
  } else {  // bottom bar (clear of the bracket, which fills the band)
    button(g, R_CUP_AGAIN, _cupMatch < 0 ? "Start >" : "Next >", C_GO, C_PANEL);
    button(g, R_CUP_EXIT, "Quit", C_INK, C_PANEL);
  }
}

// The whole bracket as left-to-right round columns: resolved rounds come from the log, the live
// round from the bracket. Each match is a little two-name box; the winner's name turns green.
void ArenaScreen::drawBracket() {
  auto& g = hal::display.gfx();
  int n = (int)_cupPlayers.size();
  int slots = 1; while (slots < n) slots <<= 1;          // field padded to a power of two
  int R = 0; for (int s = slots; s > 1; s >>= 1) R++;    // number of rounds
  if (R < 1) R = 1;
  auto nm = [&](int pid, char* d, int dn) {
    if (pid < 0) snprintf(d, dn, "bye");
    else if (pid < (int)_cupPlayers.size()) snprintf(d, dn, "%.7s", _cands[_cupPlayers[pid]].name.c_str());
    else snprintf(d, dn, "?");
  };
  int colW = (SCREEN_W - 6) / R;
  int top = BAND_Y + 14, bandH = BOTBAR_Y - top - 2;
  static const char* RLBL[4] = {"R1", "R2", "R3", "Final"};
  for (int r = 0; r < R; r++) {
    int mc = slots >> (r + 1); if (mc < 1) mc = 1;
    int colX = 3 + r * colW;
    label(g, colX + (colW - 4) / 2, BAND_Y + 2, (R - r <= 1) ? "Final" : (r < 3 ? RLBL[r] : "R?"),
          C_DIM, textdatum_t::top_center);
    int slotH = bandH / mc;
    for (int m = 0; m < mc; m++) {
      int a = -2, b = -2, win = -1, ga = -1, gb = -1;
      if (r < _cupBracket.round) {                       // resolved round -> from the log
        int cnt = 0;
        for (auto& gm : _cupLog) if (gm.round == r) { if (cnt == m) { a = gm.a; b = gm.b; win = gm.win; ga = gm.ga; gb = gm.gb; break; } cnt++; }
      } else if (r == _cupBracket.round && !_cupBracket.done()) {
        _cupBracket.pair(m, a, b); win = _cupBracket.win[m];
      }
      int boxY = top + m * slotH + 1, boxH = slotH - 3; if (boxH > 28) boxH = 28;
      g.fillRoundRect(colX, boxY, colW - 4, boxH, 3, C_PANEL);
      g.drawRoundRect(colX, boxY, colW - 4, boxH, 3, C_PANEL_HI);
      if (a > -2) {
        char na[10], nb[10]; nm(a, na, sizeof(na)); nm(b, nb, sizeof(nb));
        label(g, colX + 4, boxY + 2, na, (win == a) ? C_GO : C_INK);
        if (boxH >= 16) label(g, colX + 4, boxY + boxH - 10, nb, (win == b) ? C_GO : (b < 0 ? C_DIM : C_INK));
        // soccer: the scoreline at the right edge (0-0 win = "won on position", not a scored goal)
        if (_type == MatchType::SOCCER && ga >= 0 && b >= 0) {
          char s[4]; snprintf(s, sizeof(s), "%d", ga); label(g, colX + colW - 7, boxY + 2, s, (win == a) ? C_GO : C_DIM, textdatum_t::top_right);
          if (boxH >= 16) { snprintf(s, sizeof(s), "%d", gb); label(g, colX + colW - 7, boxY + boxH - 10, s, (win == b) ? C_GO : C_DIM, textdatum_t::top_right); }
        }
      }
    }
  }
}

void ArenaScreen::onMatchEnd() {
  if (_cup && _cupMatch >= 0) {
    int a, b; _cupBracket.pair(_cupMatch, a, b);
    int winner = (_arena.outcome() == ArenaOutcome::BOT1) ? b : a;   // BOT0 or DRAW -> 'a' (tiebreak)
    _cupBracket.reportWinner(_cupMatch, winner);
    int8_t ga = (_type == MatchType::SOCCER) ? (int8_t)_arena.goals(0) : 0;   // the scoreline (soccer)
    int8_t gb = (_type == MatchType::SOCCER) ? (int8_t)_arena.goals(1) : 0;
    _cupLog.push_back({(int8_t)_cupBracket.round, (int8_t)a, (int8_t)b, (int8_t)winner, ga, gb});
    _phase = Phase::CUP; drawCupCard();              // show the updated bracket; tap to continue
  } else {
    _phase = Phase::DONE; finishOverlay();
  }
}

// Tournament step 1: what kind of contest -- a maze race or a battle.
void ArenaScreen::drawTDisc() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Tournament", C_ACCENT, textdatum_t::top_left, 2);
  label(g, SCREEN_W / 2, 36, "What kind of contest?", C_INK, textdatum_t::top_center);
  button(g, R_G1, "Maze race - first to the goal", C_GO, C_PANEL);
  button(g, R_G2, "Soccer - shoot it into the goal", ui::rgb(120, 230, 245), C_PANEL);
  button(g, R_G3, "Battle - last bot standing", C_FUNC, C_PANEL);
  label(g, SCREEN_W / 2, 210, "race a maze, dribble a ball, or brawl on a ring", C_DIM, textdatum_t::top_center);
  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
}

// Tournament step 2: which fighters take part (checkbox each, + Select all). Reuses the pick-list
// geometry; a tap toggles a fighter in/out so the field never has bots that don't fit the contest.
void ArenaScreen::drawTPick() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Pick fighters", C_ACCENT, textdatum_t::top_left, 2);
  int sel = 0; for (int i = 0; i < (int)_cands.size(); i++) if ((_tSelMask >> i) & 1u) sel++;
  char hd[20]; snprintf(hd, sizeof(hd), "%d in", sel);
  label(g, SCREEN_W - 6, 6, hd, sel >= 2 ? C_GO : C_BAD, textdatum_t::top_right);
  int n = (int)_cands.size(), vis = pickVisible();
  if (_tScroll > n - vis) _tScroll = (n > vis) ? n - vis : 0;
  if (_tScroll < 0) _tScroll = 0;
  for (int i = _tScroll; i < n && i < _tScroll + vis; i++) {
    Rect r = {6, (int16_t)(PICK_TOP + (i - _tScroll) * PICK_ROWH), 300, 24};
    const Candidate& c = _cands[i];
    bool on = (_tSelMask >> i) & 1u;
    g.fillRoundRect(r.x, r.y, r.w, r.h, 4, C_PANEL_HI);
    g.drawRoundRect(r.x, r.y, r.w, r.h, 4, on ? C_GO : C_PANEL_HI);
    // checkbox
    g.drawRoundRect(r.x + 4, r.y + 4, 16, 16, 3, on ? C_GO : C_DIM);
    if (on) { g.fillRoundRect(r.x + 7, r.y + 7, 10, 10, 2, C_GO); }
    assets::drawCharacter(g, r.x + 34, r.cy(), 16, c.avatar, gb::SOUTH);
    label(g, r.x + 50, r.y + 2, c.name.c_str(), c.neuro ? ui::rgb(120, 230, 245) : c.house ? C_INK : C_GO);
    label(g, r.x + 130, r.y + 5, c.style.c_str(), C_DIM);
  }
  if (n > vis) {  // scrollbar (tap top/bottom half to page)
    int trackX = SCREEN_W - 8, trackH = vis * PICK_ROWH;
    g.fillRect(trackX, PICK_TOP, 4, trackH, C_PANEL_HI);
    int thumbH = trackH * vis / n; if (thumbH < 10) thumbH = 10;
    int thumbY = PICK_TOP + (trackH - thumbH) * _tScroll / (n - vis);
    g.fillRect(trackX, thumbY, 4, thumbH, C_ACCENT);
  }
  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_TALL,  "All",      C_ACCENT, C_PANEL);
  button(g, R_TNEXT, "Format >", sel >= 2 ? C_GO : C_LOCK, C_PANEL);
  button(g, R_BACK,  "< Back",   C_INK, C_PANEL);
}

void ArenaScreen::drawTChoice() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Tournament", C_ACCENT, textdatum_t::top_left, 2);
  label(g, SCREEN_W / 2, 36, "Pick a format", C_INK, textdatum_t::top_center);
  button(g, R_G1, "Ladder", C_MOVE, C_PANEL);
  label(g, SCREEN_W / 2, 80, "everyone plays everyone; ranked by wins", C_DIM, textdatum_t::top_center);
  button(g, R_G3, "Cup", C_ACCENT, C_PANEL);
  label(g, SCREEN_W / 2, 142, "knockout bracket - lose and you're out", C_DIM, textdatum_t::top_center);
  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
}

void ArenaScreen::beginQuickBattle(Profile* profile, int libIdx, const char* oppName, MatchType type) {
  _profile = profile;
  _hotseat = false;
  _type = type;
  buildCandidates(type != MatchType::RACE);    // library entries lead the list, so libIdx maps directly
  int nlib = _profile ? (int)_profile->library.size() : 0;
  _pick0 = (libIdx >= 0 && libIdx < nlib) ? libIdx : 0;
  _pick1 = -1;
  for (int i = 0; i < (int)_cands.size(); i++) if (_cands[i].name == oppName) { _pick1 = i; break; }
  if (_pick1 < 0 && type == MatchType::SOCCER) _pick1 = houseBotIndex("Strika");  // a soccer house player
  if (_pick1 < 0) _pick1 = houseBotIndex("Vex");                 // opponent not in this roster -> Vex
  if (_pick1 < 0 || _pick1 == _pick0) _pick1 = houseBotIndex(type == MatchType::SOCCER ? "Dribbla" : "Ace");
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
  label(g, SCREEN_W / 2, 38, "What do you want to do?", C_INK, textdatum_t::top_center);
  button(g, R_OPP_AI,    "vs Computer", C_GO, C_PANEL);
  button(g, R_OPP_HOT,   "Hotseat - 2 players, 1 device", C_FUNC, C_PANEL);
  button(g, R_OPP_RADIO, "Radio - friend: battle/trade/room", C_MOVE, C_PANEL);
  // Train a fighter is a NeuroBot prep activity (not a match), so it lives here, not under vs-Computer.
  if (_profile && _profile->unlocks.neuro)
    button(g, R_OPP_ROOM, "Train a fighter (NeuroBot)", ui::rgb(120, 230, 245), C_PANEL);
  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
}

// The room lobby: host or join, then watch the roster fill as nearby boards join.
void ArenaScreen::drawNetLobby() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Room", C_ACCENT, textdatum_t::top_left, 2);
  net::TourneyNet& tn = net::tourney();
  if (tn.role() == net::TRole::NONE) {
    label(g, SCREEN_W / 2, 46, "Tournament with nearby boards", C_DIM, textdatum_t::top_center);
    button(g, R_ROOM_HOST, "Host a room", C_GO, C_PANEL);
    button(g, R_ROOM_JOIN, "Join a room", C_MOVE, C_PANEL);
  } else {
    char hd[24]; snprintf(hd, sizeof(hd), "%s  %d in", tn.isHost() ? "hosting" : "joined", tn.playerCount());
    label(g, SCREEN_W - 6, 6, hd, C_GO, textdatum_t::top_right);
    label(g, 10, 28, "Players in the room:", C_DIM);
    const std::vector<net::BotCard>& r = tn.roster();
    for (int i = 0; i < (int)r.size() && i < 6; i++) {
      int y = 46 + i * 24;
      g.fillRoundRect(10, y, SCREEN_W - 20, 22, 4, C_PANEL);
      char row[44]; snprintf(row, sizeof(row), "%.8s  (%.12s)", r[i].name.c_str(), r[i].botName.c_str());
      label(g, 16, y + 6, row, C_INK);
    }
  }
  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
  if (tn.isHost()) {
    // host chooses the discipline for the whole room (rides in the SEED packet to every device)
    const char* dn = _type == MatchType::SOCCER ? "Soccer >" : _type == MatchType::SUMO ? "Battle >" : "Race >";
    button(g, R_ROOM_DISC, dn, ui::rgb(120, 230, 245), C_PANEL);
    if (tn.playerCount() >= 2) button(g, R_ROOM_START, "Start >", C_GO, C_PANEL);
    else label(g, SCREEN_W / 2, BOTBAR_Y - 10, "need 2+ boards", C_DIM, textdatum_t::bottom_center);
  } else if (tn.role() == net::TRole::PEER) {
    label(g, SCREEN_W / 2, BOTBAR_Y + 9, "waiting for host to start", C_DIM, textdatum_t::top_center);
  }
}

// Turn the lobby roster into the Cup field and start a networked Cup (shared seed -> every device
// replays the identical bracket). Roster order is uuid-sorted on every device, so fields match.
void ArenaScreen::buildRosterField() {
  net::TourneyNet& tn = net::tourney();
  const std::vector<net::BotCard>& r = tn.roster();
  _cands.clear();
  for (const net::BotCard& c : r) {
    Candidate cd;
    cd.name = std::string(c.botName.length() ? c.botName.c_str() : c.name.c_str());
    cd.prog = programFromJsonString(c.progJson);
    cd.avatar = c.avatar;
    cd.style = std::string(c.name.c_str());   // the owner's name
    cd.house = false; cd.smart = false; cd.neuro = false;  // a concrete program -> don't re-distill
    _cands.push_back(cd);
  }
  _type = (MatchType)tn.discipline();   // the host's chosen discipline (Race / Battle / Soccer)
  std::vector<int> parts;
  for (int i = 0; i < (int)_cands.size(); i++) parts.push_back(i);
  startCup(parts);                  // uses _type, seeds the bracket, _phase=CUP, draws the card
  _netCup = true; _netSeed = tn.seed();
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
  // Race / Battle / Soccer are the shared match types (Battle + Soccer unlock at Sense, L15); then
  // the branch-specific extras.
  button(g, R_G1, "Race - first to the goal", C_GO, C_PANEL);
  button(g, R_G2, battle ? "Battle - last bot standing" : "Battle - needs Sense (Lv15)",
         battle ? C_ACCENT : C_LOCK, C_PANEL);
  button(g, R_G3, battle ? "Soccer - score in the goal" : "Soccer - needs Sense (Lv15)",
         battle ? ui::rgb(120, 230, 245) : C_LOCK, C_PANEL);
  if (_hotseat) {
    button(g, R_G4, "Puzzle Race - beat the clock", C_LOOP, C_PANEL);
    button(g, R_G5, "Seed Challenge - same board", C_SENSE, C_PANEL);
  } else {
    // Tournament: a ladder/cup of your own fighters -- needs at least two saved bots. (Train a
    // fighter now lives on the Arena top menu, since it's prep, not a vs-Computer match.)
    if (neuro && _profile && _profile->library.size() >= 2)
      button(g, R_G4, "Tournament - race / soccer / battle", C_MOVE, C_PANEL);
  }
  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Opponents", C_INK, C_PANEL);
}

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
void ArenaScreen::setupMatchBot(int pick, const Pose& start) {
  Candidate& c = _cands[pick];
  if (_type == MatchType::SOCCER) {
    // A kid's SAVED fighter keeps its own (trained) brain; every house/empty bot gets a distilled
    // SOCCER brain so it actually dribbles the ball toward its goal instead of standing around.
    if (c.prog.brains.empty()) {
      c.prog.clear();
      uint32_t sd = 11u + (uint32_t)c.avatar * 13u;
      uint8_t bi = c.prog.addBrain(sd);
      distillSoccer(c.prog.brains[bi], sd, 8000);   // high-epoch -> the house players actually score
      Node loop = Node::repeatUntil(AT_GOAL); loop.body.push_back(Node::neuro(bi)); c.prog.main.push_back(loop);
    }
    return;
  }
  if (_type == MatchType::SUMO) {
    // Keep each bot's OWN behaviour so matchups stay asymmetric (Rusty charges, Bolt dashes, Vex
    // hunts...). The NEURAL fighters (Neura/Cortex/Volt) get a distilled HUNTER BRAIN -- a real
    // trained net, the neural counterpart to code-Vex -- so battling them shows a learned hunter.
    if (c.neuro && c.prog.brains.empty()) {
      c.prog.clear();
      uint8_t bi = c.prog.addBrain(7u + (uint32_t)c.avatar * 13u);
      distillHunter(c.prog.brains[bi], 7u + (uint32_t)c.avatar * 13u, 2000, true);  // jitter -> tracks a moving player
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

// Build the board for the current discipline: a maze (Race), an open ring (Sumo), or a walled
// soccer pitch (Soccer, which also sets _ball + the two goal mouths).
void ArenaScreen::genMatchBoard(uint32_t seed) {
  if (_type == MatchType::SOCCER)     MazeGen::generateSoccerPitch(_maze, seed, _s0, _s1, _ball, _goal0, _goal1);
  else if (_type == MatchType::SUMO)  MazeGen::generateSumoRing(_maze, seed, _s0, _s1);
  else                                MazeGen::generateArena(_maze, seed, _s0, _s1);
}

void ArenaScreen::startMatch() {
  hal::audio.stopMusic();  // the board uses step-tick SFX; silence the battle theme
  bool sumo = (_type == MatchType::SUMO), soccer = (_type == MatchType::SOCCER);
  // Networked Cup: derive the board from the SHARED seed + match index so every device builds the
  // identical board and replays the same result. Local play mixes the clock for variety.
  uint32_t base = _netCup ? (_netSeed + 101u * (uint32_t)(_cupMatch + 1))
                          : (_profile ? _profile->seedBase : 7u) + (uint32_t)millis() + 101u * (++_sumoNonce);
  if (sumo || soccer) genMatchBoard(base);
  else genMatchBoard(_netCup ? base : (_profile ? _profile->seedBase + 7u : 7u));
  setupMatchBot(_pick0, _s0);
  setupMatchBot(_pick1, _s1);
  const Program& p0 = _cands[_pick0].prog;
  const Program& p1 = _cands[_pick1].prog;
  // Sumo has no goal, so it'd otherwise run the full 300 ticks. The seeker bots now brawl and
  // usually KO each other well before this; the cap is a safety net (then ring-control decides).
  // Cup matches use a shorter cap so a stalemate resolves quickly (Sumo by ring-control, Soccer by
  // ball position) instead of dragging a spectator bracket out -- a single match keeps the full run.
  int cap = sumo ? (_cup ? 90 : 150) : soccer ? (_cup ? 200 : 220) : gb::ARENA_STEP_CAP;
  _arena.setup(&_maze, &p0, &p1, _s0, _s1, _type, cap);
  if (soccer) { _arena.configSoccer(_ball, _goal0, _goal1); _ballPrev = _arena.ball(); }
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
  if (_type == MatchType::SOCCER) {   // tint the open goal mouths (4 tiles tall) -- yours green, foe red
    bool m0 = (c == _goal0.col && inGoalMouth(r, _goal0.row));
    bool m1 = (c == _goal1.col && inGoalMouth(r, _goal1.row));
    if (m0 || m1) {
      g.fillRect(x, y, tile - 1, tile - 1, m0 ? ui::rgb(20, 90, 50) : ui::rgb(110, 30, 30));
      g.drawRect(x, y, tile - 1, tile - 1, C_INK);
    }
  }
}

void ArenaScreen::drawBall() {
  auto& g = hal::display.gfx();
  int tile, ox, oy; mazeGeometry(tile, ox, oy);
  int cx = ox + _arena.ball().col * tile + tile / 2, cy = oy + _arena.ball().row * tile + tile / 2;
  g.fillCircle(cx, cy, tile / 4 + 1, C_INK);
  g.drawCircle(cx, cy, tile / 4 + 1, C_BG);
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
  if (_type == MatchType::SOCCER) {
    // Live scoreline: "P0 name  G0 - G1  P1 name".
    const char* n0 = _hotseat ? "P1" : (_cup ? _cands[_pick0].name.c_str() : "You");
    const char* n1 = _hotseat ? "P2" : _cands[_pick1].name.c_str();
    char nm0[12], nm1[12]; snprintf(nm0, sizeof(nm0), "%.10s", n0); snprintf(nm1, sizeof(nm1), "%.10s", n1);
    label(g, 6, 6, nm0, C_GO);
    char sc[12]; snprintf(sc, sizeof(sc), "%d - %d", _arena.goals(0), _arena.goals(1));
    label(g, SCREEN_W / 2, 6, sc, C_ACCENT, textdatum_t::top_center);
    label(g, SCREEN_W - 6, 6, nm1, C_BAD, textdatum_t::top_right);
    return;
  }
  if (_type != MatchType::SUMO) {
    char hdr[44];
    snprintf(hdr, sizeof(hdr), "Race: %s vs %s", _cands[_pick0].name.c_str(), _cands[_pick1].name.c_str());
    label(g, 6, 4, hdr, C_ACCENT);
    return;
  }
  // In a Cup/tournament match neither bot is "you" -- show the real fighters (truncated to fit the
  // tight name slots); in your own battle the left side is "You", the foe keeps its name.
  char n0[10], n1[10];
  if (_hotseat)   { snprintf(n0, sizeof(n0), "P1"); snprintf(n1, sizeof(n1), "P2"); }
  else if (_cup)  { snprintf(n0, sizeof(n0), "%.8s", _cands[_pick0].name.c_str());
                    snprintf(n1, sizeof(n1), "%.8s", _cands[_pick1].name.c_str()); }
  else            { snprintf(n0, sizeof(n0), "You"); snprintf(n1, sizeof(n1), "%.8s", _cands[_pick1].name.c_str()); }
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
  if (_type == MatchType::SOCCER) { drawBall(); _ballPrev = _arena.ball(); }
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
  label(g, SCREEN_W / 2, y + 20, msg, col, textdatum_t::middle_center, 2);
  if (_type == MatchType::SOCCER) {   // the final scoreline (0-0 = won on position, not a goal)
    char sc[24]; snprintf(sc, sizeof(sc), "%d - %d%s", _arena.goals(0), _arena.goals(1),
                          (_arena.goals(0) == _arena.goals(1)) ? "  (on position)" : "");
    label(g, SCREEN_W / 2, y + 38, sc, C_INK, textdatum_t::middle_center);
  }
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
  if (_type == MatchType::SOCCER && _arena.justScored()) {
    // mirror the live tick: a goal kicked off -> full redraw + scoreline + a "GOAL!" flash
    drawBoard(); _ballPrev = _arena.ball();
    label(hal::display.gfx(), SCREEN_W / 2, SCREEN_H / 2 - 4, "GOAL!", C_GO, textdatum_t::middle_center, 3);
  } else {
    eraseBotAt(b0.row, b0.col); eraseBotAt(b1.row, b1.col);
    if (_type == MatchType::SOCCER) eraseBotAt(_ballPrev.row, _ballPrev.col);
    if (_arena.alive(0)) drawBot(0, _arena.pose(0), _cands[_pick0].avatar);
    if (_arena.alive(1)) drawBot(1, _arena.pose(1), _cands[_pick1].avatar);
    if (_type == MatchType::SOCCER) { drawBall(); _ballPrev = _arena.ball(); }
  }
  if (o != ArenaOutcome::RUNNING) onMatchEnd();
}

app::Signal ArenaScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  bool tap = _tap.tapped(tp, now, tx, ty);

  if (_phase == Phase::BOARD && _running && _last == 0) {
    _last = now;  // arm the timer; the first tick is one interval later (lets capture start at tick 0)
  } else if (_phase == Phase::BOARD && _running &&
             now - _last >= (uint32_t)(_cup ? 90 : _type == MatchType::SOCCER ? 130 : 250)) {  // soccer paces faster (more repositioning)
    _last = now;
    Pose b0 = _arena.pose(0), b1 = _arena.pose(1);
    int hp0 = _arena.hp(0), hp1 = _arena.hp(1);
    bool al0 = _arena.alive(0), al1 = _arena.alive(1);
    ArenaOutcome o = _arena.tick();
    if (_type == MatchType::SOCCER && _arena.justScored()) {
      // a goal kicked off (ball + both bots reset) -> full redraw + scoreline + a "GOAL!" flash
      drawBoard(); _ballPrev = _arena.ball();
      label(hal::display.gfx(), SCREEN_W / 2, SCREEN_H / 2 - 4, "GOAL!", C_GO, textdatum_t::middle_center, 3);
      hal::audio.win();
    } else {
      eraseBotAt(b0.row, b0.col); eraseBotAt(b1.row, b1.col);
      if (_type == MatchType::SOCCER) eraseBotAt(_ballPrev.row, _ballPrev.col);  // wipe the ball's old tile
      if (_arena.alive(0)) drawBot(0, _arena.pose(0), _cands[_pick0].avatar);
      if (_arena.alive(1)) drawBot(1, _arena.pose(1), _cands[_pick1].avatar);
      if (_type == MatchType::SOCCER) { drawBall(); _ballPrev = _arena.ball(); }
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
    }
    if (o != ArenaOutcome::RUNNING) { _running = false; onMatchEnd(); }
  }

  // Networked room: pump ESP-NOW every frame (not just on tap); redraw as the roster grows; once
  // the host seeds the bracket, build the field and run the Cup (same on every device).
  if (_phase == Phase::NETLOBBY) {
    net::tourney().loop(now);
    if (net::tourney().seeded()) { buildRosterField(); return app::Signal::NONE; }
    if ((int)net::tourney().playerCount() != _roomN) { _roomN = (int8_t)net::tourney().playerCount(); drawNetLobby(); }
  }
  // Networked Cup auto-plays (no manual "Next >") so every device advances in lockstep and all
  // reach the same champion at the same pace (the matches themselves are already deterministic).
  if (_netCup && _phase == Phase::CUP && !_cupBracket.done()) {
    if (_cupAutoAt == 0) _cupAutoAt = now + 1600;            // pause on the bracket card to read it
    else if (now >= _cupAutoAt) { _cupAutoAt = 0; cupAdvanceToNextMatch(); }
  }

  if (!tap) return app::Signal::NONE;

  switch (_phase) {
    case Phase::MENU:
      if (R_BACK.contains(tx, ty)) return app::Signal::BACK;
      if (R_OPP_RADIO.contains(tx, ty)) return app::Signal::GOTO_RADIO;  // radio screen has its own battle/trade
      else if (R_OPP_AI.contains(tx, ty))  { _hotseat = false; drawGameType(); }
      else if (R_OPP_HOT.contains(tx, ty)) { _hotseat = true;  drawGameType(); }
      else if (R_OPP_ROOM.contains(tx, ty) && _profile && _profile->unlocks.neuro)
        return app::Signal::GOTO_ARENA_TRAIN;   // the "Train a fighter" slot (Room moved to Radio)
      break;
    case Phase::NETLOBBY: {
      net::TourneyNet& tn = net::tourney();
      if (R_BACK.contains(tx, ty)) { tn.leave(); drawMenu(); break; }
      if (tn.role() == net::TRole::NONE && (R_ROOM_HOST.contains(tx, ty) || R_ROOM_JOIN.contains(tx, ty))) {
        net::BotCard mine;
        mine.name = _profile ? String(_profile->name.c_str()) : String("P");
        mine.avatar = _profile ? _profile->avatar : 0;
        mine.uuid = _profile ? String(_profile->uuid.substr(0, 16).c_str()) : String("anon");
        // Card chunking carries any size, so this device brings the player's REAL saved fighter
        // (their first My-Bots entry). If they have none, field a compact coded hunter.
        bool haveBot = _profile && !_profile->library.empty();
        if (haveBot) {
          mine.botName = String(_profile->library[0].name.c_str());
          mine.progJson = programToJsonString(_profile->library[0].program);
        } else {
          Program compact;
          { Node loop = Node::repeatUntil(AT_GOAL);
            Node z = Node::ifCond(ENEMY_AHEAD); z.body.push_back(Node::command(CMD_FIRE));   loop.body.push_back(z);
            Node r = Node::ifCond(ENEMY_RIGHT); r.body.push_back(Node::command(CMD_TURN_R)); loop.body.push_back(r);
            Node l = Node::ifCond(ENEMY_LEFT);  l.body.push_back(Node::command(CMD_TURN_L)); loop.body.push_back(l);
            loop.body.push_back(Node::command(CMD_FWD)); compact.main.push_back(loop); }
          mine.botName = "Hunter"; mine.progJson = programToJsonString(compact);
        }
        if (R_ROOM_HOST.contains(tx, ty)) tn.host(mine); else tn.join(mine);
        _roomN = -1; drawNetLobby();
      } else if (tn.isHost() && R_ROOM_DISC.contains(tx, ty)) {  // cycle Battle -> Soccer -> Race
        _type = _type == MatchType::SUMO ? MatchType::SOCCER
              : _type == MatchType::SOCCER ? MatchType::RACE : MatchType::SUMO;
        hal::audio.blip(); drawNetLobby();
      } else if (tn.isHost() && tn.playerCount() >= 2 && R_ROOM_START.contains(tx, ty)) {
        uint32_t seed = (_profile ? _profile->seedBase : 7u) + (uint32_t)now;
        tn.start(seed, (uint8_t)_type);   // broadcast the chosen discipline (Race/Battle/Soccer)
      }
      break;
    }
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
      } else if (R_G3.contains(tx, ty)) {    // Soccer (both branches) -- unlocks with Battle (Sense L15)
        if (!(_profile && _profile->unlocks.sense)) { hal::audio.fail(); }
        else { _type = MatchType::SOCCER; buildCandidates(true); _pickScroll = 0; _phase = Phase::PICK1; drawPick(0); }
      } else if (_hotseat && R_G4.contains(tx, ty)) {
        return app::Signal::GOTO_PUZZLE;
      } else if (_hotseat && R_G5.contains(tx, ty)) {
        return app::Signal::GOTO_CHALLENGE;
      } else if (!_hotseat && _profile && _profile->unlocks.neuro && _profile->library.size() >= 2
                 && R_G4.contains(tx, ty)) {
        _phase = Phase::TDISC; drawTDisc();            // tournament: pick discipline first
      }
      break;
    case Phase::TDISC:
      if (R_BACK.contains(tx, ty)) { drawGameType(); }
      else if (R_G1.contains(tx, ty) || R_G2.contains(tx, ty) || R_G3.contains(tx, ty)) {   // Race / Soccer / Battle
        _type = R_G3.contains(tx, ty) ? MatchType::SUMO
              : R_G2.contains(tx, ty) ? MatchType::SOCCER : MatchType::RACE;
        buildCandidates(_type != MatchType::RACE);     // race shows dashers; fights hide them
        _tSelMask = (_cands.size() >= 32) ? 0xFFFFFFFFu : ((1u << _cands.size()) - 1u);  // all selected
        _tScroll = 0; _phase = Phase::TPICK; drawTPick();
      }
      break;
    case Phase::TPICK: {
      if (R_BACK.contains(tx, ty)) { _phase = Phase::TDISC; drawTDisc(); break; }
      uint32_t allMask = (_cands.size() >= 32) ? 0xFFFFFFFFu : ((1u << _cands.size()) - 1u);
      if (R_TALL.contains(tx, ty)) {                   // Select all <-> none
        _tSelMask = (_tSelMask == allMask) ? 0u : allMask;
        hal::audio.blip(); drawTPick(); break;
      }
      if (R_TNEXT.contains(tx, ty)) {                  // Format > (need >=2 fighters)
        int sel = 0; for (int i = 0; i < (int)_cands.size(); i++) if ((_tSelMask >> i) & 1u) sel++;
        if (sel >= 2) { _phase = Phase::TCHOICE; drawTChoice(); } else hal::audio.fail();
        break;
      }
      int n = (int)_cands.size(), vis = pickVisible();
      if (n > vis && tx >= SCREEN_W - 14) {            // scrollbar paging
        _tScroll += (ty < PICK_TOP + vis * PICK_ROWH / 2) ? -(vis - 1) : (vis - 1);
        if (_tScroll > n - vis) _tScroll = n - vis;
        if (_tScroll < 0) _tScroll = 0;
        hal::audio.blip(); drawTPick(); break;
      }
      for (int i = _tScroll; i < n && i < _tScroll + vis; i++) {   // toggle a fighter in/out
        Rect r = {6, (int16_t)(PICK_TOP + (i - _tScroll) * PICK_ROWH), 300, 24};
        if (r.contains(tx, ty)) { _tSelMask ^= (1u << i); hal::audio.blip(); drawTPick(); break; }
      }
      break;
    }
    case Phase::TCHOICE: {
      if (R_BACK.contains(tx, ty)) { _phase = Phase::TPICK; drawTPick(); break; }
      std::vector<int> parts;
      for (int i = 0; i < (int)_cands.size(); i++) if ((_tSelMask >> i) & 1u) parts.push_back(i);
      if (R_G1.contains(tx, ty)) {                     // Ladder: instant round-robin leaderboard
        auto& g = hal::display.gfx();
        label(g, SCREEN_W / 2, SCREEN_H / 2, "Running tournament...", C_ACCENT, textdatum_t::middle_center);
        runTournament(parts);
      } else if (R_G3.contains(tx, ty)) {              // Cup: watchable single-elimination bracket
        startCup(parts);
      }
      break;
    }
    case Phase::TOURNEY: {
      if (_breakdownMi >= 0) {                          // a fighter's per-opponent record
        int n = _tN > 0 ? _tN - 1 : 0, npages = (n + TPER_PAGE - 1) / TPER_PAGE; if (npages < 1) npages = 1;
        if (R_BACK.contains(tx, ty)) { _breakdownMi = -1; _bdPage = 0; drawTourney(); }
        else if (R_SPREV.contains(tx, ty)) { if (_bdPage > 0) { _bdPage--; hal::audio.blip(); drawBreakdown(); } }
        else if (R_SNEXT.contains(tx, ty)) { if (_bdPage < npages - 1) { _bdPage++; hal::audio.blip(); drawBreakdown(); } }
        break;
      }
      int n = (int)_standings.size(), npages = (n + TPER_PAGE - 1) / TPER_PAGE; if (npages < 1) npages = 1;
      if (R_BACK.contains(tx, ty)) { _phase = Phase::TPICK; drawTPick(); break; }
      if (R_SPREV.contains(tx, ty)) { if (_standPage > 0) { _standPage--; hal::audio.blip(); drawTourney(); } break; }
      if (R_SNEXT.contains(tx, ty)) { if (_standPage < npages - 1) { _standPage++; hal::audio.blip(); drawTourney(); } break; }
      int rowh = 30, top = BAND_Y + 6;                  // tap a standing -> its breakdown
      for (int r = 0; r < TPER_PAGE; r++) {
        int i = _standPage * TPER_PAGE + r; if (i >= n) break;
        Rect rr = {8, (int16_t)(top + r * rowh), (int16_t)(SCREEN_W - 16), (int16_t)(rowh - 4)};
        if (rr.contains(tx, ty)) { _breakdownMi = _standings[i].mi; _bdPage = 0; hal::audio.blip(); drawBreakdown(); break; }
      }
      break;
    }
    case Phase::CUP:
      if (_cupBracket.done()) {                        // champion shown (bottom-bar buttons)
        if (R_CUP_AGAIN.contains(tx, ty)) { std::vector<int> same = _cupPlayers; startCup(same); }  // re-run, same field
        else if (R_CUP_EXIT.contains(tx, ty) || R_BACK.contains(tx, ty)) drawGameType();
      } else {
        if (R_CUP_AGAIN.contains(tx, ty)) cupAdvanceToNextMatch();   // Start / Next match
        else if (R_CUP_EXIT.contains(tx, ty)) { _cup = false; drawGameType(); }  // Quit
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
