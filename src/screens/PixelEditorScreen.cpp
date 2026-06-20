#include "screens/PixelEditorScreen.h"
#include "hal/Audio.h"
#include "assets/Assets.h"
#include <math.h>

using namespace ui;

namespace screens {

static constexpr int GX = 6, GY = BAND_Y, CELL = 11, N = gb::PIX_DIM;  // 16*11=176
static constexpr int GW = N * CELL;
// right panel
static const Rect R_CHAR  = {186, 24, 62, 20};
static const Rect R_GOAL  = {250, 24, 62, 20};
static Rect swatchRect(int i) { return {(int16_t)(186 + (i % 4) * 32), (int16_t)(48 + (i / 4) * 24), 28, 20}; }
static const Rect R_ERASE  = {186, 100, 60, 22};
static const Rect R_FILL   = {250, 100, 60, 22};
static const Rect R_MIRROR = {186, 126, 124, 22};
static const Rect R_BOOM   = {186, 152, 60, 26};
static const Rect R_SAVE   = {250, 152, 60, 26};
static const Rect R_BACK   = {6, (int16_t)(BOTBAR_Y + 2), 120, 26};

void PixelEditorScreen::begin(gb::Profile* profile) {
  _profile = profile;
  if (profile->customChar.size() != (size_t)gb::PIX_CELLS) profile->customChar.assign(gb::PIX_CELLS, 0);
  if (profile->customGoal.size() != (size_t)gb::PIX_CELLS) profile->customGoal.assign(gb::PIX_CELLS, 0);
  selectTarget(false);
}

void PixelEditorScreen::selectTarget(bool goal) {
  _editingGoal = goal;
  _target = goal ? &_profile->customGoal : &_profile->customChar;
}

void PixelEditorScreen::enter() { draw(); }

void PixelEditorScreen::drawCellAt(int r, int c) {
  auto& g = hal::display.gfx();
  int x = GX + c * CELL, y = GY + r * CELL;
  uint8_t idx = px(r, c);
  uint16_t col = (idx == 0) ? (((r + c) & 1) ? C_PANEL : C_PANEL_HI)
                            : assets::PIXEL_PALETTE[idx];
  g.fillRect(x, y, CELL - 1, CELL - 1, col);
}

void PixelEditorScreen::drawGridAll() {
  for (int r = 0; r < N; r++)
    for (int c = 0; c < N; c++) drawCellAt(r, c);
}

void PixelEditorScreen::drawPalette() {
  auto& g = hal::display.gfx();
  for (int i = 0; i < 8; i++) {
    Rect s = swatchRect(i);
    g.fillRoundRect(s.x, s.y, s.w, s.h, 3, assets::PIXEL_PALETTE[i + 1]);
    if (_tool == Tool::PENCIL && _color == i + 1)
      g.drawRoundRect(s.x - 1, s.y - 1, s.w + 2, s.h + 2, 4, C_INK);
  }
}

void PixelEditorScreen::drawTools() {
  auto& g = hal::display.gfx();
  button(g, R_ERASE, "ERASE", _tool == Tool::ERASER ? C_BG : C_INK,
         _tool == Tool::ERASER ? C_INK : C_PANEL);
  button(g, R_FILL, "FILL", _tool == Tool::FILL ? C_BG : C_INK,
         _tool == Tool::FILL ? C_INK : C_PANEL);
  button(g, R_MIRROR, "MIRROR", C_INK, C_PANEL);
  button(g, R_BOOM, "BOOM", C_BAD, C_PANEL);
  button(g, R_SAVE, "SAVE", C_GO, C_PANEL);
}

void PixelEditorScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Draw your own!", C_ACCENT, textdatum_t::top_left, 2);
  // CHAR | GOAL toggle
  button(g, R_CHAR, "Character", _editingGoal ? C_DIM : C_ACCENT, _editingGoal ? C_PANEL : C_PANEL_HI);
  button(g, R_GOAL, "Goal", _editingGoal ? C_ACCENT : C_DIM, _editingGoal ? C_PANEL_HI : C_PANEL);
  drawGridAll();
  drawPalette();
  drawTools();
  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_BACK, "< Save & Back", C_INK, C_PANEL);
}

void PixelEditorScreen::paintCell(int r, int c) {
  if (r < 0 || r >= N || c < 0 || c >= N) return;
  uint8_t want = (_tool == Tool::ERASER) ? 0 : _color;
  if (px(r, c) == want) return;
  px(r, c) = want;
  drawCellAt(r, c);
}

void PixelEditorScreen::floodFill(int r, int c, uint8_t from, uint8_t to) {
  if (from == to) return;
  // iterative stack flood (bounded 256 cells)
  static int stack[gb::PIX_CELLS];
  int sp = 0;
  stack[sp++] = r * N + c;
  while (sp > 0) {
    int idx = stack[--sp];
    int rr = idx / N, cc = idx % N;
    if (rr < 0 || rr >= N || cc < 0 || cc >= N) continue;
    if (px(rr, cc) != from) continue;
    px(rr, cc) = to;
    stack[sp++] = (rr + 1) * N + cc;
    stack[sp++] = (rr - 1) * N + cc;
    stack[sp++] = rr * N + (cc + 1);
    stack[sp++] = rr * N + (cc - 1);
  }
  drawGridAll();
}

void PixelEditorScreen::mirror() {
  for (int r = 0; r < N; r++)
    for (int c = 0; c < N / 2; c++)
      px(r, N - 1 - c) = px(r, c);
  drawGridAll();
  hal::audio.blip();
}

void PixelEditorScreen::boom() {
  auto& g = hal::display.gfx();
  int cx = GX + GW / 2, cy = GY + GW / 2;
  hal::audio.fail();  // explosion-ish
  for (int rad = 4; rad < GW; rad += 10) {
    uint16_t col = (rad / 10 % 2) ? C_ACCENT : C_BAD;
    g.drawCircle(cx, cy, rad, col);
    g.drawCircle(cx, cy, rad - 3, C_GO);
    delay(18);
  }
  for (int i = 0; i < gb::PIX_CELLS; i++) (*_target)[i] = 0;
  drawGridAll();
}

app::Signal PixelEditorScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  bool tap = _tap.tapped(tp, now, tx, ty);

  // continuous painting while held inside the grid (pencil/eraser drag)
  if (tp.pressed && _tool != Tool::FILL) {
    int c = (tp.x - GX) / CELL, r = (tp.y - GY) / CELL;
    if (tp.x >= GX && tp.x < GX + GW && tp.y >= GY && tp.y < GY + GW)
      paintCell(r, c);
  }

  if (!tap) return app::Signal::NONE;

  // grid tap with FILL tool
  if (_tool == Tool::FILL && tx >= GX && tx < GX + GW && ty >= GY && ty < GY + GW) {
    int c = (tx - GX) / CELL, r = (ty - GY) / CELL;
    floodFill(r, c, px(r, c), _color);
    return app::Signal::NONE;
  }
  // palette
  for (int i = 0; i < 8; i++) {
    if (swatchRect(i).contains(tx, ty)) {
      _color = i + 1; _tool = Tool::PENCIL; hal::audio.blip();
      drawPalette(); drawTools(); return app::Signal::NONE;
    }
  }
  if (R_CHAR.contains(tx, ty)) { selectTarget(false); draw(); return app::Signal::NONE; }
  if (R_GOAL.contains(tx, ty)) { selectTarget(true); draw(); return app::Signal::NONE; }
  if (R_ERASE.contains(tx, ty)) { _tool = Tool::ERASER; drawTools(); drawPalette(); return app::Signal::NONE; }
  if (R_FILL.contains(tx, ty)) { _tool = Tool::FILL; drawTools(); return app::Signal::NONE; }
  if (R_MIRROR.contains(tx, ty)) { mirror(); return app::Signal::NONE; }
  if (R_BOOM.contains(tx, ty)) { boom(); return app::Signal::NONE; }
  if (R_SAVE.contains(tx, ty)) { hal::audio.win(); return app::Signal::BACK; }
  if (R_BACK.contains(tx, ty)) return app::Signal::BACK;
  return app::Signal::NONE;
}

}  // namespace screens
