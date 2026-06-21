// NeuroLab — "Brain Cam": a live inspector for a robot brain. Watch it LEARN (backprop
// animated one chunk at a time, the connection lines recoloured as the weights change) and
// then THINK (step it through a maze, inputs/hidden/outputs lighting up). Works for a plain
// feedforward brain OR a recurrent (memory) RNN, on any campaign map, and you can tap a
// neuron to zoom in on its actual weights + bias as they update. (engines: Net/RNet/Distill)
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"
#include "game/Maze.h"
#include "game/Program.h"   // Net, RNet, SENSOR_COUNT_FOR_BRAIN

namespace screens {

class BrainViewScreen : public app::IScreen {
 public:
  void begin();
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;

 private:
  enum Mode { M_IDLE, M_LEARN, M_RUN };
  void draw();
  void loadMaze();          // (re)generate the demo maze + capture the solver's teacher episode
  void resetBrains();       // fresh untrained ff + rnn for the current maze
  void trainChunk();        // one animated step of backprop
  void resetRun();          // robot back to start, memory cleared
  void infer();             // compute activations for the current pose (advances rnn memory)
  void stepRun();           // apply the decision, move, re-infer
  // weight/bias accessors (branch on the active brain type)
  float wIn(int i, int j) const;    // input i -> hidden j
  float wOut(int j, int k) const;   // hidden j -> output k
  float wRec(int a, int b) const;   // hidden a -> hidden b (rnn only; 0 for ff)
  float bHid(int j) const;
  float bOut(int k) const;
  int nHid() const;
  bool nodeAt(int x, int y, int& layer, int& idx) const;  // hit-test a neuron

  gb::Maze _maze;
  gb::Program _prog;        // ff brain (brains[0]) + rnn brain (rbrains[0]) live on the heap
  gb::Net&  ff()  { return _prog.brains[0]; }
  gb::RNet& rnn() { return _prog.rbrains[0]; }
  const gb::Net&  ff()  const { return _prog.brains[0]; }
  const gb::RNet& rnn() const { return _prog.rbrains[0]; }
  bool _useRnn = false;     // which brain is on display
  int  _level = 6;          // which campaign map (cycled with the Map button)

  int   _T = 0;             // teacher-episode length (the shared buffer lives in Distill)
  int   _epoch = 0;         // training progress
  float _loss = 1.0f;

  // run state (interpreter-free so we control the rnn memory exactly)
  gb::Pose _pose;
  bool _done = false, _won = false;

  // activations for the display
  float _in[gb::SENSOR_COUNT_FOR_BRAIN] = {0};
  float _hid[gb::NET_MAX_HID] = {0};
  float _out[gb::NET_MAX_OUT] = {0};
  int   _action = 0;

  int _mode = M_IDLE;
  int _sel = -1;            // zoomed neuron: -1 none, else layer*100 + idx (layer 1=hid, 2=out)
  uint32_t _last = 0;
  app::TapDetector _tap;
};

}  // namespace screens
