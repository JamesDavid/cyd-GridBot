#include "screens/GameScreen.h"

#include <math.h>
#include "hal/Display.h"
#include "hal/Audio.h"
#include "hal/Led.h"
#include "ui/UI.h"
#include "ui/Theme.h"
#include "assets/Assets.h"
#include "game/MazeGen.h"
#include "game/Score.h"

using namespace ui;
using namespace gb;

namespace screens {

static bool spriteHasPixels(const std::vector<uint8_t>& v);

// ---- layout rects (320x240, SPEC §10) -------------------------------------
static constexpr int PAD_X0 = 6, PAD_Y0 = BAND_Y + 2, PAD_CELL = 50, PAD_PITCH = 52;
static Rect padCell(int i, int j) { return {(int16_t)(PAD_X0 + j * PAD_PITCH),
                                            (int16_t)(PAD_Y0 + i * PAD_PITCH),
                                            PAD_CELL, PAD_CELL}; }
static const Rect R_CLEAR  = {6, 179, 48, 30};
static const Rect R_DELPAD = {58, 179, 46, 30};    // delete selected line
static const Rect R_RUNPAD = {108, 179, 54, 30};   // RUN sits with CLEAR/DEL
static constexpr int LIST_X = 166, LIST_W = 320 - 166;
static constexpr int ROW_Y0 = BAND_Y + 22, ROW_H = 24;
// Block controls live INLINE on the selected row (kept left of the scrollbar zone and
// well away from the top-right HOME button, which they used to sit under).
// One BIG button per block control (the old +/- were too small to tap): the repeat
// button cycles the count 2->3->4->5->2; the cond button cycles the sense condition.
static inline Rect repCycleRect(int y) { return {(int16_t)(LIST_X + LIST_W - 92), (int16_t)(y + 1), 80, (int16_t)(ROW_H - 2)}; }
static inline Rect condRect(int y)     { return {(int16_t)(LIST_X + LIST_W - 92), (int16_t)(y + 1), 80, (int16_t)(ROW_H - 2)}; }
static const Rect R_PAUSE = {238, 0, 82, 22};   // big back button in the status bar
static const Rect R_TOGGLE = {6, (int16_t)(BOTBAR_Y + 2), 156, 26};  // right edge aligns with the RUN/pad above
static const Rect R_RUNBAR = {164, (int16_t)(BOTBAR_Y + 2), 150, 26};
static const Rect R_STEP = {6, (int16_t)(BOTBAR_Y + 2), 150, 26};
static const Rect R_RESET = {164, (int16_t)(BOTBAR_Y + 2), 150, 26};

// ---------------------------------------------------------------------------
void GameScreen::begin(Profile* profile, uint32_t level) {
  _profile = profile;
  _level = level;
  _challenge = false;
  _boardCount = MazeGen::generateBoards(_boards, gb::MAX_BOARDS,
                                        profile ? profile->seedBase : 0, level);
  _boardIdx = 0;
  _maze = _boards[0];
  recountGems();
  _biome = ui::biomeFor((int)level);
  _prog.clear();
  // Resume the in-progress script for this level if present (SPEC §11.1).
  if (profile && profile->workLevel == level && !profile->work.empty()) {
    _prog = profile->work;
  }
  _editList = &_prog.main;
  _par = shortestSolutionLen(_maze, profile && profile->unlocks.jump);
  if (_par <= 0) _par = 1;
  _stepMs = profile ? animStepMs(profile->settings) : 400;
  // (audio on/off is the session-wide menu toggle, not overridden per-level)
  _view = V_CODE;
  _mode = M_EDIT;
  _auto = false;
  _selected = -1;
  _scroll = 0;
  _followTail = true;
  _failNode = nullptr;
  _drawnPose = _maze.startPose();
  _tween = false;
  for (auto& v : _visited) v = false;
  for (auto& v : _coinTaken) v = false;
  for (auto& v : _gemTaken) v = false;
  _gemsThisRun = 0;
  _coinsThisRun = 0;
  _it.load(&_prog, &_maze, _maze.startPose());
}

void GameScreen::beginChallenge(Profile* profile, uint32_t seedCode) {
  static constexpr int CHALLENGE_LEVEL = 12;  // fixed difficulty so a code => one board
  _profile = profile;
  _challenge = true;
  _challengeCode = seedCode;
  _level = CHALLENGE_LEVEL;
  _boardCount = 1;
  MazeGen::generate(_boards[0], seedCode, CHALLENGE_LEVEL);  // keyed purely by the code
  _boardIdx = 0;
  _maze = _boards[0];
  recountGems();
  _biome = ui::biomeFor(CHALLENGE_LEVEL);
  _prog.clear();                       // never resume a campaign script into a challenge
  _editList = &_prog.main;
  _par = shortestSolutionLen(_maze, profile && profile->unlocks.jump);
  if (_par <= 0) _par = 1;
  _stepMs = profile ? animStepMs(profile->settings) : 400;
  _view = V_CODE;
  _mode = M_EDIT;
  _auto = false;
  _selected = -1;
  _scroll = 0;
  _followTail = true;
  _failNode = nullptr;
  _drawnPose = _maze.startPose();
  _tween = false;
  for (auto& v : _visited) v = false;
  for (auto& v : _coinTaken) v = false;
  for (auto& v : _gemTaken) v = false;
  _gemsThisRun = 0;
  _coinsThisRun = 0;
  _it.load(&_prog, &_maze, _maze.startPose());
}

void GameScreen::enter() {
  // Show the maze for a couple of seconds so the kid can study the layout, then
  // auto-switch to the code view (a tap skips the preview).
  _previewing = true;
  _previewStart = 0;
  _view = V_MAZE;
  drawMazeView(false);
  auto& g = hal::display.gfx();
  label(g, SCREEN_W / 2, BAND_Y + 6, "Study the maze...", C_ACCENT,
        textdatum_t::top_center);
}

// Return straight to the editor (no "Study the maze" preview) — used when coming back
// from the brain trainer, where the kid was already editing and wants to RUN their brain.
void GameScreen::resumeCode() {
  _previewing = false;
  _mode = M_EDIT;
  _view = V_CODE;
  drawCodeView();
}

bool GameScreen::cornerUnlocked(int slot) const {
  if (!_profile) return false;
  switch (slot) {
    case 0: return _profile->unlocks.jump;
    case 1: return _profile->unlocks.repeat;
    case 2: return _profile->unlocks.func;
    case 3: return _profile->unlocks.sense;
  }
  return false;
}

// ---- chrome ---------------------------------------------------------------
void GameScreen::drawChrome() {
  auto& g = hal::display.gfx();
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  char buf[40];
  const char* nm = _profile ? _profile->name.c_str() : "Player";
  if (_challenge)
    snprintf(buf, sizeof(buf), "%s   Seed %04u   par %d", nm,
             (unsigned)(_challengeCode % 10000), _par);
  else if (_boardCount > 1)
    snprintf(buf, sizeof(buf), "%s  Lv %u  maze %d/%d", nm, (unsigned)_level,
             _boardIdx + 1, _boardCount);
  else
    snprintf(buf, sizeof(buf), "%s   Lv %u   par %d", nm, (unsigned)_level, _par);
  label(g, 6, 4, buf, C_INK);
  uint32_t stars = _profile ? _profile->stats.starsTotal : 0;
  snprintf(buf, sizeof(buf), "*%u", (unsigned)stars);
  label(g, 232, 4, buf, C_ACCENT, textdatum_t::top_right);
  // big back button (SPEC §10): returns to the profile menu
  button(g, R_PAUSE, "< HOME", C_INK, C_PANEL_HI);
}

// Live step counter, drawn over the stars during a run so you can watch the count
// climb — a brain that never reaches the goal visibly marches toward the step cap.
void GameScreen::drawStepBadge() {
  auto& g = hal::display.gfx();
  g.fillRect(176, 2, 58, 18, C_PANEL);
  char sb[16]; snprintf(sb, sizeof(sb), "step %d", _it.primCount());
  label(g, 232, 4, sb, C_GO, textdatum_t::top_right);
}

// ---- maze view ------------------------------------------------------------
void GameScreen::mazeGeometry(int& tile, int& ox, int& oy) {
  int cols = _maze.cols(), rows = _maze.rows();
  tile = SCREEN_W / cols;
  int th = BAND_H / rows;
  if (th < tile) tile = th;
  int gw = tile * cols, gh = tile * rows;
  ox = (SCREEN_W - gw) / 2;
  oy = BAND_Y + (BAND_H - gh) / 2;
}

void GameScreen::drawCell(int r, int c) {
  auto& g = hal::display.gfx();
  int tile, ox, oy;
  mazeGeometry(tile, ox, oy);
  int x = ox + c * tile, y = oy + r * tile;
  Tile t = _maze.at(r, c);
  if (t == PIT) {
    // a pit reads as a hole: fill with the background (looks "missing"), thin rim
    g.fillRect(x, y, tile - 1, tile - 1, C_BG);
    g.drawRect(x + 1, y + 1, tile - 3, tile - 3, C_FLOOR2);
  } else if (t == WALL) {
    g.fillRect(x, y, tile - 1, tile - 1, _biome.wall);
    // brick courses, tinted per biome
    g.drawFastHLine(x, y + tile / 3, tile - 1, _biome.wallLine);
    g.drawFastHLine(x, y + 2 * tile / 3, tile - 1, _biome.wallLine);
    g.drawFastVLine(x + tile / 2, y, tile / 3, _biome.wallLine);
    g.drawFastVLine(x + tile / 4, y + tile / 3, tile / 3, _biome.wallLine);
    g.drawFastVLine(x + 3 * tile / 4, y + tile / 3, tile / 3, _biome.wallLine);
  } else {
    g.fillRect(x, y, tile - 1, tile - 1, ((r + c) & 1) ? _biome.floorA : _biome.floorB);
    // breadcrumb: outline tiles the robot has already stepped on (per-biome colour)
    if (_visited[r * _maze.cols() + c])
      g.drawRoundRect(x + 2, y + 2, tile - 5, tile - 5, 3, _biome.crumb);
    // a coin pickup (bonus currency)
    if (t == COIN && !_coinTaken[r * _maze.cols() + c]) {
      int cx = x + tile / 2, cy = y + tile / 2, rad = tile / 5 + 1;
      g.fillCircle(cx, cy, rad, C_ACCENT);
      g.drawCircle(cx, cy, rad, ui::rgb(180, 130, 0));
      g.fillCircle(cx - rad / 3, cy - rad / 3, 1, C_INK);  // shine
    }
    // a STAR gem (rarer bonus): a bright cyan 4-point sparkle, distinct from the coin
    if (t == STAR && !_gemTaken[r * _maze.cols() + c]) {
      int cx = x + tile / 2, cy = y + tile / 2, rad = tile / 4;
      uint16_t gem = ui::rgb(120, 230, 245);
      g.fillTriangle(cx, cy - rad, cx - rad / 3, cy, cx + rad / 3, cy, gem);  // N
      g.fillTriangle(cx, cy + rad, cx - rad / 3, cy, cx + rad / 3, cy, gem);  // S
      g.fillTriangle(cx - rad, cy, cx, cy - rad / 3, cx, cy + rad / 3, gem);  // W
      g.fillTriangle(cx + rad, cy, cx, cy - rad / 3, cx, cy + rad / 3, gem);  // E
      g.fillCircle(cx, cy, rad / 3, C_INK);  // bright core
    }
  }
  if (_maze.isGoal(r, c)) {
    if (_profile && spriteHasPixels(_profile->customGoal))
      assets::drawCustomSprite(g, x + tile / 2, y + tile / 2, tile, _profile->customGoal.data());
    else
      assets::drawGoalToken(g, x + tile / 2, y + tile / 2, tile, _profile ? _profile->avatar : 0);
  }
}

static bool spriteHasPixels(const std::vector<uint8_t>& v) {
  if (v.size() != (size_t)gb::PIX_CELLS) return false;
  for (uint8_t b : v) if (b) return true;
  return false;
}

void GameScreen::drawCharacterPx(int cx, int cy, Facing facing, int emote) {
  auto& g = hal::display.gfx();
  int tile, ox, oy;
  mazeGeometry(tile, ox, oy);
  if (_profile && spriteHasPixels(_profile->customChar)) {
    assets::drawCustomSprite(g, cx, cy, tile, _profile->customChar.data());
    int dr, dc; facingDelta(facing, dr, dc);
    g.fillCircle(cx + dc * (tile / 2 - 2), cy + dr * (tile / 2 - 2), 2, C_ACCENT);
  } else {
    uint16_t tint = (_profile && _profile->shopColor > 0 && _profile->shopColor <= assets::SHOP_COLOR_N)
                        ? assets::SHOP_COLORS[_profile->shopColor - 1].color : 0;
    if (tint) assets::drawCharacterTinted(g, cx, cy, tile, tint, facing);
    else assets::drawCharacter(g, cx, cy, tile, _profile ? _profile->avatar : 0, facing);
  }
  // equipped emoji accessory (shop)
  if (_profile && _profile->shopEmoji > 0)
    assets::drawEmoji(g, _profile->shopEmoji, cx + tile / 3, cy - tile / 2, tile / 4);
  // emotes: happy = sparkles above; dizzy = red X eyes
  int eo = tile * 0.11f;
  if (emote == 1) {
    g.drawFastVLine(cx - tile / 3, cy - tile / 2, 4, C_ACCENT);
    g.drawFastHLine(cx - tile / 3 - 2, cy - tile / 2 + 2, 4, C_ACCENT);
    g.drawFastVLine(cx + tile / 3, cy - tile / 2 - 2, 4, C_ACCENT);
    g.drawFastHLine(cx + tile / 3 - 2, cy - tile / 2, 4, C_ACCENT);
  } else if (emote == 2) {
    for (int s = -1; s <= 1; s += 2) {
      int ex = cx + s * eo, ey = cy - tile / 12;
      g.drawLine(ex - 2, ey - 2, ex + 2, ey + 2, C_BAD);
      g.drawLine(ex - 2, ey + 2, ex + 2, ey - 2, C_BAD);
    }
  }
}

void GameScreen::drawCharacterAt(const Pose& p) {
  int tile, ox, oy;
  mazeGeometry(tile, ox, oy);
  drawCharacterPx(ox + p.col * tile + tile / 2, oy + p.row * tile + tile / 2, p.facing, 0);
}

// Redraw the static cells the tweening sprite passes over (so it leaves no trail).
// The robot's antenna pokes ABOVE its tile and the facing-arrow nose pokes into the
// NEXT tile, so we clear a one-cell margin around the from/to bounding box — otherwise
// those yellow bits smear a trail along the path (antenna above, nose ahead).
void GameScreen::redrawCellsBetween(const Pose& a, const Pose& b) {
  int r0 = (a.row < b.row ? a.row : b.row) - 1;
  int r1 = (a.row > b.row ? a.row : b.row) + 1;
  int c0 = (a.col < b.col ? a.col : b.col) - 1;
  int c1 = (a.col > b.col ? a.col : b.col) + 1;
  for (int r = r0; r <= r1; r++)
    for (int c = c0; c <= c1; c++)
      if (_maze.inBounds(r, c)) drawCell(r, c);
}

void GameScreen::winCelebration() {
  auto& g = hal::display.gfx();
  int tile, ox, oy; mazeGeometry(tile, ox, oy);
  int cx = ox + _it.pose().col * tile + tile / 2;
  int cy = oy + _it.pose().row * tile + tile / 2;
  const uint16_t cols[4] = {C_ACCENT, C_GO, C_MOVE, C_FUNC};
  for (int f = 0; f < 5; f++) {
    int rad = 6 + f * 7;
    // alternate the spoke angle each frame so the burst shimmers rather than pulses
    for (int a = (f & 1) ? 22 : 0; a < 360; a += 45) {
      float rr = a * 3.14159f / 180.f;
      int sx = cx + (int)(rad * cosf(rr)), sy = cy + (int)(rad * sinf(rr));
      g.fillCircle(sx, sy, 2, cols[(f + a / 45) % 4]);
    }
    drawCharacterPx(cx, cy - (f % 2 ? 2 : 0), _it.pose().facing, 1);  // happy bounce
    if (f >= 2) assets::drawEmoji(g, 2, cx, cy - 14 - f, 8);          // a star rises up
    delay(45);
    if (f < 4) { drawCell(_it.pose().row, _it.pose().col); }
  }
  // clear the rising-star trail above the robot, then settle the character
  drawCell(_it.pose().row, _it.pose().col);
  if (_it.pose().row > 0) drawCell(_it.pose().row - 1, _it.pose().col);
  drawCharacterAt(_it.pose());
}

void GameScreen::drawMazeView(bool peek) {
  auto& g = hal::display.gfx();
  g.fillRect(0, BAND_Y, SCREEN_W, BAND_H, C_BG);
  drawChrome();
  for (int r = 0; r < _maze.rows(); r++)
    for (int c = 0; c < _maze.cols(); c++) drawCell(r, c);
  Pose p = (_mode == M_RUN) ? _it.pose() : _maze.startPose();
  drawCharacterAt(p);
  _drawnPose = p;
  drawBottomBar();
  if (peek) label(g, SCREEN_W / 2, BAND_Y + 8, "(peek)", C_DIM,
                  textdatum_t::top_center);
}

// ---- code view ------------------------------------------------------------
void GameScreen::drawCodeView() {
  auto& g = hal::display.gfx();
  g.fillRect(0, BAND_Y, SCREEN_W, BAND_H, C_BG);
  drawChrome();
  drawControlPad();
  drawProgramList();
  drawBottomBar();
}

void GameScreen::drawControlPad() {
  auto& g = hal::display.gfx();
  auto cell = [&](int i, int j, Glyph gl, uint16_t col, bool locked) {
    Rect r = padCell(i, j);
    panel(g, r, locked ? C_LOCK : C_PANEL_HI);
    drawGlyph(g, locked ? Glyph::LOCK : gl, r.cx(), r.cy(), 22,
              locked ? C_DIM : col);
  };
  cell(0, 1, Glyph::ARROW_UP, C_MOVE, false);          // FORWARD
  cell(1, 0, Glyph::TURN_L, C_TURN, false);            // TURN LEFT
  cell(1, 2, Glyph::TURN_R, C_TURN, false);            // TURN RIGHT
  // The old BACKWARD slot (bottom-centre) is now the NeuroBot brain slot once unlocked;
  // before that it's a padlock like the other growth slots (consistent + previews it).
  if (_profile && _profile->unlocks.neuro)
    button(g, padCell(2, 1), "+brain", ui::rgb(120, 230, 245), C_PANEL);
  else
    cell(2, 1, Glyph::SENSE, C_DIM, true);  // locked padlock until NeuroBot unlocks (Lv 28)
  // centre avatar — faces the maze's START orientation so the kid can reason about
  // "forward" before running. Once the Arena opens it doubles as the ZAP button (the
  // robot zaps!) — a big central target, with a lightning badge to show it's tappable.
  Rect ctr = padCell(1, 1);
  bool zapOn = _profile && _profile->unlocks.sense;
  panel(g, ctr, zapOn ? C_PANEL_HI : C_PANEL);
  if (_profile && spriteHasPixels(_profile->customChar))
    assets::drawCustomSprite(g, ctr.cx(), ctr.cy(), 40, _profile->customChar.data());
  else
    assets::drawCharacter(g, ctr.cx(), ctr.cy(), 40, _profile ? _profile->avatar : 0,
                          _maze.startPose().facing);
  if (zapOn) drawGlyph(g, Glyph::ZAP, ctr.x + ctr.w - 9, ctr.y + 9, 12, ui::rgb(255, 200, 40));
  // four corner growth slots
  const Glyph cg[4] = {Glyph::JUMP, Glyph::REPEAT, Glyph::CALL, Glyph::SENSE};
  const uint16_t cc[4] = {C_GO, C_LOOP, C_FUNC, C_SENSE};
  const int ci[4] = {0, 0, 2, 2}, cj[4] = {0, 2, 0, 2};
  for (int s = 0; s < 4; s++)
    cell(ci[s], cj[s], cg[s], cc[s], !cornerUnlocked(s));
  // CLEAR / DEL / RUN under the pad
  button(g, R_CLEAR, "CLR", C_BAD, C_PANEL);
  bool canDel = false;
  if (_selected >= 0) {
    std::vector<Row> rows; flatten(*_editList, 0, rows);
    canDel = (_selected < (int)rows.size() && !rows[_selected].addSlot);
  }
  button(g, R_DELPAD, "DEL", canDel ? C_BAD : C_DIM, C_PANEL);
  button(g, R_RUNPAD, "RUN", C_GO, C_PANEL);
}

static uint16_t bracketColorFor(const Node& n) {
  switch (n.type) {
    case N_REPEAT: return C_LOOP;
    case N_REPEAT_UNTIL:
    case N_IF: return C_SENSE;
    case N_CALL: return C_FUNC;
    default: return C_DIM;
  }
}

void GameScreen::flatten(NodeList& list, int depth, std::vector<Row>& out, uint16_t bracket) {
  for (size_t i = 0; i < list.size(); i++) {
    Node& nd = list[i];
    out.push_back({&nd, &list, (int)i, depth, bracket, false});
    bool block = (nd.type == N_REPEAT || nd.type == N_IF || nd.type == N_REPEAT_UNTIL);
    if (block) {
      uint16_t br = bracketColorFor(nd);
      if (!nd.body.empty()) flatten(nd.body, depth + 1, out, br);
      // a slot to add INSIDE the block (the parent block's own "+ add inside" + the
      // global "+ add here" cover everything outside, so no separate "+ add outside").
      out.push_back({&nd, &nd.body, -1, depth + 1, br, true});
    } else if (nd.type == N_NEURO) {
      // a "train this brain" line directly under the brain node, at the SAME indent
      // (part of the brain block, not nested deeper)
      out.push_back({&nd, &list, -1, depth, bracket, false, true});
    }
  }
  // a general "+ add here" at the end of the top level (node = null marks it)
  if (depth == 0) out.push_back({nullptr, &list, -1, 0, 0, true});
}

int GameScreen::listTop() const {
  bool tabs = _profile && _profile->unlocks.func;
  return ROW_Y0 + (tabs ? 26 : 0);
}

ui::Rect GameScreen::funcTabRect(int i) const {
  // 5 cells: MAIN | F1 | F2 | Save>Lib | Load<Lib (SPEC §10, §11.1). Taller and
  // wider so F1/F2 are easy to tap on the resistive panel.
  int w = (LIST_W - 6) / 5;
  return {(int16_t)(LIST_X + 3 + i * w), (int16_t)(BAND_Y + 20),
          (int16_t)(w - 2), 24};
}

NodeList* GameScreen::appendTarget() {
  // If a block row is selected, new items go into its body; else the edit list.
  if (_selected >= 0) {
    std::vector<Row> rows;
    flatten(*_editList, 0, rows);
    if (_selected < (int)rows.size()) {
      Row& r = rows[_selected];
      if (r.addSlot) {  // an add-slot: inside -> block body; outside/here -> target list
        if (r.node && r.list == &r.node->body) return &r.node->body;
        return r.list;
      }
      Node* n = r.node;
      if (n->type == N_REPEAT || n->type == N_IF || n->type == N_REPEAT_UNTIL)
        return &n->body;
    }
  }
  return _editList;
}

void GameScreen::appendNodeToTarget(const Node& n) {
  appendTarget()->push_back(n);
  _selected = -1;
  _failNode = nullptr;  // editing clears the failure highlight
  _followTail = true;   // scroll so the newest command is visible
  hal::audio.blip();
  drawProgramList();
}

static const char* condName(Cond c) {
  switch (c) {
    case WALL_AHEAD: return "wall";
    case PIT_AHEAD: return "pit";
    case AT_GOAL: return "goal";
    case ENEMY_AHEAD: return "enemy";
    case ENEMY_NEAR: return "near";
  }
  return "?";
}

static void nodeLabel(const Node& n, char* buf, size_t bn, Glyph& gl, uint16_t& col) {
  switch (n.type) {
    case N_CMD:
      switch (n.cmd) {
        case CMD_FWD:    gl = Glyph::ARROW_UP;  col = C_MOVE; snprintf(buf, bn, "forward"); break;
        case CMD_BACK:   gl = Glyph::ARROW_DOWN;col = C_MOVE; snprintf(buf, bn, "back"); break;
        case CMD_TURN_L: gl = Glyph::TURN_L;    col = C_TURN; snprintf(buf, bn, "turn L"); break;
        case CMD_TURN_R: gl = Glyph::TURN_R;    col = C_TURN; snprintf(buf, bn, "turn R"); break;
        case CMD_JUMP:   gl = Glyph::JUMP;      col = C_GO;   snprintf(buf, bn, "jump"); break;
        case CMD_FIRE:   gl = Glyph::ZAP;       col = ui::rgb(255, 200, 40); snprintf(buf, bn, "zap"); break;
        default:         gl = Glyph::PLAY;      col = C_INK;  snprintf(buf, bn, "?"); break;
      }
      break;
    case N_REPEAT:       gl = Glyph::REPEAT; col = C_LOOP;  snprintf(buf, bn, "repeat %d", n.count); break;
    case N_REPEAT_UNTIL: gl = Glyph::SENSE;  col = C_SENSE; snprintf(buf, bn, "until %s", condName(n.cond)); break;
    case N_IF:           gl = Glyph::SENSE;  col = C_SENSE; snprintf(buf, bn, "if %s", condName(n.cond)); break;
    case N_CALL:         gl = Glyph::CALL;   col = C_FUNC;  snprintf(buf, bn, "call F%d", n.func); break;
    case N_NEURO:        gl = Glyph::SENSE;  col = ui::rgb(120, 230, 245);
      snprintf(buf, bn, n.rnn ? (n.pilot ? "rnn pilot" : "rnn brain")
                              : (n.pilot ? "pilot" : "brain")); break;
  }
}

void GameScreen::drawProgramList() {
  auto& g = hal::display.gfx();
  // the program pane extends to the very bottom (the bottom-bar RUN is gone), so
  // more commands fit on screen.
  g.fillRect(LIST_X, BAND_Y, LIST_W, SCREEN_H - BAND_Y, C_PANEL);
  const char* body = (_editList == &_prog.f1) ? "F1" : (_editList == &_prog.f2) ? "F2" : "MAIN";
  char hdr[28];
  _writtenCount = programWrittenCount(_prog);
  snprintf(hdr, sizeof(hdr), "%s  %d/%d", body, _writtenCount, _par);
  label(g, LIST_X + 6, BAND_Y + 5, hdr, C_INK);

  std::vector<Row> rows;
  flatten(*_editList, 0, rows);
  int n = (int)rows.size();
  if (_selected >= n) _selected = -1;

  // Hierarchical line numbers: top level 1,2,3; items nested in a block are 1a,1b,1c
  // (so it's obvious which steps live inside which loop).
  std::vector<std::string> rownum(n);
  {
    int ctr[8] = {0};
    std::string seg[8];
    for (int i = 0; i < n; i++) {
      if (rows[i].addSlot || rows[i].trainSlot) continue;  // synthetic rows have no number
      int d = rows[i].depth; if (d > 7) d = 7;
      ctr[d]++;
      for (int k = d + 1; k < 8; k++) ctr[k] = 0;
      if (d == 0) seg[0] = std::to_string(ctr[0]);
      else        seg[d] = std::string(1, (char)('a' + (ctr[d] - 1) % 26));
      std::string lbl;
      for (int k = 0; k <= d; k++) lbl += seg[k];
      rownum[i] = lbl;
    }
  }

  // MAIN | F1 | F2 | Save | Load once functions/library unlock (SPEC §10, §11.1)
  if (_profile && _profile->unlocks.func) {
    const char* tabs[3] = {"MAIN", "F1", "F2"};
    NodeList* lists[3] = {&_prog.main, &_prog.f1, &_prog.f2};
    for (int i = 0; i < 3; i++) {
      Rect r = funcTabRect(i);
      bool on = (_editList == lists[i]);
      button(g, r, tabs[i], on ? C_ACCENT : C_DIM, on ? C_PANEL_HI : C_PANEL);
    }
    button(g, funcTabRect(3), "S>L", C_GO, C_PANEL);
    button(g, funcTabRect(4), "L<L", C_FUNC, C_PANEL);
  }

  int top = listTop();
  int visible = (SCREEN_H - top) / ROW_H;  // pane now reaches the screen bottom
  if (_followTail && n > visible) _scroll = n - visible;  // keep newest in view
  if (_selected >= 0 && _selected < _scroll) _scroll = _selected;
  if (_selected >= _scroll + visible) _scroll = _selected - visible + 1;
  if (_scroll > n - visible) _scroll = (n > visible) ? n - visible : 0;
  if (_scroll < 0) _scroll = 0;

  // scrollbar (tappable) when the program overflows the visible rows
  if (n > visible) {
    int trackX = LIST_X + LIST_W - 5, trackY = top, trackH = visible * ROW_H;
    g.fillRect(trackX, trackY, 3, trackH, C_PANEL_HI);
    int thumbH = trackH * visible / n;
    if (thumbH < 8) thumbH = 8;
    int thumbY = trackY + (trackH - thumbH) * _scroll / (n - visible);
    g.fillRect(trackX, thumbY, 3, thumbH, C_ACCENT);
  }

  for (int vi = 0; vi < visible; vi++) {
    int ri = _scroll + vi;
    if (ri >= n) break;
    Row& row = rows[ri];
    int y = top + vi * ROW_H;
    bool sel = (ri == _selected);
    bool failed = (_failNode && row.node == _failNode);
    if (failed) {
      g.fillRoundRect(LIST_X + 2, y, LIST_W - 4, ROW_H - 2, 4, C_BAD);
    } else if (sel) {
      g.fillRoundRect(LIST_X + 2, y, LIST_W - 4, ROW_H - 2, 4, C_PANEL_HI);
    }
    // E-bracket for nested rows (SPEC §10)
    if (row.depth > 0) {
      int bx = LIST_X + 22 + (row.depth - 1) * 12;
      g.drawFastVLine(bx, y + 2, ROW_H - 4, row.bracket);
      g.drawFastHLine(bx, y + ROW_H / 2, 4, row.bracket);
    }
    int gx = LIST_X + 24 + row.depth * 12;

    if (row.trainSlot) {
      // the "train this brain" line under a NEURO node — a tappable button
      Rect tr = {(int16_t)(gx + 2), (int16_t)(y + 1), 120, (int16_t)(ROW_H - 3)};
      button(g, tr, "train brain >", ui::rgb(120, 230, 245), C_PANEL_HI);
      continue;
    }
    if (row.addSlot) {
      // clear "where will my next step go?" affordances
      const char* txt = !row.node ? (sel ? "+ adding here"   : "+ add here")
                                  : (sel ? "+ adding inside" : "+ add inside");
      label(g, gx + 4, y + 6, txt, sel ? C_GO : C_DIM);
      continue;
    }

    label(g, LIST_X + 2, y + 6, rownum[ri].c_str(), C_DIM);
    char lab[20]; Glyph gl = Glyph::PLAY; uint16_t col = C_INK;
    nodeLabel(*row.node, lab, sizeof(lab), gl, col);
    drawGlyph(g, gl, gx + 7, y + ROW_H / 2 - 1, 12, col);

    // A selected block shows its editor INLINE (big, mid-pane, away from HOME):
    //   repeat -> [-] N [+] ;  if/until -> the condition cycle button.
    Node* nd = row.node;
    if (sel && nd->type == N_REPEAT) {
      char rl[14]; snprintf(rl, sizeof(rl), "repeat %d", nd->count);
      button(g, repCycleRect(y), rl, C_LOOP, C_PANEL_HI);  // one big tap: cycles 2-5
    } else if (sel && (nd->type == N_IF || nd->type == N_REPEAT_UNTIL)) {
      label(g, gx + 18, y + 6, nd->type == N_IF ? "if" : "until", C_SENSE);
      button(g, condRect(y), condName(nd->cond), C_SENSE, C_PANEL_HI);
    } else if (sel && nd->type == N_CALL) {
      char cl[6]; snprintf(cl, sizeof(cl), "F%d", nd->func);
      label(g, gx + 18, y + 6, "call", C_FUNC);
      button(g, condRect(y), cl, C_FUNC, C_PANEL_HI);   // tap to switch F1<->F2
    } else if (sel && nd->type == N_NEURO) {
      const char* mode = nd->rnn ? (nd->pilot ? "rnn+pilot" : "rnn")
                                 : (nd->pilot ? "pilot" : "plain");
      button(g, condRect(y), mode, ui::rgb(120, 230, 245), C_PANEL_HI);  // cycle brain mode
    } else {
      label(g, gx + 18, y + 6, lab, C_INK);
    }
  }
  // friendly first-time onboarding (a 6yo needs to know what to do). Keyed off the real
  // program being empty — `n` is never 0 because flatten() always adds a "+ add here" row.
  if (_editList && _editList->empty()) {
    int hy = top + ROW_H + 8;
    label(g, LIST_X + 8, hy,      "tap an arrow", C_GO);
    label(g, LIST_X + 8, hy + 14, "to add a step,", C_DIM);
    label(g, LIST_X + 8, hy + 28, "then tap RUN!", C_GO);
  }
}

void GameScreen::drawBottomBar() {
  auto& g = hal::display.gfx();
  if (_mode == M_RUN) {
    g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
    button(g, R_STEP, "STEP", C_INK, C_PANEL);
    button(g, R_RESET, "RESET", C_INK, C_PANEL);
  } else if (_view == V_CODE) {
    // only the VIEW toggle on the left; the program pane owns the right side. Clear
    // right up to the pane edge so no stale button pixel (e.g. the old RUN) pokes out.
    g.fillRect(0, BOTBAR_Y, LIST_X, BOTBAR_H, C_BG);
    button(g, R_TOGGLE, "VIEW: Maze", C_ACCENT, C_PANEL);
  } else {
    g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
    button(g, R_TOGGLE, "VIEW: Code", C_ACCENT, C_PANEL);
    button(g, R_RUNBAR, "RUN >", C_GO, C_PANEL);
  }
}

void GameScreen::toast(const char* msg, uint16_t color) {
  auto& g = hal::display.gfx();
  int w = 180, h = 30, x = (SCREEN_W - w) / 2, y = BAND_Y + 8;
  g.fillRoundRect(x, y, w, h, 6, C_PANEL);
  g.drawRoundRect(x, y, w, h, 6, color);
  label(g, x + w / 2, y + h / 2, msg, color, textdatum_t::middle_center);
}

// ---- input ----------------------------------------------------------------
void GameScreen::appendCommand(Cmd c) {
  if (!_editList) return;
  if (_profile) {  // command histogram (SPEC §9)
    if (c == CMD_FWD) _profile->stats.commandsUsed[CS_FWD]++;
    else if (c == CMD_JUMP) _profile->stats.commandsUsed[CS_JUMP]++;
    else _profile->stats.commandsUsed[CS_TURN]++;
  }
  appendNodeToTarget(Node::command(c));
}

void GameScreen::handlePadTap(int x, int y) {
  // direction cells
  if (padCell(0, 1).contains(x, y)) { appendCommand(CMD_FWD); return; }
  if (padCell(1, 0).contains(x, y)) { appendCommand(CMD_TURN_L); return; }
  if (padCell(1, 2).contains(x, y)) { appendCommand(CMD_TURN_R); return; }
  if (padCell(1, 1).contains(x, y)) {  // the centre robot doubles as ZAP once the Arena opens
    if (_profile && _profile->unlocks.sense) appendCommand(CMD_FIRE);
    return;
  }
  if (padCell(2, 1).contains(x, y)) {  // the brain slot (was backward)
    if (_profile && _profile->unlocks.neuro) {
      uint8_t idx = _prog.addBrain(_profile->seedBase + (uint32_t)_prog.brains.size() * 101u + 1u);
      gb::Node loop = gb::Node::repeatUntil(gb::AT_GOAL);
      loop.body.push_back(gb::Node::neuro(idx));
      appendNodeToTarget(loop);
    } else {
      toast("NeuroBot unlocks at Lv 28", C_ACCENT);  // consistent with the corner padlocks
    }
    return;
  }
  // corner growth slots
  const int ci[4] = {0, 0, 2, 2}, cj[4] = {0, 2, 0, 2};
  const int unlockLv[4] = {6, 10, 20, 15};               // Jump / Repeat / Functions / Sense
  const char* cname[4] = {"Jump", "Repeat", "Functions", "Sense"};
  for (int s = 0; s < 4; s++) {
    if (!padCell(ci[s], cj[s]).contains(x, y)) continue;
    if (!cornerUnlocked(s)) {  // a 6yo taps a padlock -> tell them when it opens (no dead tap)
      char t[28]; snprintf(t, sizeof(t), "%s unlocks at Lv %d", cname[s], unlockLv[s]);
      toast(t, C_ACCENT);
      return;
    }
    if (s == 0) appendCommand(CMD_JUMP);
    else if (s == 1) { if (_profile) _profile->stats.commandsUsed[CS_REPEAT]++; appendNodeToTarget(Node::repeat(2)); }
    else if (s == 2) { if (_profile) _profile->stats.commandsUsed[CS_CALL]++;   appendNodeToTarget(Node::call(1)); }
    else if (s == 3) { if (_profile) _profile->stats.commandsUsed[CS_SENSE]++;  appendNodeToTarget(Node::repeatUntil(WALL_AHEAD)); }
    return;
  }
}

void GameScreen::handleListTap(int x, int y) {
  // function-body tabs + library Save/Load
  if (_profile && _profile->unlocks.func) {
    NodeList* lists[3] = {&_prog.main, &_prog.f1, &_prog.f2};
    for (int i = 0; i < 3; i++) {
      if (funcTabRect(i).contains(x, y)) {
        _editList = lists[i]; _selected = -1; _scroll = 0; hal::audio.blip();
        drawProgramList(); return;
      }
    }
    if (funcTabRect(3).contains(x, y)) { hal::audio.blip(); saveToLibrary(); return; }
    if (funcTabRect(4).contains(x, y)) { hal::audio.blip(); loadFromLibrary(); return; }
  }
  if (x < LIST_X) return;
  int top = listTop();
  int visible = (SCREEN_H - top) / ROW_H;
  std::vector<Row> rows;
  flatten(*_editList, 0, rows);
  int n = (int)rows.size();

  // scrollbar zone (right edge): page up/down
  if (x >= LIST_X + LIST_W - 9 && n > visible) {
    _followTail = false;
    if (y < top + visible * ROW_H / 2) _scroll -= visible - 1;
    else _scroll += visible - 1;
    if (_scroll < 0) _scroll = 0;
    if (_scroll > n - visible) _scroll = n - visible;
    drawProgramList();
    return;
  }
  for (int vi = 0; vi < visible; vi++) {
    int yy = top + vi * ROW_H;
    if (y >= yy && y < yy + ROW_H) {
      int ri = _scroll + vi;
      if (ri >= n) { drawProgramList(); return; }
      // tap the "train brain" line -> open the neuro interface
      if (rows[ri].trainSlot) { _pendingNeuro = rows[ri].node->brainIdx; hal::audio.blip(); return; }
      // inline controls on the currently-selected block row (big, mid-pane)
      if (ri == _selected && !rows[ri].addSlot) {
        Node* sn = rows[ri].node;
        if (sn->type == N_REPEAT) {
          if (repCycleRect(yy).contains(x, y)) {  // one big button cycles 2->3->4->5->2
            sn->count = (sn->count >= 5) ? 2 : sn->count + 1;
            hal::audio.blip(); drawProgramList(); return;
          }
        } else if (sn->type == N_IF || sn->type == N_REPEAT_UNTIL) {
          if (condRect(yy).contains(x, y)) {
            sn->cond = (sn->cond == WALL_AHEAD) ? PIT_AHEAD
                     : (sn->cond == PIT_AHEAD) ? AT_GOAL : WALL_AHEAD;
            hal::audio.blip(); drawProgramList(); return;
          }
        } else if (sn->type == N_CALL) {
          if (condRect(yy).contains(x, y)) {       // switch which function this call runs
            sn->func = (sn->func == 1) ? 2 : 1;
            hal::audio.blip(); drawProgramList(); return;
          }
        } else if (sn->type == N_NEURO) {
          if (condRect(yy).contains(x, y)) {       // plain -> pilot -> rnn -> rnn+pilot
            if (!sn->pilot && !sn->rnn)      sn->pilot = true;
            else if (sn->pilot && !sn->rnn)  { sn->pilot = false; sn->rnn = true; }
            else if (!sn->pilot && sn->rnn)  sn->pilot = true;
            else                             { sn->pilot = false; sn->rnn = false; }
            hal::audio.blip(); drawProgramList(); return;
          }
        }
      }
      _selected = (ri == _selected) ? -1 : ri; _followTail = false; hal::audio.blip();
      drawProgramList();
      drawControlPad();  // refresh DEL enabled state
      return;
    }
  }
}

void GameScreen::deleteSelected() {
  std::vector<Row> rows;
  flatten(*_editList, 0, rows);
  if (_selected < 0 || _selected >= (int)rows.size()) return;
  Row& r = rows[_selected];
  if (r.addSlot) return;  // the "+ add inside" slot isn't a real node
  r.list->erase(r.list->begin() + r.index);
  _selected = -1;
  _failNode = nullptr;  // the node pointer is now invalid; clear the fail highlight
  hal::audio.blip();
  drawProgramList();
}

void GameScreen::handleCodeTap(int x, int y) {
  if (R_CLEAR.contains(x, y)) { _editList->clear(); _selected = -1; _failNode = nullptr; hal::audio.blip(); drawProgramList(); drawControlPad(); return; }
  if (R_DELPAD.contains(x, y)) { if (_selected >= 0) { deleteSelected(); drawControlPad(); } return; }
  if (R_RUNPAD.contains(x, y)) { startRun(); return; }
  if (x < LIST_X) handlePadTap(x, y);
  else handleListTap(x, y);
}

void GameScreen::startRun() {
  if (_prog.main.empty()) { toast("add some commands!", C_ACCENT); return; }
  if (_profile) {
    _profile->stats.totalRuns++;
    if (!_challenge) {  // a challenge never touches the campaign resume slot
      _profile->workLevel = _level;     // autosave resume slot in memory (SPEC §11.1)
      _profile->work = _prog;
    }
  }
  for (auto& v : _visited) v = false;
  for (auto& v : _coinTaken) v = false;
  for (auto& v : _gemTaken) v = false;
  _gemsThisRun = 0;
  _coinsThisRun = 0;   // fresh breadcrumb trail
  _it.load(&_prog, &_maze, _maze.startPose(), DEFAULT_STEP_CAP);
  _mode = M_RUN;
  _auto = true;
  _view = V_MAZE;
  _failNode = nullptr;
  drawMazeView(false);
  drawStepBadge();
  _lastStep = 0;
}

void GameScreen::resetRun() {
  _boardIdx = 0;
  _maze = _boards[0];
  recountGems();
  for (auto& v : _visited) v = false;
  for (auto& v : _coinTaken) v = false;
  for (auto& v : _gemTaken) v = false;
  _gemsThisRun = 0;
  _coinsThisRun = 0;
  _it.load(&_prog, &_maze, _maze.startPose(), DEFAULT_STEP_CAP);
  _auto = false;
  _drawnPose = _maze.startPose();
  _tween = false;
  drawMazeView(false);
}

void GameScreen::beginAutoRun() {
  _prog.clear();
  gb::solveMaze(_maze, _profile && _profile->unlocks.jump, _prog);
  _editList = &_prog.main;
  for (auto& v : _visited) v = false;
  for (auto& v : _coinTaken) v = false;
  for (auto& v : _gemTaken) v = false;
  _gemsThisRun = 0;
  _coinsThisRun = 0;
  _it.load(&_prog, &_maze, _maze.startPose(), DEFAULT_STEP_CAP);
  _mode = M_RUN;
  _auto = false;        // paused — advance with debugStep()
  _view = V_MAZE;
  _tween = false;
  _failNode = nullptr;
  _previewing = false;
  _drawnPose = _maze.startPose();
  drawMazeView(false);
  _lastStep = 0;
}

void GameScreen::debugStep() {
  if (_mode == M_RUN && !_tween && !_it.finished()) stepOnce(millis());
}

void GameScreen::recountGems() {
  _gemTotal = 0;
  for (int r = 0; r < _maze.rows(); r++)
    for (int c = 0; c < _maze.cols(); c++)
      if (_maze.at(r, c) == STAR) _gemTotal++;
}

void GameScreen::advanceBoard() {
  _boardIdx++;
  _maze = _boards[_boardIdx];
  recountGems();
  for (auto& v : _visited) v = false;
  for (auto& v : _coinTaken) v = false;
  for (auto& v : _gemTaken) v = false;
  _gemsThisRun = 0;
  _coinsThisRun = 0;
  _it.load(&_prog, &_maze, _maze.startPose(), DEFAULT_STEP_CAP);
  _drawnPose = _maze.startPose();
  _tween = false;
  drawMazeView(false);
  char buf[24];
  snprintf(buf, sizeof(buf), "Maze %d / %d", _boardIdx + 1, _boardCount);
  toast(buf, C_ACCENT);
  _lastStep = 0;  // brief pause then continue auto-stepping
}

void GameScreen::saveToLibrary() {
  if (!_profile) return;
  if (_prog.main.empty()) { toast("nothing to save", C_ACCENT); return; }
  if ((int)_profile->library.size() >= gb::LIBRARY_MAX) { toast("library full", C_BAD); return; }
  gb::LibEntry e;
  char nm[16];
  snprintf(nm, sizeof(nm), "Lib %d", (int)_profile->library.size() + 1);
  e.name = nm;
  e.program = _prog;
  _profile->library.push_back(e);
  drawProgramList();
  toast("saved to library", C_GO);
}

void GameScreen::loadFromLibrary() {
  if (!_profile || _profile->library.empty()) { toast("library empty", C_ACCENT); return; }
  _prog = _profile->library.back().program;  // load most recent (SPEC §11.1)
  _editList = &_prog.main;
  _selected = -1;
  drawProgramList();
  toast("loaded from library", C_GO);
}

void GameScreen::stepOnce(uint32_t now) {
  if (_it.finished()) return;
  Pose old = _it.pose();
  _visited[old.row * _maze.cols() + old.col] = true;  // drop a breadcrumb
  Outcome o = _it.step();
  // collect a coin or a gem if we landed on one
  int ci = _it.pose().row * _maze.cols() + _it.pose().col;
  Tile here = _maze.at(_it.pose().row, _it.pose().col);
  if (here == COIN && !_coinTaken[ci]) {
    _coinTaken[ci] = true; _coinsThisRun++; hal::audio.blip();
  } else if (here == STAR && !_gemTaken[ci]) {
    _gemTaken[ci] = true; _gemsThisRun++; hal::audio.badge();  // sparkly chime
  }
  drawStepBadge();  // refresh the top-chrome step count
  bool moved = (old.row != _it.pose().row || old.col != _it.pose().col);
  if (moved && (o == OUT_OK || o == OUT_WIN)) {
    // glide between tiles; settleOutcome() runs when the tween lands
    _tween = true; _tweenFrom = old; _tweenTo = _it.pose();
    _tweenT0 = now; _pendingOutcome = o;
    return;
  }
  // a turn (no move) or a failure: redraw in place, then resolve immediately
  drawCell(old.row, old.col);
  drawCharacterAt(_it.pose());
  _drawnPose = _it.pose();
  if (_it.lastCmd() == CMD_FIRE) {  // ZAP animation — a bolt + sparks in the tile ahead
    auto& g = hal::display.gfx();   // (a no-op for solving outside the Arena, but it animates)
    int tile, ox, oy; mazeGeometry(tile, ox, oy);
    int dr, dc; facingDelta(_it.pose().facing, dr, dc);
    int zc = _it.pose().col + dc, zr = _it.pose().row + dr;
    if (_maze.inBounds(zr, zc)) {
      int cx = ox + zc * tile + tile / 2, cy = oy + zr * tile + tile / 2;
      for (int a = 0; a < 360; a += 45) {  // a little spark burst
        float rr = a * 3.14159f / 180.f;
        g.drawLine(cx, cy, cx + (int)(cosf(rr) * tile / 2), cy + (int)(sinf(rr) * tile / 2), ui::rgb(255, 220, 60));
      }
      drawGlyph(g, Glyph::ZAP, cx, cy, tile / 2, ui::rgb(255, 240, 120));
    }
    hal::audio.blip();
  }
  settleOutcome(o);
}

void GameScreen::settleOutcome(Outcome o) {
  if (o == OUT_OK) { hal::audio.tick(); return; }
  if (o == OUT_WIN) {
    // Multi-maze generalization level: the same program must clear every board.
    if (_boardIdx + 1 < _boardCount) { hal::audio.tick(); advanceBoard(); return; }
    _mode = M_WIN;
    _writtenCount = programWrittenCount(_prog);
    _stars = starsFor(_writtenCount, _par);
    hal::audio.win();
    hal::led.green();
    // gems pay out in coins; grabbing every gem on the board adds an all-clear bonus
    bool allGems = _gemTotal > 0 && _gemsThisRun == _gemTotal;
    int gemCoins = _gemsThisRun * 3 + (allGems ? 5 : 0);
    if (_profile && !_challenge) {  // challenges award no coins/stats (not farmable)
      _profile->stats.totalWins++;
      _profile->stats.starsTotal += _stars;
      _profile->coins += _coinsThisRun + gemCoins;  // keep collected coins+gems on a win
    }
    winCelebration();
    auto& g = hal::display.gfx();
    bool showGems = _gemsThisRun > 0;
    int w = 210, h = showGems ? 108 : 90, x = (SCREEN_W - w) / 2, yy = (SCREEN_H - h) / 2;
    g.fillRoundRect(x, yy, w, h, 10, C_PANEL);
    g.drawRoundRect(x, yy, w, h, 10, C_GO);
    label(g, SCREEN_W / 2, yy + 20, "YOU WIN!", C_GO, textdatum_t::middle_center, 2);
    // star rating: dim placeholders for all three, then pop each earned star in
    int sy = yy + 44;
    for (int i = 0; i < 3; i++) {
      int sx = SCREEN_W / 2 + (i - 1) * 30;
      g.fillCircle(sx, sy, 12, C_PANEL);
      g.drawCircle(sx, sy, 8, C_LOCK);  // empty slot
    }
    for (int i = 0; i < _stars; i++) {
      int sx = SCREEN_W / 2 + (i - 1) * 30;
      const int sizes[3] = {10, 24, 19};  // grow -> overshoot -> settle (a little pop)
      for (int f = 0; f < 3; f++) {
        g.fillCircle(sx, sy, 13, C_PANEL);
        assets::drawEmoji(g, 2, sx, sy, sizes[f]);  // gold 5-point star
        delay(55);
      }
    }
    char st[28];
    int totalCoins = _challenge ? 0 : (_coinsThisRun + gemCoins), ly = yy + 60;
    if (totalCoins > 0) {
      snprintf(st, sizeof(st), "+%d coins!", totalCoins);
      label(g, SCREEN_W / 2, ly, st, ui::rgb(255, 210, 60), textdatum_t::middle_center);
      ly += 16;
    }
    if (showGems) {
      if (allGems) snprintf(st, sizeof(st), "All %d gems! +5", _gemTotal);
      else         snprintf(st, sizeof(st), "%d/%d gems", _gemsThisRun, _gemTotal);
      label(g, SCREEN_W / 2, ly, st, ui::rgb(120, 230, 245), textdatum_t::middle_center);
    }
    label(g, SCREEN_W / 2, yy + h - 12, "tap to continue", C_DIM, textdatum_t::middle_center);
    return;
  }
  // failure (BONK / FELL / DONE_NO_WIN)
  if (_profile) {
    if (o == OUT_BONK) _profile->stats.bonks++;
    else if (o == OUT_FELL) _profile->stats.falls++;
  }
  _failNode = _it.currentNode();
  _mode = M_FAIL;
  _auto = false;
  hal::audio.fail();
  hal::led.red();
  // shake + dizzy (X eyes) so the failure reads clearly (SPEC §8.3)
  auto& g = hal::display.gfx();
  int tile, ox, oy; mazeGeometry(tile, ox, oy);
  int cx = ox + _it.pose().col * tile + tile / 2;
  int cy = oy + _it.pose().row * tile + tile / 2;
  for (int s = 0; s < 3; s++) {
    drawCell(_it.pose().row, _it.pose().col);
    drawCharacterPx(cx + (s & 1 ? 3 : -3), cy, _it.pose().facing, 2);
    delay(55);
  }
  drawCell(_it.pose().row, _it.pose().col);
  drawCharacterPx(cx, cy, _it.pose().facing, 2);
  const char* msg = (o == OUT_BONK) ? "Bonk! a wall - tap to fix" :
                    (o == OUT_FELL) ? "Splash! a pit - tap to fix" :
                                      "Missed the goal - tap to fix";
  toast(msg, C_BAD);
}

// ---- main tick ------------------------------------------------------------
app::Signal GameScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx = 0, ty = 0;
  bool tap = _tap.tapped(tp, now, tx, ty);

  // HOME/back always exits to the profile menu, from ANY mode (run/win/fail too) — it
  // used to be checked after the mode handlers, so tapping it on the win/fail screen was
  // swallowed and dropped you back in the editor instead. Autosave the resume slot first.
  if (tap && R_PAUSE.contains(tx, ty)) {
    if (_profile) { _profile->workLevel = _level; _profile->work = _prog; }
    return app::Signal::BACK;
  }

  // level-start maze preview: hold ~2.5s (or until tapped), then go to code view
  if (_previewing) {
    if (_previewStart == 0) _previewStart = now;
    if (tap || now - _previewStart > 2500) {
      _previewing = false;
      _view = V_CODE;
      drawCodeView();
    }
    return app::Signal::NONE;
  }

  if (_mode == M_WIN) {
    if (tap) return app::Signal::WON;
    return app::Signal::NONE;
  }

  // Failure: stay on the maze showing the reddened character + message; a tap takes
  // the kid to the Code view with the failing instruction flashed (SPEC §8.3).
  if (_mode == M_FAIL) {
    if (tap) { _mode = M_EDIT; _view = V_CODE; drawCodeView(); }
    return app::Signal::NONE;
  }

  if (_mode == M_RUN) {
    if (_tween) {
      // glide the robot between tiles, then resolve the move's outcome
      uint32_t dur = (_stepMs > 140) ? (_stepMs - 70) : _stepMs / 2;
      int tile, ox, oy; mazeGeometry(tile, ox, oy);
      int fx = ox + _tweenFrom.col * tile + tile / 2, fy = oy + _tweenFrom.row * tile + tile / 2;
      int gx = ox + _tweenTo.col * tile + tile / 2,   gy = oy + _tweenTo.row * tile + tile / 2;
      uint32_t el = now - _tweenT0;
      if (el >= dur) {
        _tween = false;
        redrawCellsBetween(_tweenFrom, _tweenTo);
        drawCharacterAt(_tweenTo);
        _drawnPose = _tweenTo;
        settleOutcome(_pendingOutcome);
      } else {
        float t = el / (float)dur;
        int cx = fx + (int)((gx - fx) * t), cy = fy + (int)((gy - fy) * t);
        // A JUMP spans 2 tiles -> hop in a little arc: rise UP when travelling sideways
        // across the screen, swing to the SIDE when travelling up/down (SPEC §8.1 jump).
        int dRow = _tweenTo.row - _tweenFrom.row, dCol = _tweenTo.col - _tweenFrom.col;
        int span = (dRow < 0 ? -dRow : dRow) + (dCol < 0 ? -dCol : dCol);
        if (span >= 2) {
          float h = 4.0f * t * (1.0f - t);        // parabola: 0 -> 1 (mid) -> 0
          int peak = tile / 2;                    // within redrawCellsBetween's 1-cell margin
          if (dCol != 0) cy -= (int)(h * peak);   // horizontal travel -> arc upward
          else           cx += (int)(h * peak);   // vertical travel -> arc to the side
        }
        redrawCellsBetween(_tweenFrom, _tweenTo);
        drawCharacterPx(cx, cy, _tweenTo.facing, 0);
      }
    } else if (_auto && (_lastStep == 0 || now - _lastStep >= _stepMs)) {
      stepOnce(now);
      _lastStep = now;
    }
    // RESET must work even mid-glide so a runaway run (e.g. a brain that never reaches
    // the goal and loops to the step cap) can always be stopped. STEP only between glides.
    if (tap && _mode == M_RUN) {
      if (R_RESET.contains(tx, ty)) { resetRun(); }
      else if (!_tween && R_STEP.contains(tx, ty)) { _auto = false; stepOnce(now); _lastStep = now; }
    }
    return app::Signal::NONE;
  }

  // deferred view toggle + hold-to-peek
  if (tap && _view == V_CODE && R_TOGGLE.contains(tx, ty)) {
    _toggleActive = true; _peeking = false; tap = false;
  } else if (tap && _view == V_MAZE && R_TOGGLE.contains(tx, ty)) {
    _view = V_CODE; drawCodeView(); tap = false;
  }
  if (_toggleActive) {
    if (_view == V_CODE && !_peeking && _tap.heldMs(now) > 300) {
      _peeking = true; drawMazeView(true);
    }
    if (!_tap.held()) {
      if (_peeking) { _peeking = false; drawCodeView(); }
      else { _view = V_MAZE; drawMazeView(false); }
      _toggleActive = false;
    }
    return app::Signal::NONE;
  }

  if (tap) {
    if (_view == V_CODE) handleCodeTap(tx, ty);
    else if (R_RUNBAR.contains(tx, ty)) startRun();
    if (_pendingNeuro >= 0) return app::Signal::GOTO_NEURO_TRAIN;  // open the neuro interface
  }
  return app::Signal::NONE;
}

}  // namespace screens
