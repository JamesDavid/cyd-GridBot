// NeuroBot backprop lessons — "watch it learn". Three modes, each small enough to fully
// visualize (see docs/NEUROBOT_IDEA.md):
//   0  one neuron, binary  (turn if wall OR pit)      -> a Perceptron, no hidden layer
//   1  multi-class         (go / turn / jump, argmax) -> a Net, 3 outputs
//   2  hidden layer (XOR)  (a line can't split it)    -> a Net, proves the hidden layer
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"
#include "game/Perceptron.h"
#include "game/Net.h"

namespace screens {

class NeuroLessonScreen : public app::IScreen {
 public:
  void begin(int mode = 0);
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;

 private:
  void draw();
  void trainEpochs(int n);
  float lossNow() const;
  int decision(int i) const;  // the model's chosen class for situation i

  int _mode = 0;
  uint32_t _seed = 7;  // init seed; Reset varies it (escape an XOR local minimum)
  gb::Perceptron _p;   // mode 0
  gb::Net _net;        // modes 1, 2
  int _epochs = 0;
  float _loss = 1.0f;
  app::TapDetector _tap;
};

}  // namespace screens
