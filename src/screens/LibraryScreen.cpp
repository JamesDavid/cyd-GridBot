#include "screens/LibraryScreen.h"
#include "hal/Audio.h"
#include "game/Program.h"

using namespace ui;
using namespace gb;

namespace screens {

static const int TOP = 28, ROWH = 28, VIS = 6;
static Rect rowRect(int i) { return {8, (int16_t)(TOP + i * ROWH), 304, (int16_t)(ROWH - 2)}; }
static Rect renChip(int y) { return {(int16_t)(312 - 112), (int16_t)(y + 2), 50, 22}; }
static Rect delChip(int y) { return {(int16_t)(312 - 56),  (int16_t)(y + 2), 50, 22}; }
static const Rect R_BACK = {6,   (int16_t)(BOTBAR_Y + 2), 96, 26};
static const Rect R_PREV = {150, (int16_t)(BOTBAR_Y + 2), 78, 26};
static const Rect R_NEXT = {234, (int16_t)(BOTBAR_Y + 2), 80, 26};

static const char* srcLabel(uint8_t s) {
  switch (s) {
    case LIB_CODE:     return "code";
    case LIB_NEURO:    return "trainer";
    case LIB_BRAINCAM: return "braincam";
    case LIB_ARENA:    return "arena";
    case LIB_RADIO:    return "radio";
    default:           return "saved";
  }
}

// Walk a program for its N_NEURO node so we can name the brain type.
static const Node* findNeuro(const NodeList& l) {
  for (const Node& n : l) {
    if (n.type == N_NEURO) return &n;
    const Node* r = findNeuro(n.body);
    if (r) return r;
  }
  return nullptr;
}
static const char* botType(const Program& p) {
  if (p.brains.empty()) return "code";
  const Node* nn = findNeuro(p.main);
  if (!nn) nn = findNeuro(p.f1);
  if (!nn) nn = findNeuro(p.f2);
  if (!nn) return "brain";
  if (nn->rnn) return nn->pilot ? "rnn+pilot" : "rnn (memory)";
  return nn->pilot ? "pilot" : "brain";
}

void LibraryScreen::begin(Profile* profile) { _p = profile; _sel = -1; _scroll = 0; _renameIdx = -1; }
void LibraryScreen::enter() { draw(); }

void LibraryScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "My Bots", C_ACCENT, textdatum_t::top_left, 2);
  int n = _p ? (int)_p->library.size() : 0;
  char hd[16]; snprintf(hd, sizeof(hd), "%d / %d", n, LIBRARY_MAX);
  label(g, SCREEN_W - 6, 6, hd, n >= LIBRARY_MAX ? C_BAD : C_DIM, textdatum_t::top_right);

  if (n == 0) {
    label(g, SCREEN_W / 2, 100, "No saved bots yet.", C_DIM, textdatum_t::middle_center);
    label(g, SCREEN_W / 2, 120, "Save one from the editor, a trainer, or Brain Cam.",
          C_DIM, textdatum_t::middle_center);
  }
  if (_scroll > n - VIS) _scroll = (n > VIS) ? n - VIS : 0;
  if (_scroll < 0) _scroll = 0;
  for (int r = 0; r < VIS; r++) {
    int idx = _scroll + r;
    if (idx >= n) break;
    const LibEntry& e = _p->library[idx];
    Rect rr = rowRect(r);
    bool sel = (idx == _sel);
    g.fillRoundRect(rr.x, rr.y, rr.w, rr.h, 4, sel ? C_PANEL_HI : C_PANEL);
    g.drawRoundRect(rr.x, rr.y, rr.w, rr.h, 4, sel ? C_ACCENT : C_PANEL_HI);
    label(g, rr.x + 8, rr.y + 3, e.name.c_str(), e.source == LIB_RADIO ? ui::rgb(120, 230, 245) : C_GO);
    // source (+ level) and brain type
    char sub[36];
    if (e.source == LIB_CODE && e.srcLevel) snprintf(sub, sizeof(sub), "code L%u . %s", (unsigned)e.srcLevel, botType(e.program));
    else snprintf(sub, sizeof(sub), "%s . %s", srcLabel(e.source), botType(e.program));
    label(g, rr.x + 8, rr.y + 14, sub, C_DIM);
    if (sel) {
      button(g, renChip(rr.y), "rename", C_ACCENT, C_PANEL);
      button(g, delChip(rr.y), "delete", C_BAD, C_PANEL);
    }
  }

  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
  if (n > VIS) {
    button(g, R_PREV, "< Prev", _scroll > 0 ? C_ACCENT : C_LOCK, C_PANEL);
    button(g, R_NEXT, "Next >", _scroll + VIS < n ? C_ACCENT : C_LOCK, C_PANEL);
  }
}

app::Signal LibraryScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  int n = _p ? (int)_p->library.size() : 0;

  if (R_BACK.contains(tx, ty)) return app::Signal::BACK;
  if (n > VIS && R_PREV.contains(tx, ty)) { if (_scroll > 0) { _scroll -= VIS; _sel = -1; hal::audio.blip(); draw(); } return app::Signal::NONE; }
  if (n > VIS && R_NEXT.contains(tx, ty)) { if (_scroll + VIS < n) { _scroll += VIS; _sel = -1; hal::audio.blip(); draw(); } return app::Signal::NONE; }

  for (int r = 0; r < VIS; r++) {
    int idx = _scroll + r;
    if (idx >= n) break;
    Rect rr = rowRect(r);
    if (idx == _sel) {  // acting on the selected row's chips
      if (renChip(rr.y).contains(tx, ty)) { _renameIdx = idx; hal::audio.blip(); return app::Signal::RENAME_LIB; }
      if (delChip(rr.y).contains(tx, ty)) {
        _p->library.erase(_p->library.begin() + idx);
        _sel = -1; hal::audio.badge(); draw(); return app::Signal::NONE;
      }
    }
    if (rr.contains(tx, ty)) { _sel = (idx == _sel) ? -1 : idx; hal::audio.blip(); draw(); return app::Signal::NONE; }
  }
  return app::Signal::NONE;
}

}  // namespace screens
