#include "screens/ArenaScreen.h"
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
  buildCandidates();
}

void ArenaScreen::buildCandidates() {
  _cands.clear();
  // The kid's OWN bots first — any program saved to the library in the campaign
  // (Save>Lib) fights here. THIS is how you battle a custom program (SPEC §18.4).
  if (_profile) {
    for (auto& e : _profile->library) {
      if (_cands.size() >= 3) break;
      _cands.push_back({e.name, e.program, _profile->avatar, "your bot", false, false});
    }
  }
  // House battle-bots: themed names + characters to go up against.
  _cands.push_back({"Rusty", alwaysForwardProgram(), 5, "charges blindly", true, false});
  _cands.push_back({"Bolt",  dashProgram(),          6, "fast & straight", true, false});
  _cands.push_back({"Vex",   hunterProgram(),        7, "hunts & zaps",    true, false});
  // Ace solves the board on the fly — a real navigator (program built at match start).
  _cands.push_back({"Ace",   Program{},              3, "solves the maze", true, true});
  // Pre-trained NeuroBots: their brain (a neural net) is trained on the board at match
  // start, then drives them — battle an actual trained brain.
  _cands.push_back({"Neura",  Program{}, 2, "a trained brain",  true, false, true});
  _cands.push_back({"Cortex", Program{}, 4, "an evolved brain", true, false, true});
}

int ArenaScreen::houseBotIndex(const char* name) const {
  for (int i = 0; i < (int)_cands.size(); i++)
    if (_cands[i].house && _cands[i].name == name) return i;
  return -1;
}

void ArenaScreen::enter() { drawMenu(); }

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
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, _hotseat ? "Hotseat" : "vs Computer", C_ACCENT, textdatum_t::top_left, 2);
  label(g, SCREEN_W / 2, 38, "Pick a game", C_INK, textdatum_t::top_center);
  if (_hotseat) {
    button(g, R_G1, "Race - first to the goal", C_GO, C_PANEL);
    button(g, R_G2, "Sumo - shove them off", C_ACCENT, C_PANEL);
    button(g, R_G3, "Puzzle Race - beat the clock", C_LOOP, C_PANEL);
    button(g, R_G4, "Seed Challenge - same board", C_SENSE, C_PANEL);
  } else {
    button(g, R_G1, "Race - first to the goal", C_GO, C_PANEL);
    button(g, R_G2, "Sumo - shove them off", C_ACCENT, C_PANEL);
    if (_profile && _profile->unlocks.neuro)
      button(g, R_G3, "Train a fighter (NeuroBot)", ui::rgb(120, 230, 245), C_PANEL);
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
  // vs-Computer picks only YOUR bot (the computer brings its own); Hotseat picks P1 then P2.
  char t[28];
  if (_hotseat) snprintf(t, sizeof(t), "Player %d: pick a bot", player + 1);
  else          snprintf(t, sizeof(t), "Pick YOUR bot");
  label(g, 6, 3, t, C_ACCENT, textdatum_t::top_left, 2);
  if (!_hotseat) label(g, SCREEN_W - 6, 6, "vs Computer", C_DIM, textdatum_t::top_right);
  int n = (int)_cands.size(), vis = pickVisible();
  if (_pickScroll > n - vis) _pickScroll = (n > vis) ? n - vis : 0;
  if (_pickScroll < 0) _pickScroll = 0;
  for (int i = _pickScroll; i < n && i < _pickScroll + vis; i++) {
    Rect r = pickRowRect(i);
    const Candidate& c = _cands[i];
    g.fillRoundRect(r.x, r.y, r.w, r.h, 4, C_PANEL_HI);  // all rows equally pickable (none greyed)
    g.drawRoundRect(r.x, r.y, r.w, r.h, 4, c.house ? C_PANEL_HI : C_GO);  // your bots outlined green
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
    // Keep each bot's OWN behaviour so matchups stay asymmetric (Rusty charges, Bolt
    // dashes, Vex hunts, the kid's trained brain hunts) -- identical programs from the
    // mirror-image starts would just tie. Only the "built at match start" bots (Ace,
    // Neura, Cortex) have an empty program here; give them a hunter so they engage.
    if (c.prog.main.empty() && c.prog.brains.empty()) c.prog = hunterProgram();
    return;
  }
  if (c.smart) solveMazeFrom(_maze, start, true, c.prog);
  else if (c.neuro) buildNeuroBot(c.prog, _maze, start, 7u + (uint32_t)c.avatar * 13u);
  else adaptNeuroBot(c.prog, _maze, start);
}

void ArenaScreen::startMatch() {
  hal::audio.stopMusic();  // the board uses step-tick SFX; silence the battle theme
  MazeGen::generateArena(_maze, _profile ? _profile->seedBase + 7u : 7u, _s0, _s1);
  bool sumo = (_type == MatchType::SUMO);
  if (sumo) _maze.clearGoal();   // Sumo = last bot standing: no goal, so bots fight not race
  setupMatchBot(_pick0, _s0, sumo);
  setupMatchBot(_pick1, _s1, sumo);
  const Program& p0 = _cands[_pick0].prog;
  const Program& p1 = _cands[_pick1].prog;
  _arena.setup(&_maze, &p0, &p1, _s0, _s1, _type);
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

void ArenaScreen::drawBot(int i, const Pose& p, int avatar) {
  auto& g = hal::display.gfx();
  int tile, ox, oy; mazeGeometry(tile, ox, oy);
  int cx = ox + p.col * tile + tile / 2, cy = oy + p.row * tile + tile / 2;
  assets::drawCharacter(g, cx, cy, tile, avatar, p.facing);
}

void ArenaScreen::drawBoard() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  char hdr[40];
  snprintf(hdr, sizeof(hdr), "%s: %s vs %s", _type == MatchType::RACE ? "Race" : "Sumo",
           _cands[_pick0].name.c_str(), _cands[_pick1].name.c_str());
  label(g, 6, 4, hdr, C_ACCENT);
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
  drawCell(b0.row, b0.col); drawCell(b1.row, b1.col);
  if (_arena.alive(0)) drawBot(0, _arena.pose(0), _cands[_pick0].avatar);
  if (_arena.alive(1)) drawBot(1, _arena.pose(1), _cands[_pick1].avatar);
  if (o != ArenaOutcome::RUNNING) { _phase = Phase::DONE; finishOverlay(); }
}

app::Signal ArenaScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  bool tap = _tap.tapped(tp, now, tx, ty);

  if (_phase == Phase::BOARD && _running && _last == 0) {
    _last = now;  // arm the timer; the first tick is one interval later (lets capture start at tick 0)
  } else if (_phase == Phase::BOARD && _running && now - _last >= 250) {
    _last = now;
    Pose b0 = _arena.pose(0), b1 = _arena.pose(1);
    ArenaOutcome o = _arena.tick();
    drawCell(b0.row, b0.col); drawCell(b1.row, b1.col);
    if (_arena.alive(0)) drawBot(0, _arena.pose(0), _cands[_pick0].avatar);
    if (_arena.alive(1)) drawBot(1, _arena.pose(1), _cands[_pick1].avatar);
    hal::audio.tick();
    if (o != ArenaOutcome::RUNNING) { _running = false; _phase = Phase::DONE; finishOverlay(); }
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
        _type = MatchType::RACE; _pickScroll = 0; _phase = Phase::PICK1; drawPick(0);
      } else if (R_G2.contains(tx, ty)) {  // Sumo (both branches)
        _type = MatchType::SUMO; _pickScroll = 0; _phase = Phase::PICK1; drawPick(0);
      } else if (_hotseat && R_G3.contains(tx, ty)) {
        return app::Signal::GOTO_PUZZLE;
      } else if (_hotseat && R_G4.contains(tx, ty)) {
        return app::Signal::GOTO_CHALLENGE;
      } else if (!_hotseat && _profile && _profile->unlocks.neuro && R_G3.contains(tx, ty)) {
        return app::Signal::GOTO_ARENA_TRAIN;
      }
      break;
    case Phase::PICK1:
      if (pickScrollTap(tx, ty)) { drawPick(0); break; }
      for (int i = 0; i < (int)_cands.size(); i++) {
        if (pickRowRect(i).contains(tx, ty)) {
          _pick0 = i; hal::audio.blip();
          if (_hotseat) { drawHandoff(); }
          else {
            // vs the Computer, give a BEATABLE opponent (a blind dasher) so a kid's bot —
            // or a freshly-trained Brainy — can actually win; the clever bots (Ace, Vex)
            // are there to PICK and play as, and Hotseat-vs-a-friend is the real challenge.
            int opp = houseBotIndex("Bolt");
            if (opp == _pick0) opp = houseBotIndex("Rusty");
            _pick1 = (opp >= 0) ? opp : 0;
            startMatch();
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
