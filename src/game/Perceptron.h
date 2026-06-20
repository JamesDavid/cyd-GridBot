// GridBot / NeuroBot — a single neuron (perceptron) with sigmoid activation and
// gradient-descent (backprop) training. This is the ATOM of the "watch it learn"
// lesson: inputs (sensors) -> weighted sum -> sigmoid -> a 0/1 decision such as
// "go vs turn". Deliberately tiny and legible (it's teaching material), and
// Arduino-free so the math is host-unit-tested.
#pragma once
#include <stdint.h>

namespace gb {

constexpr int NN_MAX_IN = 8;

struct Perceptron {
  int nIn = 1;
  float w[NN_MAX_IN] = {0};
  float b = 0.0f;
  float lr = 0.5f;            // learning rate — "how big a step downhill"

  // Small deterministic pseudo-random init (no <random>, so it runs on-device too).
  void reset(uint32_t seed);

  // Forward pass: sigmoid(w·x + b), in (0,1). >0.5 reads as the "fire"/decision.
  float forward(const float* x) const;

  // One gradient-descent (backprop) step on a single example.
  // Returns the SQUARED ERROR *before* the update (so a caller can plot loss falling).
  float trainStep(const float* x, float target);

  // One pass over n examples: X is n*nIn row-major, y is n targets in {0,1}.
  // Returns the mean squared error over the pass.
  float trainEpoch(const float* X, const float* y, int n);
};

}  // namespace gb
