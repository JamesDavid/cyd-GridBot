#include "screens/ProfileCreateScreen.h"
#include "assets/Assets.h"
#include <string.h>

using namespace ui;

namespace screens {

static constexpr int GB_NAME_MAX = 8;
// QWERTY layout. 28 keys: 26 letters in QWERTY order, then DEL, OK.
// Row lengths: 10 (QWERTYUIOP), 9 (ASDFGHJKL), 9 (ZXCVBNM + DEL + OK).
static const char* KB_CHARS = "QWERTYUIOPASDFGHJKLZXCVBNM";
static constexpr int KEY_W = 31, KEY_H = 30, KB_Y = 82, ROW_GAP = 32;
static constexpr int DEL_IDX = 26, OK_IDX = 27;

static int rowOf(int i) { return i < 10 ? 0 : (i < 19 ? 1 : 2); }
static int colOf(int i) { return i < 10 ? i : (i < 19 ? i - 10 : i - 19); }
static int countOf(int row) { return row == 0 ? 10 : 9; }

void ProfileCreateScreen::begin() { _name.clear(); _avatar = 0; _edit = false; }

void ProfileCreateScreen::beginEdit(const std::string& name, uint8_t avatar) {
  _name = name; _avatar = avatar; _edit = true;
}

ui::Rect ProfileCreateScreen::keyRect(int i) const {
  int row = rowOf(i), col = colOf(i), cnt = countOf(row);
  int rowW = cnt * KEY_W;
  int x0 = (SCREEN_W - rowW) / 2;
  return {(int16_t)(x0 + col * KEY_W), (int16_t)(KB_Y + row * ROW_GAP),
          (int16_t)(KEY_W - 2), (int16_t)(KEY_H - 2)};
}

ui::Rect ProfileCreateScreen::avatarRect(int i) const {
  int w = SCREEN_W / assets::ROSTER_SIZE;
  return {(int16_t)(i * w), 46, (int16_t)(w - 2), 30};
}

void ProfileCreateScreen::enter() { draw(); }

void ProfileCreateScreen::drawNameField() {
  auto& g = hal::display.gfx();
  g.fillRect(0, BAND_Y, SCREEN_W, 22, C_BG);
  g.fillRoundRect(60, BAND_Y, 200, 20, 4, C_PANEL);
  std::string shown = _name.empty() ? "(type a name)" : _name;
  label(g, 70, BAND_Y + 4, shown.c_str(), _name.empty() ? C_DIM : C_INK);
}

void ProfileCreateScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, SCREEN_W / 2, 3, _edit ? "Edit Player" : "New Player", C_ACCENT, textdatum_t::top_center, 2);
  button(g, {4, 1, 60, 20}, "Cancel", C_INK, C_PANEL);   // bail without creating

  drawNameField();

  // avatar picker
  for (int i = 0; i < assets::ROSTER_SIZE; i++) {
    Rect r = avatarRect(i);
    bool sel = (i == _avatar);
    g.fillRoundRect(r.x, r.y, r.w, r.h, 4, sel ? C_PANEL_HI : C_PANEL);
    if (sel) g.drawRoundRect(r.x, r.y, r.w, r.h, 4, C_ACCENT);
    assets::drawCharacter(g, r.cx(), r.cy(), 24, i, gb::SOUTH);
  }

  // QWERTY keyboard
  for (int i = 0; i < 28; i++) {
    Rect r = keyRect(i);
    char lab[4];
    if (i == DEL_IDX) strcpy(lab, "DEL");
    else if (i == OK_IDX) strcpy(lab, "OK");
    else { lab[0] = KB_CHARS[i]; lab[1] = 0; }
    uint16_t fg = (i == OK_IDX) ? C_GO : (i == DEL_IDX ? C_BAD : C_INK);
    button(g, r, lab, fg, C_PANEL);
  }
}

app::Signal ProfileCreateScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;

  if (ui::Rect{4, 1, 60, 20}.contains(tx, ty)) return app::Signal::BACK;  // Cancel

  for (int i = 0; i < assets::ROSTER_SIZE; i++) {
    if (avatarRect(i).contains(tx, ty)) { _avatar = i; draw(); return app::Signal::NONE; }
  }
  for (int i = 0; i < 28; i++) {
    if (!keyRect(i).contains(tx, ty)) continue;
    if (i == OK_IDX) {
      if (!_name.empty()) return app::Signal::CREATED;
      return app::Signal::NONE;
    }
    if (i == DEL_IDX) {
      if (!_name.empty()) _name.pop_back();
    } else if ((int)_name.size() < GB_NAME_MAX) {
      _name.push_back(KB_CHARS[i]);
    }
    drawNameField();
    return app::Signal::NONE;
  }
  return app::Signal::NONE;
}

}  // namespace screens
