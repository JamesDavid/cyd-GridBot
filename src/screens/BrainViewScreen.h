// NeuroLab lesson — "Brain Cam": watch a trained brain THINK. As it steps through a maze
// you see its inputs (what it senses), the hidden layer (what it computes), and the
// outputs (what it decides) light up live — sense -> think -> act. (engines: Net/Distill)
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"
#include "game/Maze.h"
#include "game/Net.h"
#include "game/Interpreter.h"

namespace screens {

class BrainViewScreen : public app::IScreen {
 public:
  void begin();
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;

 private:
  void draw();
  void computeActivations();

  gb::Maze _maze;
  gb::Net _brain;
  gb::Program _prog;
  gb::Interpreter _it;
  float _in[gb::SENSOR_COUNT_FOR_BRAIN] = {0};
  float _hid[gb::NET_MAX_HID] = {0};
  float _out[gb::NET_MAX_OUT] = {0};
  int _action = 0;
  bool _running = false;
  uint32_t _last = 0;
  app::TapDetector _tap;
};

}  // namespace screens
