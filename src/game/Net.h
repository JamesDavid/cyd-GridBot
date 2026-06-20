// GridBot / NeuroBot — the brain: a tiny fixed-capacity MLP (inputs -> hidden tanh ->
// sigmoid outputs) with backprop. This is the engine behind lessons 2-4 and the NEURO
// block: argmax(outputs) picks the action. Fixed arrays (no heap) so it runs on the
// no-PSRAM ESP32 and is host-unit-tested. Seeded init => reproducible brains/battles.
#pragma once
#include <stdint.h>

namespace gb {

constexpr int NET_MAX_IN  = 12;
constexpr int NET_MAX_HID = 12;
constexpr int NET_MAX_OUT = 6;

struct Net {
  int nIn = 2, nHid = 4, nOut = 1;
  float w1[NET_MAX_HID][NET_MAX_IN] = {{0}};  // hidden weights
  float b1[NET_MAX_HID] = {0};
  float w2[NET_MAX_OUT][NET_MAX_HID] = {{0}};  // output weights
  float b2[NET_MAX_OUT] = {0};
  float lr = 0.3f;

  void config(int in, int hid, int out, uint32_t seed);
  void reset(uint32_t seed);                 // small random weights

  // Forward pass. Fills out[0..nOut) with sigmoid activations; hid (optional, size
  // >= nHid) gets the hidden activations for visualization.
  void forward(const float* x, float* out, float* hid = nullptr) const;
  int  argmax(const float* x) const;         // index of the strongest output (the action)

  // One backprop step on a single example (target is one-hot or in [0,1] per output).
  // Returns the mean squared error BEFORE the update (so a caller can plot loss).
  float trainStep(const float* x, const float* target);
  // One pass over n examples (X is n*nIn row-major, Y is n*nOut row-major). Mean MSE.
  float trainEpoch(const float* X, const float* Y, int n);
};

}  // namespace gb
