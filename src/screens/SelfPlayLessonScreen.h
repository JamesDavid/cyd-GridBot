// NeuroBot lesson — SELF-PLAY. No teacher, no fixed foe: the brain trains by fighting a copy of
// ITSELF. Each "round" a little population evolves against the current CHAMP; if the new best beats
// the champ, it takes the crown. Only improvements stick, so the champ keeps climbing -- the idea
// behind AlphaGo/AlphaZero. Exercise: rack up upgrades. (engine: Evolve vs self)
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"
#include "game/Maze.h"
#include "game/Net.h"
#include "game/Evolve.h"

namespace screens {

class SelfPlayLessonScreen : public app::IScreen {
 public:
  void begin();
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;

 private:
  void draw();
  bool beats(const gb::Net& a, const gb::Net& b);  // does A out-fight B over a few rings?

  gb::Evolve _evo;
  gb::Maze _maze;
  gb::Pose _s0, _s1;
  gb::Net _champ;       // the reigning champion (what challengers must beat)
  int _round = 0;       // challenges run
  int _upgrades = 0;    // times a challenger dethroned the champ (the champ improved)
  bool _lastUp = false; // did the last round produce an upgrade?
  app::TapDetector _tap;
};

}  // namespace screens
