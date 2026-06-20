// NeuroBot lesson 1 — "watch it learn". A single neuron (2 sensor inputs: wall, pit)
// learns the rule "TURN if wall OR pit, else GO" by backprop. The kid taps Train and
// watches the error fall, the weight edges thicken, and each example flip from ✗ to ✓.
// This is the make-or-break test of whether backprop *clicks* before we scale to a
// full maze brain (see docs/NEUROBOT_IDEA.md).
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"
#include "game/Perceptron.h"

namespace screens {

class NeuroLessonScreen : public app::IScreen {
 public:
  void begin();
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;

 private:
  void draw();
  void trainEpochs(int n);
  float lossNow() const;

  gb::Perceptron _p;
  int _epochs = 0;
  float _loss = 1.0f;
  app::TapDetector _tap;
};

}  // namespace screens
