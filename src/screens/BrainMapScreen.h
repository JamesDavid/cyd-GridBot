// NeuroLab lesson — "Meet your robot's brain": a guided tour of the REAL NeuroBot
// brain. Three pages name every sense (walls / pit / goal-dir / enemy-dir), every
// action (fwd / turn / jump / zap), and explain that one 10->8->5 brain drives both
// mazes and arena battles. Static/illustrative — no training. (engine: Net shape)
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"

namespace screens {

class BrainMapScreen : public app::IScreen {
 public:
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;

 private:
  void draw();
  void drawSchematic(int highlight);  // 0 = inputs lit, 1 = outputs lit, 2 = whole net
  int _page = 0;
  app::TapDetector _tap;
};

}  // namespace screens
