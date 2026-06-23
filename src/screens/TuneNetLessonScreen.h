// NeuroLab lesson 11 — TUNE A REAL BRAIN. The Tuning lesson turned knobs on a tabular Q grid;
// this turns the same kind of knobs on a real neural net doing BACKPROP. The net learns the
// "turn at a corner" task (the Hidden-layer lesson's job) while the kid watches the LOSS curve:
// learning rate too low -> it barely moves (crawls); too high -> the loss bounces (thrashes);
// just right -> a smooth fall to zero. Rounds = how long it trains. Pairs with the Arena
// trainer's "Knobs" panel, where the same dials tune a real fighter. (engine: Net + backprop)
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"
#include "game/Net.h"

namespace screens {

class TuneNetLessonScreen : public app::IScreen {
 public:
  void begin();
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;

 private:
  void draw();
  void reset();            // fresh random brain with the current knobs (so runs compare fairly)
  void pushLoss(float l);

  gb::Net _net;
  int   _lrIdx = 4;        // learn-rate knob: index into a preset ladder (default 0.50)
  float lr() const;        // the current learn rate (ladder[_lrIdx])
  int   _rounds = 2;       // mini-batches per "Train" press (training length)
  uint32_t _seed = 3;      // varied on Reset so each fresh start differs
  static constexpr int LOSS_N = 40;
  float _loss[LOSS_N] = {0};
  int   _lossLen = 0;
  float _last = 1.0f;      // most recent loss
  bool  _thrash = false;   // loss rose after training (lr too high)
  app::TapDetector _tap;
};

}  // namespace screens
