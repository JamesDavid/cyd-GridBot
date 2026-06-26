#include "screens/HomeScreen.h"
#include "hal/Audio.h"
#include "game/Achievements.h"

using namespace ui;
using namespace gb;

namespace screens {

// 2x3 tile grid + a footer.
static Rect tileRect(int i) {
  int col = i % 2, row = i / 2;
  return {(int16_t)(8 + col * 156), (int16_t)(44 + row * 52), 148, 48};
}
static const Rect R_PLAYERS = {6,   (int16_t)(BOTBAR_Y + 2), 96, 26};
static const Rect R_MYBOTS  = {106, (int16_t)(BOTBAR_Y + 2), 104, 26};
static const Rect R_STATS   = {214, (int16_t)(BOTBAR_Y + 2), 100, 26};

void HomeScreen::enter() { draw(); }

void HomeScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "GridBot", C_ACCENT, textdatum_t::top_left, 2);
  if (_p) {
    char hdr[40];
    int badges = achievementCount(_p->achievements);
    snprintf(hdr, sizeof(hdr), "%s  Lv %u  *%u  %d/%d", _p->name.c_str(),
             (unsigned)_p->level, (unsigned)_p->stats.starsTotal, badges, ACH_COUNT);
    label(g, SCREEN_W - 6 - SOUND_ICON_W, 6, hdr, C_DIM, textdatum_t::top_right);
  }

  const bool arenaOpen = _p && _p->unlocks.sense;   // Arena unlocks with sensing (Lv 15)
  char playSub[16] = "start playing";
  char badgeSub[16] = "collect them";
  char shopSub[16] = "spend coins";
  if (_p) {
    snprintf(playSub, sizeof(playSub), "level %u", (unsigned)_p->level);
    snprintf(badgeSub, sizeof(badgeSub), "%d / %d", achievementCount(_p->achievements), ACH_COUNT);
    snprintf(shopSub, sizeof(shopSub), "%u coins", (unsigned)_p->coins);
  }
  struct Tile { const char* title; const char* sub; uint16_t col; bool locked; };
  const Tile T[6] = {
    {"\x10 Play", playSub, C_GO, false},
    {"Arena", arenaOpen ? "battle bots" : "unlocks Lv 15", C_BAD, !arenaOpen},
    {"Learn", "code + AI", ui::rgb(120, 230, 245), false},
    {"Customize", "draw your bot", C_FUNC, false},
    {"Badges", badgeSub, C_ACCENT, false},
    {"Shop", shopSub, ui::rgb(255, 210, 60), false},
  };
  for (int i = 0; i < 6; i++) {
    Rect r = tileRect(i);
    uint16_t fg = T[i].locked ? C_DIM : T[i].col;
    g.fillRoundRect(r.x, r.y, r.w, r.h, 7, T[i].locked ? C_LOCK : C_PANEL);
    g.drawRoundRect(r.x, r.y, r.w, r.h, 7, fg);
    label(g, r.x + 12, r.y + 8, T[i].title, fg, textdatum_t::top_left, 2);
    label(g, r.x + 12, r.y + 30, T[i].sub, C_DIM);
  }

  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_PLAYERS, "< Players", C_INK, C_PANEL);
  button(g, R_MYBOTS, "My Bots", ui::rgb(120, 230, 245), C_PANEL);
  button(g, R_STATS, "Stats >", C_INK, C_PANEL);
}

app::Signal HomeScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  const bool arenaOpen = _p && _p->unlocks.sense;
  if (tileRect(0).contains(tx, ty)) { hal::audio.blip(); return app::Signal::GOTO_PLAY; }
  if (tileRect(1).contains(tx, ty)) {
    if (arenaOpen) { hal::audio.blip(); return app::Signal::GOTO_ARENA; }
    hal::audio.fail();  // locked
  }
  if (tileRect(2).contains(tx, ty)) { hal::audio.blip(); return app::Signal::GOTO_LEARN; }
  if (tileRect(3).contains(tx, ty)) { hal::audio.blip(); return app::Signal::GOTO_DRAW; }
  if (tileRect(4).contains(tx, ty)) { hal::audio.blip(); return app::Signal::GOTO_BADGES; }
  if (tileRect(5).contains(tx, ty)) { hal::audio.blip(); return app::Signal::GOTO_SHOP; }
  if (R_PLAYERS.contains(tx, ty)) return app::Signal::BACK;        // -> profile select
  if (R_MYBOTS.contains(tx, ty)) { hal::audio.blip(); return app::Signal::GOTO_MYBOTS; }
  if (R_STATS.contains(tx, ty)) return app::Signal::GOTO_STATS;
  return app::Signal::NONE;
}

}  // namespace screens
