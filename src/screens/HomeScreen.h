// GridBot — the Home hub. After picking a profile you land here, not straight in a maze,
// so every mode is reachable: Play (campaign), Arena, Learn (CodeLab + NeuroLab),
// Customize (pixel editor), Badges, Shop. The game's HOME button returns here too.
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"
#include "game/Profile.h"

namespace screens {

class HomeScreen : public app::IScreen {
 public:
  void begin(gb::Profile* p) { _p = p; }
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;

 private:
  void draw();
  gb::Profile* _p = nullptr;
  app::TapDetector _tap;
};

}  // namespace screens
