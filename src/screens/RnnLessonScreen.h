// NeuroLab lesson — Memory (a recurrent net). Pilot gave the brain an OUTSIDE planner.
// Here the brain grows its OWN memory: a feedforward brain (action = senses now) loops in a
// dead-end because it forgets it's been there; an RNN (action = senses now + memory) backs
// out and solves it. Both learn from the same teacher; only the RNN can hold the state.
// (engines: Net / RNet / Reactive)
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"
#include "game/Maze.h"
#include "game/Net.h"
#include "game/RNet.h"

namespace screens {

class RnnLessonScreen : public app::IScreen {
 public:
  void begin();
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;

 private:
  void draw();
  void mazeGeom(int& tile, int& ox, int& oy) const;
  void trace(bool rnn);     // run the chosen brain, record its path + whether it won

  gb::Maze _maze;
  gb::Net  _ff;             // no memory
  gb::RNet _rnn;            // has memory
  int _phase = 0;           // 0 feedforward (stuck), 1 RNN (solves), 2 the explanation
  uint8_t _path[104];   // trimmed from 120 to free static DRAM for the long-press menu; lesson
  int _pathLen = 0;     // mazes are tiny so the route never approaches this (fill is size-bounded)
  bool _won = false;
  app::TapDetector _tap;
};

}  // namespace screens
