#include "screens/SelfPlayLessonScreen.h"
#include "hal/Audio.h"
#include "assets/Assets.h"
#include "game/MazeGen.h"
#include "game/Arena.h"
#include "game/Program.h"
#include "game/Types.h"

using namespace ui;
using namespace gb;

namespace screens {

static const Rect R_ROUND = {6,   (int16_t)(BOTBAR_Y + 2), 150, 26};
static const Rect R_RESET = {162, (int16_t)(BOTBAR_Y + 2), 80, 26};
static const Rect R_BACK  = {250, (int16_t)(BOTBAR_Y + 2), 64, 26};
static const int GOAL_UPGRADES = 3;

// wrap a brain as a hunting fighter (loop { brain }); the brain drives via N_NEURO
static Program fighter(const Net& b) {
  Program p; uint8_t i = p.addBrain(1); p.brains[i] = b;
  Node loop = Node::repeatUntil(AT_GOAL); loop.body.push_back(Node::neuro(i)); p.main.push_back(loop);
  return p;
}

void SelfPlayLessonScreen::begin() {
  MazeGen::generateSumoRing(_maze, 1234u, _s0, _s1);
  _evo.init(SENSOR_COUNT_FOR_BRAIN, 8, 5, 7);
  _champ = _evo.best();                 // a fresh, clueless champ
  _round = 0; _upgrades = 0; _lastUp = false;
}

void SelfPlayLessonScreen::enter() { draw(); }

// Best-of-3 across distinct rings: A "wins" a ring if it fights better than B (higher fitness:
// closing in + zaps + surviving). Smooth fitness, so even clueless brains compare.
bool SelfPlayLessonScreen::beats(const Net& a, const Net& b) {
  Program pa = fighter(a), pb = fighter(b);
  int wins = 0;
  for (int r = 0; r < 3; r++) {
    Maze m; Pose s0, s1; MazeGen::generateSumoRing(m, 100u + (uint32_t)r * 31u, s0, s1);
    if (scoreFighter(a, m, s0, s1, pb, 120, MatchType::SUMO) >
        scoreFighter(b, m, s0, s1, pa, 120, MatchType::SUMO)) wins++;
  }
  return wins >= 2;
}

void SelfPlayLessonScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Self-play: beat yourself", C_ACCENT, textdatum_t::top_left, 2);
  char hd[20]; snprintf(hd, sizeof(hd), "round %d", _round);
  label(g, SCREEN_W - 6 - SOUND_ICON_W, 6, hd, C_DIM, textdatum_t::top_right);

  assets::drawCharacter(g, 96, 70, 40, 1, gb::EAST);     // champ
  assets::drawCharacter(g, 224, 70, 40, 4, gb::WEST);    // challengers (a population vs the champ)
  label(g, 96, 96, "CHAMP", C_GO, textdatum_t::middle_center);
  label(g, 224, 96, "challengers", C_MOVE, textdatum_t::middle_center);

  label(g, SCREEN_W / 2, 120, "upgrades", C_DIM, textdatum_t::middle_center);
  int pip = 16, gap = 6, total = GOAL_UPGRADES * (pip + gap) - gap, x0 = (SCREEN_W - total) / 2;
  for (int i = 0; i < GOAL_UPGRADES; i++)
    g.fillRoundRect(x0 + i * (pip + gap), 132, pip, 12, 3, i < _upgrades ? C_GO : C_LOCK);

  const char* msg;
  if (_upgrades >= GOAL_UPGRADES) msg = "Champion! it climbed by fighting itself";
  else if (_round == 0)           msg = "tap Challenge: a copy-army takes on the champ";
  else if (_lastUp)               msg = "a challenger won - new champ! it got better";
  else                            msg = "champ held on; challenge again";
  label(g, SCREEN_W / 2, 168, msg, _upgrades >= GOAL_UPGRADES ? C_GO : C_INK, textdatum_t::middle_center);
  label(g, SCREEN_W / 2, 184, "only better copies stick, so the champ keeps climbing",
        C_DIM, textdatum_t::middle_center);

  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_ROUND, "Challenge!", C_GO, C_PANEL);
  button(g, R_RESET, "Reset", C_ACCENT, C_PANEL);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
}

app::Signal SelfPlayLessonScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  if (R_ROUND.contains(tx, ty)) {
    Program champProg = fighter(_champ);              // the crown to beat (frozen this round)
    for (int gen = 0; gen < 6; gen++) {               // a few generations evolve AGAINST the champ
      _evo.evaluateArena(_maze, _s0, _s1, champProg, 120, MatchType::SUMO);
      _evo.breed();
    }
    Net challenger = _evo.best();
    _round++;
    _lastUp = beats(challenger, _champ);
    if (_lastUp) { _champ = challenger; _upgrades++; hal::audio.badge(); }  // only winners take the crown
    else hal::audio.blip();
    draw();
  } else if (R_RESET.contains(tx, ty)) {
    begin(); hal::audio.blip(); draw();
  } else if (R_BACK.contains(tx, ty)) {
    return app::Signal::BACK;
  }
  return app::Signal::NONE;
}

}  // namespace screens
