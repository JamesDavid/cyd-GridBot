#include "screens/StatsScreen.h"
#include "assets/Assets.h"
#include "game/Achievements.h"

using namespace ui;
using namespace gb;

namespace screens {

static const Rect R_BACK = {6,   (int16_t)(BOTBAR_Y + 2), 64, 26};
static const Rect R_EDIT = {74,  (int16_t)(BOTBAR_Y + 2), 72, 26};
static const Rect R_DRAW = {150, (int16_t)(BOTBAR_Y + 2), 72, 26};
static const Rect R_SHOP = {226, (int16_t)(BOTBAR_Y + 2), 88, 26};
static const Rect R_DEL  = {262, 2, 54, 18};                 // small, top-right
static const Rect R_CANCEL = {30, 150, 120, 34};
static const Rect R_CONF   = {170, 150, 120, 34};

void StatsScreen::enter() { _confirm = 0; draw(); }

void StatsScreen::drawConfirm() {
  auto& g = hal::display.gfx();
  g.fillRoundRect(20, 60, 280, 150, 10, C_PANEL);
  g.drawRoundRect(20, 60, 280, 150, 10, C_BAD);
  char buf[48];
  if (_confirm == 1) {
    label(g, SCREEN_W / 2, 80, "Delete this player?", C_BAD, textdatum_t::top_center, 2);
    snprintf(buf, sizeof(buf), "%s loses ALL progress.", _p ? _p->name.c_str() : "");
    label(g, SCREEN_W / 2, 112, buf, C_INK, textdatum_t::top_center);
    button(g, R_CANCEL, "Keep", C_GO, C_PANEL_HI);
    button(g, R_CONF, "Delete...", C_BAD, C_PANEL_HI);
  } else {
    label(g, SCREEN_W / 2, 80, "Are you SURE?", C_BAD, textdatum_t::top_center, 2);
    label(g, SCREEN_W / 2, 112, "This cannot be undone!", C_INK, textdatum_t::top_center);
    button(g, R_CANCEL, "No, keep", C_GO, C_PANEL_HI);
    button(g, R_CONF, "DELETE", C_BAD, C_PANEL_HI);
  }
}

void StatsScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  if (!_p) { label(g, 6, 4, "Stats", C_ACCENT); return; }
  const Stats& s = _p->stats;

  char buf[48];
  snprintf(buf, sizeof(buf), "%s - Stats", _p->name.c_str());
  label(g, 6, 3, buf, C_ACCENT, textdatum_t::top_left, 2);
  snprintf(buf, sizeof(buf), "Badges %d/%d >", gb::achievementCount(_p->achievements), gb::ACH_COUNT);
  label(g, 160, 6, buf, C_FUNC, textdatum_t::top_left);

  int y = BAND_Y + 2;
  auto line = [&](const char* k, const char* v, uint16_t col) {
    label(g, 10, y, k, C_DIM);
    label(g, 150, y, v, col);
    y += 14;
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
  // NeuroBot progress (only once the ML mode has been reached/used)
  if (_p->unlocks.neuro || s.brainsTrained || s.fightersSaved) {
    snprintf(buf, sizeof(buf), "%u trained / %u won / %u saved",
             (unsigned)s.brainsTrained, (unsigned)s.neuroWins, (unsigned)s.fightersSaved);
    line("Brains", buf, ui::rgb(120, 230, 245));
    if (s.gauntletBest > 0) {  // best Generalist-challenge run for a frozen brain
      snprintf(buf, sizeof(buf), "%u/50 levels%s", (unsigned)s.gauntletBest,
               s.gauntletBest >= 50 ? " - GENERALIST!" : "");
      line("Gauntlet", buf, s.gauntletBest >= 50 ? C_GO : C_ACCENT);
    }
  }

  // command histogram — locked commands greyed out with their unlock level
  label(g, 10, y + 1, "Commands used", C_DIM);
  y += 14;
  const CmdStat order[6] = {CS_FWD, CS_TURN, CS_JUMP, CS_REPEAT, CS_CALL, CS_SENSE};
  const char* nm[6] = {"Fwd", "Turn", "Jump", "Loop", "Func", "Sense"};
  const int lvl[6] = {1, 1, 6, 10, 20, 15};
  const gb::Unlocks& u = _p->unlocks;
  const bool unl[6] = {true, true, u.jump, u.repeat, u.func, u.sense};
  uint16_t maxv = 1;
  for (int i = 0; i < 6; i++) if (s.commandsUsed[order[i]] > maxv) maxv = s.commandsUsed[order[i]];
  int bx = 60, bw = 196;
  for (int i = 0; i < 6; i++) {
    int yy = y + i * 11;
    label(g, 10, yy, nm[i], unl[i] ? C_INK : C_LOCK);
    if (unl[i]) {
      int w = (int)((uint32_t)s.commandsUsed[order[i]] * bw / maxv);
      g.fillRect(bx, yy, bw, 8, C_PANEL);
      g.fillRect(bx, yy, w, 8, C_MOVE);
    } else {
      g.fillRect(bx, yy, bw, 8, C_PANEL);
      char lk[16]; snprintf(lk, sizeof(lk), "unlock Lv %d", lvl[i]);
      label(g, bx + 6, yy, lk, C_LOCK);
    }
  }

  button(g, R_BACK, "Back", C_INK, C_PANEL);
  button(g, R_EDIT, "Edit", C_ACCENT, C_PANEL);
  button(g, R_DRAW, "Draw", C_FUNC, C_PANEL);
  button(g, R_SHOP, "Shop", ui::rgb(255, 210, 60), C_PANEL);
  button(g, R_DEL, "delete", C_BAD, C_PANEL);  // small; guarded by 2 confirmations
  if (_confirm > 0) drawConfirm();
}

app::Signal StatsScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;

  if (_confirm > 0) {  // confirmation dialog is up
    if (R_CANCEL.contains(tx, ty)) { _confirm = 0; draw(); }
    else if (R_CONF.contains(tx, ty)) {
      if (_confirm == 1) { _confirm = 2; drawConfirm(); }
      else return app::Signal::DELETE_PROFILE;  // confirmed twice
    }
    return app::Signal::NONE;
  }

  if (R_BACK.contains(tx, ty)) return app::Signal::BACK;
  if (R_EDIT.contains(tx, ty)) return app::Signal::EDIT_PROFILE;
  if (R_DRAW.contains(tx, ty)) return app::Signal::GOTO_DRAW;
  if (R_SHOP.contains(tx, ty)) return app::Signal::GOTO_SHOP;
  if (R_DEL.contains(tx, ty)) { _confirm = 1; drawConfirm(); }
  // tapping the "Badges N/16 >" header opens the gallery
  if (tx >= 158 && tx < 256 && ty < 22) return app::Signal::GOTO_BADGES;
  return app::Signal::NONE;
}

}  // namespace screens
