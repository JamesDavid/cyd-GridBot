#include "screens/GameScreen.h"

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
static const Rect R_RMINUS = {246, 24, 18, 16};   // repeat count -
static const Rect R_RPLUS  = {268, 24, 18, 16};   // repeat count +
static const Rect R_COND   = {244, 24, 46, 16};   // sense condition cycle
static constexpr int ROW_Y0 = BAND_Y + 22, ROW_H = 24;
static const Rect R_PAUSE = {238, 0, 82, 22};   // big back button in the status bar
static const Rect R_TOGGLE = {6, (int16_t)(BOTBAR_Y + 2), 150, 26};
static const Rect R_RUNBAR = {164, (int16_t)(BOTBAR_Y + 2), 150, 26};
static const Rect R_STEP = {6, (int16_t)(BOTBAR_Y + 2), 150, 26};
static const Rect R_RESET = {164, (int16_t)(BOTBAR_Y + 2), 150, 26};

// ---------------------------------------------------------------------------
void GameScreen::begin(Profile* profile, uint32_t level) {
  _profile = profile;
  _level = level;
  _boardCount = MazeGen::generateBoards(_boards, gb::MAX_BOARDS,
                                        profile ? profile->seedBase : 0, level);
  _boardIdx = 0;
  _maze = _boards[0];
  _prog.clear();
  // Resume the in-progress script for this level if present (SPEC §11.1).
  if (profile && profile->workLevel == level && !profile->work.empty()) {
    _prog = profile->work;
  }
  _editList = &_prog.main;
  _par = shortestSolutionLen(_maze, profile && profile->unlocks.jump);
  if (_par <= 0) _par = 1;
  _stepMs = profile ? animStepMs(profile->settings) : 400;
  hal::audio.setEnabled(profile ? profile->settings.sound : true);
  _view = V_CODE;
  _mode = M_EDIT;
  _auto = false;
  _selected = -1;
  _scroll = 0;
  _followTail = true;
  _failNode = nullptr;
  _drawnPose = _maze.startPose();
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
  if (_boardCount > 1)
    snprintf(buf, sizeof(buf), "%s  Lv %u  maze %d/%d", nm, (unsigned)_level,
             _boardIdx + 1, _boardCount);
  else
    snprintf(buf, sizeof(buf), "%s   Lv %u", nm, (unsigned)_level);
  label(g, 6, 4, buf, C_INK);
  uint32_t stars = _profile ? _profile->stats.starsTotal : 0;
  snprintf(buf, sizeof(buf), "*%u", (unsigned)stars);
  label(g, 232, 4, buf, C_ACCENT, textdatum_t::top_right);
  // big back button (SPEC §10): returns to the profile menu
  button(g, R_PAUSE, "< HOME", C_INK, C_PANEL_HI);
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
    g.fillRect(x, y, tile - 1, tile - 1, C_WALL);
    // simple brick courses
    g.drawFastHLine(x, y + tile / 3, tile - 1, C_WALL_LINE);
    g.drawFastHLine(x, y + 2 * tile / 3, tile - 1, C_WALL_LINE);
    g.drawFastVLine(x + tile / 2, y, tile / 3, C_WALL_LINE);
    g.drawFastVLine(x + tile / 4, y + tile / 3, tile / 3, C_WALL_LINE);
    g.drawFastVLine(x + 3 * tile / 4, y + tile / 3, tile / 3, C_WALL_LINE);
  } else {
    g.fillRect(x, y, tile - 1, tile - 1, ((r + c) & 1) ? C_FLOOR : C_FLOOR2);
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

void GameScreen::drawCharacterAt(const Pose& p) {
  auto& g = hal::display.gfx();
  int tile, ox, oy;
  mazeGeometry(tile, ox, oy);
  int cx = ox + p.col * tile + tile / 2;
  int cy = oy + p.row * tile + tile / 2;
  if (_profile && spriteHasPixels(_profile->customChar)) {
    assets::drawCustomSprite(g, cx, cy, tile, _profile->customChar.data());
    // facing cue so the heading still reads
    int dr, dc; facingDelta(p.facing, dr, dc);
    int nx = cx + dc * (tile / 2 - 2), ny = cy + dr * (tile / 2 - 2);
    g.fillCircle(nx, ny, 2, C_ACCENT);
  } else {
    assets::drawCharacter(g, cx, cy, tile, _profile ? _profile->avatar : 0, p.facing);
  }
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
  bool bw = _profile && _profile->unlocks.backward;
  cell(0, 1, Glyph::ARROW_UP, C_MOVE, false);          // FORWARD
  cell(1, 0, Glyph::TURN_L, C_TURN, false);            // TURN LEFT
  cell(1, 2, Glyph::TURN_R, C_TURN, false);            // TURN RIGHT
  cell(2, 1, Glyph::ARROW_DOWN, C_MOVE, !bw);          // BACKWARD (locked < Tier1.5)
  // centre avatar — faces the maze's START orientation so the kid can reason about
  // "forward" before running.
  Rect ctr = padCell(1, 1);
  panel(g, ctr, C_PANEL);
  if (_profile && spriteHasPixels(_profile->customChar))
    assets::drawCustomSprite(g, ctr.cx(), ctr.cy(), 44, _profile->customChar.data());
  else
    assets::drawCharacter(g, ctr.cx(), ctr.cy(), 44, _profile ? _profile->avatar : 0,
                          _maze.startPose().facing);
  // four corner growth slots
  const Glyph cg[4] = {Glyph::JUMP, Glyph::REPEAT, Glyph::CALL, Glyph::SENSE};
  const uint16_t cc[4] = {C_GO, C_LOOP, C_FUNC, C_SENSE};
  const int ci[4] = {0, 0, 2, 2}, cj[4] = {0, 2, 0, 2};
  for (int s = 0; s < 4; s++)
    cell(ci[s], cj[s], cg[s], cc[s], !cornerUnlocked(s));
  // CLEAR / DEL / RUN under the pad
  button(g, R_CLEAR, "CLR", C_BAD, C_PANEL);
  bool canDel = (_selected >= 0);
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
    out.push_back({&list[i], &list, (int)i, depth, bracket});
    if (!list[i].body.empty())
      flatten(list[i].body, depth + 1, out, bracketColorFor(list[i]));
  }
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
      Node* n = rows[_selected].node;
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
        default:         gl = Glyph::PLAY;      col = C_INK;  snprintf(buf, bn, "?"); break;
      }
      break;
    case N_REPEAT:       gl = Glyph::REPEAT; col = C_LOOP;  snprintf(buf, bn, "repeat %d", n.count); break;
    case N_REPEAT_UNTIL: gl = Glyph::SENSE;  col = C_SENSE; snprintf(buf, bn, "until %s", condName(n.cond)); break;
    case N_IF:           gl = Glyph::SENSE;  col = C_SENSE; snprintf(buf, bn, "if %s", condName(n.cond)); break;
    case N_CALL:         gl = Glyph::CALL;   col = C_FUNC;  snprintf(buf, bn, "call F%d", n.func); break;
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

  // header controls for the selected row
  if (_selected >= 0) {
    Node* sn = rows[_selected].node;
    if (sn->type == N_REPEAT) {
      button(g, R_RMINUS, "-", C_LOOP, C_PANEL_HI);
      button(g, R_RPLUS, "+", C_LOOP, C_PANEL_HI);
    } else if (sn->type == N_IF || sn->type == N_REPEAT_UNTIL) {
      button(g, R_COND, condName(sn->cond), C_SENSE, C_PANEL_HI);  // tap to cycle
    }
    // (delete is the big DEL button under the pad)
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
    char num[6]; snprintf(num, sizeof(num), "%d", ri + 1);
    label(g, LIST_X + 4, y + 6, num, C_DIM);
    // E-bracket for nested rows (SPEC §10)
    if (row.depth > 0) {
      int bx = LIST_X + 22 + (row.depth - 1) * 12;
      g.drawFastVLine(bx, y + 2, ROW_H - 4, row.bracket);
      g.drawFastHLine(bx, y + ROW_H / 2, 4, row.bracket);
    }
    int gx = LIST_X + 24 + row.depth * 12;
    char lab[20]; Glyph gl = Glyph::PLAY; uint16_t col = C_INK;
    nodeLabel(*row.node, lab, sizeof(lab), gl, col);
    drawGlyph(g, gl, gx + 7, y + ROW_H / 2 - 1, 12, col);
    label(g, gx + 18, y + 6, lab, C_INK);
  }
  if (n == 0)
    label(g, LIST_X + 8, top + 8, "tap pad to add", C_DIM);
}

void GameScreen::drawBottomBar() {
  auto& g = hal::display.gfx();
  if (_mode == M_RUN) {
    g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
    button(g, R_STEP, "STEP", C_INK, C_PANEL);
    button(g, R_RESET, "RESET", C_INK, C_PANEL);
  } else if (_view == V_CODE) {
    // only the VIEW toggle on the left; the program pane owns the right side
    g.fillRect(0, BOTBAR_Y, LIST_X - 2, BOTBAR_H, C_BG);
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
    else if (c == CMD_BACK) _profile->stats.commandsUsed[CS_BACK]++;
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
  if (padCell(2, 1).contains(x, y)) {
    if (_profile && _profile->unlocks.backward) appendCommand(CMD_BACK);
    return;
  }
  // corner growth slots
  const int ci[4] = {0, 0, 2, 2}, cj[4] = {0, 2, 0, 2};
  for (int s = 0; s < 4; s++) {
    if (padCell(ci[s], cj[s]).contains(x, y) && cornerUnlocked(s)) {
      if (s == 0) appendCommand(CMD_JUMP);
      else if (s == 1) { if (_profile) _profile->stats.commandsUsed[CS_REPEAT]++; appendNodeToTarget(Node::repeat(2)); }
      else if (s == 2) { if (_profile) _profile->stats.commandsUsed[CS_CALL]++;   appendNodeToTarget(Node::call(1)); }
      else if (s == 3) { if (_profile) _profile->stats.commandsUsed[CS_SENSE]++;  appendNodeToTarget(Node::repeatUntil(WALL_AHEAD)); }
      return;
    }
  }
}

void GameScreen::handleListTap(int x, int y) {
  // selected-row header controls
  if (_selected >= 0) {
    std::vector<Row> rows;
    flatten(*_editList, 0, rows);
    if (_selected < (int)rows.size()) {
      Node* sn = rows[_selected].node;
      if (sn->type == N_REPEAT) {
        if (R_RMINUS.contains(x, y)) { if (sn->count > 2) sn->count--; hal::audio.blip(); drawProgramList(); return; }
        if (R_RPLUS.contains(x, y))  { if (sn->count < 5) sn->count++; hal::audio.blip(); drawProgramList(); return; }
      } else if (sn->type == N_IF || sn->type == N_REPEAT_UNTIL) {
        if (R_COND.contains(x, y)) {
          // cycle WALL_AHEAD -> PIT_AHEAD -> AT_GOAL
          sn->cond = (sn->cond == WALL_AHEAD) ? PIT_AHEAD
                   : (sn->cond == PIT_AHEAD) ? AT_GOAL : WALL_AHEAD;
          hal::audio.blip(); drawProgramList(); return;
        }
      }
    }
  }
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
      if (ri < n) { _selected = (ri == _selected) ? -1 : ri; _followTail = false; hal::audio.blip(); }
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
    _profile->workLevel = _level;       // autosave resume slot in memory (SPEC §11.1)
    _profile->work = _prog;
  }
  _it.load(&_prog, &_maze, _maze.startPose(), DEFAULT_STEP_CAP);
  _mode = M_RUN;
  _auto = true;
  _view = V_MAZE;
  _failNode = nullptr;
  drawMazeView(false);
  _lastStep = 0;
}

void GameScreen::resetRun() {
  _boardIdx = 0;
  _maze = _boards[0];
  _it.load(&_prog, &_maze, _maze.startPose(), DEFAULT_STEP_CAP);
  _auto = false;
  _drawnPose = _maze.startPose();
  drawMazeView(false);
}

void GameScreen::advanceBoard() {
  _boardIdx++;
  _maze = _boards[_boardIdx];
  _it.load(&_prog, &_maze, _maze.startPose(), DEFAULT_STEP_CAP);
  _drawnPose = _maze.startPose();
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
  (void)now;
  if (_it.finished()) return;
  Pose old = _it.pose();
  Outcome o = _it.step();
  // dirty-rect: clear the cell we left, redraw the goal/character.
  drawCell(old.row, old.col);
  drawCharacterAt(_it.pose());
  _drawnPose = _it.pose();

  if (o == OUT_OK) { hal::audio.tick(); return; }
  if (o == OUT_WIN) {
    // Multi-maze generalization level: the same program must clear every board.
    if (_boardIdx + 1 < _boardCount) { hal::audio.tick(); advanceBoard(); return; }
    _mode = M_WIN;
    _writtenCount = programWrittenCount(_prog);
    _stars = starsFor(_writtenCount, _par);
    hal::audio.win();
    hal::led.green();
    if (_profile) {
      _profile->stats.totalWins++;
      _profile->stats.starsTotal += _stars;
    }
    // win overlay
    auto& g = hal::display.gfx();
    int w = 200, h = 80, x = (SCREEN_W - w) / 2, yy = (SCREEN_H - h) / 2;
    g.fillRoundRect(x, yy, w, h, 10, C_PANEL);
    g.drawRoundRect(x, yy, w, h, 10, C_GO);
    label(g, SCREEN_W / 2, yy + 22, "YOU WIN!", C_GO, textdatum_t::middle_center, 2);
    char st[16]; snprintf(st, sizeof(st), "%d star%s", _stars, _stars == 1 ? "" : "s");
    label(g, SCREEN_W / 2, yy + 50, st, C_ACCENT, textdatum_t::middle_center);
    label(g, SCREEN_W / 2, yy + 68, "tap to continue", C_DIM, textdatum_t::middle_center);
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
  // Redden + "shake" the character in place so the failure is visible on the maze
  // (SPEC §8.3) — don't snap straight back to code.
  {
    auto& g = hal::display.gfx();
    int tile, ox, oy; mazeGeometry(tile, ox, oy);
    int cx = ox + _it.pose().col * tile + tile / 2;
    int cy = oy + _it.pose().row * tile + tile / 2;
    for (int s = 0; s < 3; s++) {  // quick shake
      g.fillCircle(cx + (s & 1 ? 2 : -2), cy, tile * 0.36f, C_BAD);
      delay(40);
      drawCell(_it.pose().row, _it.pose().col);
      drawCharacterAt(_it.pose());
      delay(30);
    }
    g.fillCircle(cx, cy, tile * 0.36f, C_BAD);
    g.drawCircle(cx, cy, tile * 0.36f + 1, ui::C_INK);
  }
  const char* msg = (o == OUT_BONK) ? "Bonk! a wall - tap to fix" :
                    (o == OUT_FELL) ? "Splash! a pit - tap to fix" :
                                      "Missed the goal - tap to fix";
  toast(msg, C_BAD);
}

// ---- main tick ------------------------------------------------------------
app::Signal GameScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx = 0, ty = 0;
  bool tap = _tap.tapped(tp, now, tx, ty);

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
    if (_auto && (_lastStep == 0 || now - _lastStep >= _stepMs)) {
      stepOnce(now);
      _lastStep = now;
    }
    if (tap && _mode == M_RUN) {
      if (R_STEP.contains(tx, ty)) { _auto = false; stepOnce(now); _lastStep = now; }
      else if (R_RESET.contains(tx, ty)) { resetRun(); }
    }
    return app::Signal::NONE;
  }

  // back/pause -> profile menu (autosave the resume slot first, SPEC §11.1)
  if (tap && R_PAUSE.contains(tx, ty)) {
    if (_profile) { _profile->workLevel = _level; _profile->work = _prog; }
    return app::Signal::BACK;
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
  }
  return app::Signal::NONE;
}

}  // namespace screens
