#include "screens/StatsScreen.h"
#include "assets/Assets.h"

using namespace ui;
using namespace gb;

namespace screens {

static const Rect R_BACK = {6, (int16_t)(BOTBAR_Y + 2), 92, 26};
static const Rect R_EDIT = {102, (int16_t)(BOTBAR_Y + 2), 104, 26};
static const Rect R_DRAW = {210, (int16_t)(BOTBAR_Y + 2), 104, 26};
static const char* CMD_NAMES[CS_COUNT] = {"Fwd","Back","Turn","Jump","Loop","Func","Sense"};

void StatsScreen::enter() { draw(); }

void StatsScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  if (!_p) { label(g, 6, 4, "Stats", C_ACCENT); return; }
  const Stats& s = _p->stats;

  char buf[48];
  snprintf(buf, sizeof(buf), "%s - Stats", _p->name.c_str());
  label(g, 6, 3, buf, C_ACCENT, textdatum_t::top_left, 2);
  assets::drawCharacter(g, SCREEN_W - 16, 11, 16, _p->avatar, gb::SOUTH);

  int y = BAND_Y + 4;
  auto line = [&](const char* k, const char* v, uint16_t col) {
    label(g, 10, y, k, C_DIM);
    label(g, 150, y, v, col);
    y += 18;
  };
  snprintf(buf, sizeof(buf), "%u", (unsigned)_p->level);            line("Level reached", buf, C_INK);
  snprintf(buf, sizeof(buf), "%u", (unsigned)s.starsTotal);        line("Total stars", buf, C_ACCENT);
  int rate = s.totalRuns ? (int)(100u * s.totalWins / s.totalRuns) : 0;
  snprintf(buf, sizeof(buf), "%d%%  (%u/%u)", rate, (unsigned)s.totalWins, (unsigned)s.totalRuns);
  line("Win rate", buf, C_GO);
  snprintf(buf, sizeof(buf), "%u levels", (unsigned)s.levelsCompleted); line("Completed", buf, C_INK);
  snprintf(buf, sizeof(buf), "%u bonks / %u falls", (unsigned)s.bonks, (unsigned)s.falls);
  line("Oopsies", buf, C_BAD);
  snprintf(buf, sizeof(buf), "%u", (unsigned)s.currentStreak);     line("Streak", buf, C_INK);

  // command histogram bar chart
  label(g, 10, y + 2, "Commands used", C_DIM);
  y += 20;
  uint16_t maxv = 1;
  for (int i = 0; i < CS_COUNT; i++) if (s.commandsUsed[i] > maxv) maxv = s.commandsUsed[i];
  int bx = 60, bw = 200;
  for (int i = 0; i < CS_COUNT; i++) {
    int yy = y + i * 11;
    label(g, 10, yy, CMD_NAMES[i], C_DIM);
    int w = (int)((uint32_t)s.commandsUsed[i] * bw / maxv);
    g.fillRect(bx, yy, bw, 8, C_PANEL);
    g.fillRect(bx, yy, w, 8, C_MOVE);
  }

  button(g, R_BACK, "< Back", C_INK, C_PANEL);
  button(g, R_EDIT, "Edit name", C_ACCENT, C_PANEL);
  button(g, R_DRAW, "Draw sprite", C_FUNC, C_PANEL);
}

app::Signal StatsScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (_tap.tapped(tp, now, tx, ty)) {
    if (R_BACK.contains(tx, ty)) return app::Signal::BACK;
    if (R_EDIT.contains(tx, ty)) return app::Signal::EDIT_PROFILE;
    if (R_DRAW.contains(tx, ty)) return app::Signal::GOTO_DRAW;
  }
  return app::Signal::NONE;
}

}  // namespace screens
