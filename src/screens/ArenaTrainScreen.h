// NeuroBot — Arena Trainer: prep a brain to BATTLE. It evolves (or is taught) to beat an
// AI opponent on a real arena board; "Save" drops the trained fighter into your library so
// it shows up as an Arena opponent you can pick. (engines: Evolve / Distill)
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"
#include "game/Maze.h"
#include "game/Program.h"
#include "game/Evolve.h"
#include "game/Profile.h"

namespace screens {

class ArenaTrainScreen : public app::IScreen {
 public:
  void begin(gb::Profile* profile);
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;
  bool savedFighter() const { return _saved; }  // tapped "Save fighter"

 private:
  void draw();
  void mazeGeom(int& tile, int& ox, int& oy) const;
  void evaluateAndTrace();

  gb::Profile* _profile = nullptr;
  gb::Maze _maze;
  gb::Pose _s0, _s1;
  gb::Program _ai;       // the AI opponent (a navigator)
  gb::Evolve _evo;
  gb::Net _brain;
  uint8_t _path[64];
  int _pathLen = 0;
  bool _beatsAI = false, _taught = false, _saved = false;
  app::TapDetector _tap;
};

}  // namespace screens
