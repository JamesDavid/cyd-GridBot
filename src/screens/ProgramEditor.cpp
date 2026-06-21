#include "screens/ProgramEditor.h"
#include "hal/Display.h"
#include "hal/Audio.h"
#include "ui/UI.h"
#include "ui/Theme.h"
#include "assets/Assets.h"
#include "game/Score.h"

using namespace ui;
using namespace gb;

namespace screens {

// ---- layout rects (320x240, SPEC §10) — shared by every host that embeds the editor --
static constexpr int PAD_X0 = 6, PAD_Y0 = BAND_Y + 2, PAD_CELL = 50, PAD_PITCH = 52;
static Rect padCell(int i, int j) { return {(int16_t)(PAD_X0 + j * PAD_PITCH),
                                            (int16_t)(PAD_Y0 + i * PAD_PITCH),
                                            PAD_CELL, PAD_CELL}; }
static const Rect R_CLEAR  = {6, 179, 30, 30};
static const Rect R_DELPAD = {38, 179, 30, 30};
static const Rect R_UP     = {70, 179, 26, 30};    // move selected line up
static const Rect R_DN     = {98, 179, 26, 30};    // move selected line down (= insert anywhere)
static const Rect R_RUNPAD = {126, 179, 36, 30};
static constexpr int LIST_X = 166, LIST_W = 320 - 166;
static constexpr int ROW_Y0 = BAND_Y + 22, ROW_H = 24;
static inline Rect repCycleRect(int y) { return {(int16_t)(LIST_X + LIST_W - 92), (int16_t)(y + 1), 80, (int16_t)(ROW_H - 2)}; }
static inline Rect condRect(int y)     { return {(int16_t)(LIST_X + LIST_W - 92), (int16_t)(y + 1), 80, (int16_t)(ROW_H - 2)}; }
static inline Rect callCycleRect(int y){ return {(int16_t)(LIST_X + LIST_W - 92), (int16_t)(y + 1), 80, (int16_t)(ROW_H - 2)}; }

static bool spriteHasPixels(const std::vector<uint8_t>& v) {
  if (v.size() != (size_t)gb::PIX_CELLS) return false;
  for (uint8_t b : v) if (b) return true;
  return false;
}

static const char* condName(Cond c) {
  switch (c) {
    case WALL_AHEAD: return "wall";
    case PIT_AHEAD: return "pit";
    case AT_GOAL: return "goal";
    case ENEMY_AHEAD: return "enemy";
    case ENEMY_NEAR: return "near";
    case BLOCKED_AHEAD: return "wall/pit";
  }
  return "?";
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
    case N_NEURO:        gl = Glyph::SENSE;  col = ui::rgb(120, 230, 245);
      snprintf(buf, bn, n.rnn ? (n.pilot ? "rnn pilot" : "rnn brain")
                              : (n.pilot ? "pilot" : "brain")); break;
  }
}

// ---------------------------------------------------------------------------
void ProgramEditor::attach(Program* prog, const EditorConfig& cfg, int par, Profile* profile) {
  _prog = prog; _cfg = cfg; _par = par; _profile = profile;
  _editList = &_prog->main;
  _selected = -1; _scroll = 0; _followTail = true;
  _failNode = nullptr; _neuroIdx = -1;
}

void ProgramEditor::rebind(Program* prog) {
  _prog = prog; _editList = &_prog->main;
  _selected = -1; _scroll = 0; _followTail = true; _failNode = nullptr;
}

bool ProgramEditor::cornerUnlocked(int slot) const {
  switch (slot) {
    case 0: return _cfg.jump;
    case 1: return _cfg.repeat;
    case 2: return _cfg.call;
    case 3: return _cfg.sense;
  }
  return false;
}

int ProgramEditor::listTop() const { return ROW_Y0 + (_cfg.func ? 26 : 0); }

ui::Rect ProgramEditor::funcTabRect(int i) const {
  int w = (LIST_W - 6) / 5;
  return {(int16_t)(LIST_X + 3 + i * w), (int16_t)(BAND_Y + 20), (int16_t)(w - 2), 24};
}

void ProgramEditor::draw() { drawControlPad(); drawProgramList(); }

void ProgramEditor::drawControlPad() {
  auto& g = hal::display.gfx();
  auto cell = [&](int i, int j, Glyph gl, uint16_t col, bool locked) {
    Rect r = padCell(i, j);
    panel(g, r, locked ? C_LOCK : C_PANEL_HI);
    drawGlyph(g, locked ? Glyph::LOCK : gl, r.cx(), r.cy(), 22, locked ? C_DIM : col);
  };
  cell(0, 1, Glyph::ARROW_UP, C_MOVE, false);  // FORWARD
  cell(1, 0, Glyph::TURN_L, C_TURN, false);    // TURN LEFT
  cell(1, 2, Glyph::TURN_R, C_TURN, false);    // TURN RIGHT
  // bottom-centre slot: the NeuroBot brain block once unlocked (the old BACKWARD slot).
  if (_cfg.neuro)
    button(g, padCell(2, 1), "+brain", ui::rgb(120, 230, 245), C_PANEL);
  else
    panel(g, padCell(2, 1), C_PANEL);
  // centre avatar — faces the maze's START orientation
  Rect ctr = padCell(1, 1);
  panel(g, ctr, C_PANEL);
  if (_cfg.customChar && spriteHasPixels(*_cfg.customChar))
    assets::drawCustomSprite(g, ctr.cx(), ctr.cy(), 44, _cfg.customChar->data());
  else
    assets::drawCharacter(g, ctr.cx(), ctr.cy(), 44, _cfg.avatar, _cfg.facing);
  // four corner growth slots
  const Glyph cg[4] = {Glyph::JUMP, Glyph::REPEAT, Glyph::CALL, Glyph::SENSE};
  const uint16_t cc[4] = {C_GO, C_LOOP, C_FUNC, C_SENSE};
  const int ci[4] = {0, 0, 2, 2}, cj[4] = {0, 2, 0, 2};
  for (int s = 0; s < 4; s++) cell(ci[s], cj[s], cg[s], cc[s], !cornerUnlocked(s));
  button(g, R_CLEAR, "CLR", C_BAD, C_PANEL);
  bool canDel = false, canUp = false, canDn = false;
  if (_selected >= 0) {
    std::vector<Row> rows; flatten(*_editList, 0, rows);
    if (_selected < (int)rows.size() && !rows[_selected].addSlot && !rows[_selected].trainSlot) {
      canDel = true;
      Row& r = rows[_selected];
      canUp = r.index > 0;
      canDn = r.index + 1 < (int)r.list->size();
    }
  }
  button(g, R_DELPAD, "DEL", canDel ? C_BAD : C_DIM, C_PANEL);
  button(g, R_UP, "Up", canUp ? C_INK : C_DIM, C_PANEL);
  button(g, R_DN, "Dn", canDn ? C_INK : C_DIM, C_PANEL);
  button(g, R_RUNPAD, "RUN", C_GO, C_PANEL);
}

void ProgramEditor::flatten(NodeList& list, int depth, std::vector<Row>& out, uint16_t bracket) {
  for (size_t i = 0; i < list.size(); i++) {
    Node& nd = list[i];
    out.push_back({&nd, &list, (int)i, depth, bracket, false});
    bool block = (nd.type == N_REPEAT || nd.type == N_IF || nd.type == N_REPEAT_UNTIL);
    if (block) {
      uint16_t br = bracketColorFor(nd);
      if (!nd.body.empty()) flatten(nd.body, depth + 1, out, br);
      if (!_cfg.readOnly) out.push_back({&nd, &nd.body, -1, depth + 1, br, true});  // "+ add inside"
    } else if (nd.type == N_NEURO && !_cfg.readOnly) {
      out.push_back({&nd, &list, -1, depth, bracket, false, true});
    }
  }
  if (depth == 0 && !_cfg.readOnly) out.push_back({nullptr, &list, -1, 0, 0, true});  // "+ add here"
}

// Static, host-agnostic: render a program body in the editor's row style (read-only).
int ProgramEditor::drawReadOnlyList(LGFX& g, const NodeList& list, int x, int y,
                                    int depth, uint16_t bracket, int rowH) {
  for (const Node& n : list) {
    int gx = x + 18 + depth * 12;
    if (depth > 0) {
      int bx = x + 4 + (depth - 1) * 12;
      g.drawFastVLine(bx, y + 1, rowH - 2, bracket);
      g.drawFastHLine(bx, y + rowH / 2, 4, bracket);
    }
    char lab[20]; Glyph gl = Glyph::PLAY; uint16_t col = C_INK;
    nodeLabel(n, lab, sizeof(lab), gl, col);
    drawGlyph(g, gl, x + 6 + depth * 12, y + rowH / 2 - 1, 11, col);
    label(g, gx, y + 2, lab, col);
    y += rowH;
    if (!n.body.empty()) y = drawReadOnlyList(g, n.body, x, y, depth + 1, bracketColorFor(n), rowH);
  }
  return y;
}

NodeList* ProgramEditor::appendTarget() {
  if (_selected >= 0) {
    std::vector<Row> rows;
    flatten(*_editList, 0, rows);
    if (_selected < (int)rows.size()) {
      Row& r = rows[_selected];
      if (r.addSlot) {
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

void ProgramEditor::appendNodeToTarget(const Node& n) {
  appendTarget()->push_back(n);
  _selected = -1;
  _failNode = nullptr;
  _followTail = true;
  hal::audio.blip();
  drawProgramList();
}

void ProgramEditor::appendCommand(Cmd c) {
  if (!_editList) return;
  if (_profile) {
    if (c == CMD_FWD) _profile->stats.commandsUsed[CS_FWD]++;
    else if (c == CMD_JUMP) _profile->stats.commandsUsed[CS_JUMP]++;
    else _profile->stats.commandsUsed[CS_TURN]++;
  }
  appendNodeToTarget(Node::command(c));
}

void ProgramEditor::drawProgramList() {
  auto& g = hal::display.gfx();
  g.fillRect(LIST_X, BAND_Y, LIST_W, SCREEN_H - BAND_Y, C_PANEL);
  const char* body = (_editList == &_prog->f1) ? "F1" : (_editList == &_prog->f2) ? "F2" : "MAIN";
  char hdr[28];
  _writtenCount = programWrittenCount(*_prog);
  snprintf(hdr, sizeof(hdr), "%s  %d/%d", body, _writtenCount, _par);
  label(g, LIST_X + 6, BAND_Y + 5, hdr, C_INK);

  std::vector<Row> rows;
  flatten(*_editList, 0, rows);
  int n = (int)rows.size();
  if (_selected >= n) _selected = -1;

  std::vector<std::string> rownum(n);
  {
    int ctr[8] = {0};
    std::string seg[8];
    for (int i = 0; i < n; i++) {
      if (rows[i].addSlot || rows[i].trainSlot) continue;
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

  if (_cfg.func) {
    const char* tabs[3] = {"MAIN", "F1", "F2"};
    NodeList* lists[3] = {&_prog->main, &_prog->f1, &_prog->f2};
    for (int i = 0; i < 3; i++) {
      Rect r = funcTabRect(i);
      bool on = (_editList == lists[i]);
      button(g, r, tabs[i], on ? C_ACCENT : C_DIM, on ? C_PANEL_HI : C_PANEL);
    }
    if (_cfg.library) {  // library save/load is host-opt-in (e.g. off during a timed race)
      button(g, funcTabRect(3), "S>L", C_GO, C_PANEL);
      button(g, funcTabRect(4), "L<L", C_FUNC, C_PANEL);
    }
  }

  int top = listTop();
  int visible = (SCREEN_H - top) / ROW_H;
  if (_followTail && n > visible) _scroll = n - visible;
  if (_selected >= 0 && _selected < _scroll) _scroll = _selected;
  if (_selected >= _scroll + visible) _scroll = _selected - visible + 1;
  if (_scroll > n - visible) _scroll = (n > visible) ? n - visible : 0;
  if (_scroll < 0) _scroll = 0;

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
    if (row.depth > 0) {
      int bx = LIST_X + 22 + (row.depth - 1) * 12;
      g.drawFastVLine(bx, y + 2, ROW_H - 4, row.bracket);
      g.drawFastHLine(bx, y + ROW_H / 2, 4, row.bracket);
    }
    int gx = LIST_X + 24 + row.depth * 12;

    if (row.trainSlot) {
      Rect tr = {(int16_t)(gx + 2), (int16_t)(y + 1), 120, (int16_t)(ROW_H - 3)};
      button(g, tr, "train brain >", ui::rgb(120, 230, 245), C_PANEL_HI);
      continue;
    }
    if (row.addSlot) {
      const char* txt = !row.node ? (sel ? "+ adding here"   : "+ add here")
                                  : (sel ? "+ adding inside" : "+ add inside");
      label(g, gx + 4, y + 6, txt, sel ? C_GO : C_DIM);
      continue;
    }

    label(g, LIST_X + 2, y + 6, rownum[ri].c_str(), C_DIM);
    char lab[20]; Glyph gl = Glyph::PLAY; uint16_t col = C_INK;
    nodeLabel(*row.node, lab, sizeof(lab), gl, col);
    drawGlyph(g, gl, gx + 7, y + ROW_H / 2 - 1, 12, col);

    Node* nd = row.node;
    if (sel && nd->type == N_REPEAT) {
      char rl[14]; snprintf(rl, sizeof(rl), "repeat %d", nd->count);
      button(g, repCycleRect(y), rl, C_LOOP, C_PANEL_HI);
    } else if (sel && (nd->type == N_IF || nd->type == N_REPEAT_UNTIL)) {
      label(g, gx + 18, y + 6, nd->type == N_IF ? "if" : "until", C_SENSE);
      button(g, condRect(y), condName(nd->cond), C_SENSE, C_PANEL_HI);
    } else if (sel && nd->type == N_CALL) {
      char cl[8]; snprintf(cl, sizeof(cl), "F%d", nd->func);
      label(g, gx + 18, y + 6, "call", C_FUNC);
      button(g, callCycleRect(y), cl, C_FUNC, C_PANEL_HI);
    } else if (sel && nd->type == N_NEURO) {
      const char* mode = nd->rnn ? (nd->pilot ? "rnn+pilot" : "rnn")
                                 : (nd->pilot ? "pilot" : "plain");
      button(g, callCycleRect(y), mode, ui::rgb(120, 230, 245), C_PANEL_HI);
    } else {
      label(g, gx + 18, y + 6, lab, C_INK);
    }
  }
  if (n == 0)
    label(g, LIST_X + 8, top + 8, "tap pad to add", C_DIM);
}

void ProgramEditor::handlePadTap(int x, int y) {
  if (padCell(0, 1).contains(x, y)) { appendCommand(CMD_FWD); return; }
  if (padCell(1, 0).contains(x, y)) { appendCommand(CMD_TURN_L); return; }
  if (padCell(1, 2).contains(x, y)) { appendCommand(CMD_TURN_R); return; }
  if (padCell(2, 1).contains(x, y)) {  // the brain slot
    if (_cfg.neuro) {
      uint32_t seed = (_profile ? _profile->seedBase : 7u) + (uint32_t)_prog->brains.size() * 101u + 1u;
      uint8_t idx = _prog->addBrain(seed);
      Node loop = Node::repeatUntil(AT_GOAL);
      loop.body.push_back(Node::neuro(idx));
      appendNodeToTarget(loop);
    }
    return;
  }
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

ProgramEditor::Action ProgramEditor::handleListTap(int x, int y) {
  if (_cfg.func) {
    NodeList* lists[3] = {&_prog->main, &_prog->f1, &_prog->f2};
    for (int i = 0; i < 3; i++) {
      if (funcTabRect(i).contains(x, y)) {
        _editList = lists[i]; _selected = -1; _scroll = 0; hal::audio.blip();
        drawProgramList(); return Action::NONE;
      }
    }
    if (_cfg.library && funcTabRect(3).contains(x, y)) { hal::audio.blip(); return Action::SAVE_LIB; }
    if (_cfg.library && funcTabRect(4).contains(x, y)) { hal::audio.blip(); return Action::LOAD_LIB; }
  }
  if (x < LIST_X) return Action::NONE;
  int top = listTop();
  int visible = (SCREEN_H - top) / ROW_H;
  std::vector<Row> rows;
  flatten(*_editList, 0, rows);
  int n = (int)rows.size();

  if (x >= LIST_X + LIST_W - 9 && n > visible) {
    _followTail = false;
    if (y < top + visible * ROW_H / 2) _scroll -= visible - 1;
    else _scroll += visible - 1;
    if (_scroll < 0) _scroll = 0;
    if (_scroll > n - visible) _scroll = n - visible;
    drawProgramList();
    return Action::NONE;
  }
  for (int vi = 0; vi < visible; vi++) {
    int yy = top + vi * ROW_H;
    if (y >= yy && y < yy + ROW_H) {
      int ri = _scroll + vi;
      if (ri >= n) { drawProgramList(); return Action::NONE; }
      if (rows[ri].trainSlot) { _neuroIdx = rows[ri].node->brainIdx; hal::audio.blip(); return Action::NEURO_TRAIN; }
      if (ri == _selected && !rows[ri].addSlot) {
        Node* sn = rows[ri].node;
        if (sn->type == N_REPEAT) {
          if (repCycleRect(yy).contains(x, y)) {
            sn->count = (sn->count >= 5) ? 2 : sn->count + 1;
            hal::audio.blip(); drawProgramList(); return Action::NONE;
          }
        } else if (sn->type == N_IF || sn->type == N_REPEAT_UNTIL) {
          if (condRect(yy).contains(x, y)) {
            // wall -> pit -> goal -> enemy -> near -> wall. The two enemy senses are
            // arena conditions, but we expose them in maze mode too so a kid can write
            // and test their battle-bot's logic here (they simply read false with no foe).
            sn->cond = (sn->cond == WALL_AHEAD) ? PIT_AHEAD
                     : (sn->cond == PIT_AHEAD) ? BLOCKED_AHEAD
                     : (sn->cond == BLOCKED_AHEAD) ? AT_GOAL
                     : (sn->cond == AT_GOAL) ? ENEMY_AHEAD
                     : (sn->cond == ENEMY_AHEAD) ? ENEMY_NEAR : WALL_AHEAD;
            hal::audio.blip(); drawProgramList(); return Action::NONE;
          }
        } else if (sn->type == N_CALL) {
          if (callCycleRect(yy).contains(x, y)) {
            sn->func = (sn->func == 1) ? 2 : 1;
            hal::audio.blip(); drawProgramList(); return Action::NONE;
          }
        } else if (sn->type == N_NEURO) {
          if (callCycleRect(yy).contains(x, y)) {  // cycle brain mode: plain -> pilot -> rnn -> rnn+pilot
            if (!sn->pilot && !sn->rnn)      sn->pilot = true;
            else if (sn->pilot && !sn->rnn)  { sn->pilot = false; sn->rnn = true; }
            else if (!sn->pilot && sn->rnn)  sn->pilot = true;
            else                             { sn->pilot = false; sn->rnn = false; }
            hal::audio.blip(); drawProgramList(); return Action::NONE;
          }
        }
      }
      _selected = (ri == _selected) ? -1 : ri; _followTail = false; hal::audio.blip();
      drawProgramList();
      drawControlPad();
      return Action::NONE;
    }
  }
  return Action::NONE;
}

void ProgramEditor::deleteSelected() {
  std::vector<Row> rows;
  flatten(*_editList, 0, rows);
  if (_selected < 0 || _selected >= (int)rows.size()) return;
  Row& r = rows[_selected];
  if (r.addSlot) return;
  r.list->erase(r.list->begin() + r.index);
  _selected = -1;
  _failNode = nullptr;
  hal::audio.blip();
  drawProgramList();
}

void ProgramEditor::moveSelected(int dir) {
  std::vector<Row> rows;
  flatten(*_editList, 0, rows);
  if (_selected < 0 || _selected >= (int)rows.size()) return;
  Row& r = rows[_selected];
  if (r.addSlot || r.trainSlot) return;
  NodeList* lst = r.list;
  int i = r.index, j = i + dir;
  if (j < 0 || j >= (int)lst->size()) return;
  std::swap((*lst)[i], (*lst)[j]);            // moves the whole block (its body rides along)
  _failNode = nullptr;
  std::vector<Row> rows2;
  flatten(*_editList, 0, rows2);
  for (int k = 0; k < (int)rows2.size(); k++)
    if (rows2[k].list == lst && rows2[k].index == j && !rows2[k].addSlot && !rows2[k].trainSlot) { _selected = k; break; }
  hal::audio.blip();
  drawProgramList(); drawControlPad();
}

ProgramEditor::Action ProgramEditor::handleTap(int x, int y) {
  if (_cfg.readOnly) return Action::NONE;
  if (R_CLEAR.contains(x, y)) { _editList->clear(); _selected = -1; _failNode = nullptr; hal::audio.blip(); drawProgramList(); drawControlPad(); return Action::NONE; }
  if (R_DELPAD.contains(x, y)) { if (_selected >= 0) { deleteSelected(); drawControlPad(); } return Action::NONE; }
  if (R_UP.contains(x, y)) { if (_selected >= 0) moveSelected(-1); return Action::NONE; }
  if (R_DN.contains(x, y)) { if (_selected >= 0) moveSelected(+1); return Action::NONE; }
  if (R_RUNPAD.contains(x, y)) return Action::RUN;
  if (x < LIST_X) { handlePadTap(x, y); return Action::NONE; }
  return handleListTap(x, y);
}

}  // namespace screens
