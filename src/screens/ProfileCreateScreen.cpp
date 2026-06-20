#include "screens/ProfileCreateScreen.h"
#include "assets/Assets.h"

using namespace ui;

namespace screens {

static constexpr int GB_NAME_MAX = 8;
static constexpr int KB_COLS = 7, KEY_W = 44, KEY_H = 30, KB_X = 6, KB_Y = 80;
static const char* KEY_LABELS[28] = {
  "A","B","C","D","E","F","G",
  "H","I","J","K","L","M","N",
  "O","P","Q","R","S","T","U",
  "V","W","X","Y","Z","DEL","OK"};

void ProfileCreateScreen::begin() { _name.clear(); _avatar = 0; _edit = false; }

void ProfileCreateScreen::beginEdit(const std::string& name, uint8_t avatar) {
  _name = name; _avatar = avatar; _edit = true;
}

ui::Rect ProfileCreateScreen::keyRect(int i) const {
  int r = i / KB_COLS, c = i % KB_COLS;
  return {(int16_t)(KB_X + c * KEY_W), (int16_t)(KB_Y + r * KEY_H),
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

  drawNameField();

  // avatar picker
  for (int i = 0; i < assets::ROSTER_SIZE; i++) {
    Rect r = avatarRect(i);
    bool sel = (i == _avatar);
    g.fillRoundRect(r.x, r.y, r.w, r.h, 4, sel ? C_PANEL_HI : C_PANEL);
    if (sel) g.drawRoundRect(r.x, r.y, r.w, r.h, 4, C_ACCENT);
    assets::drawCharacter(g, r.cx(), r.cy(), 24, i, gb::SOUTH);
  }

  // keyboard
  for (int i = 0; i < 28; i++) {
    Rect r = keyRect(i);
    uint16_t fg = (i == 27) ? C_GO : (i == 26 ? C_BAD : C_INK);
    button(g, r, KEY_LABELS[i], fg, C_PANEL);
  }
}

app::Signal ProfileCreateScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;

  for (int i = 0; i < assets::ROSTER_SIZE; i++) {
    if (avatarRect(i).contains(tx, ty)) { _avatar = i; draw(); return app::Signal::NONE; }
  }
  for (int i = 0; i < 28; i++) {
    if (!keyRect(i).contains(tx, ty)) continue;
    if (i == 27) {  // OK
      if (!_name.empty()) return app::Signal::CREATED;
      return app::Signal::NONE;
    }
    if (i == 26) {  // DEL
      if (!_name.empty()) _name.pop_back();
    } else if ((int)_name.size() < GB_NAME_MAX) {
      _name.push_back((char)('A' + i));
    }
    drawNameField();
    return app::Signal::NONE;
  }
  return app::Signal::NONE;
}

}  // namespace screens
