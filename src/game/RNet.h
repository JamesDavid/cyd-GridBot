// GridBot / NeuroBot — a tiny RECURRENT net (Elman RNN): the brain gets a memory.
// Unlike the feedforward Net (action = f(senses now)), the RNN keeps a hidden STATE that
// carries across steps (action = f(senses now, what I remember)). That memory is what lets
// a brain escape dead-ends on its own — no planner needed. Trained by backprop-through-time
// over a whole maze run. Fixed arrays (no heap), host-unit-tested.
#pragma once
#include <stdint.h>

namespace gb {

constexpr int RNET_MAX_IN  = 12;
constexpr int RNET_MAX_HID = 12;   // brains use 8; 12 is headroom (keeps static DRAM down)
constexpr int RNET_MAX_OUT = 5;
constexpr int RNET_MAX_T   = 56;   // longest episode we backprop through (bounds DRAM)

struct RNet {
  int nIn = 10, nHid = 12, nOut = 5;
  float wih[RNET_MAX_HID][RNET_MAX_IN] = {{0}};   // input -> hidden
  float whh[RNET_MAX_HID][RNET_MAX_HID] = {{0}};  // hidden(t-1) -> hidden(t)  (the memory)
  float bh[RNET_MAX_HID] = {0};
  float who[RNET_MAX_OUT][RNET_MAX_HID] = {{0}};  // hidden -> output
  float bo[RNET_MAX_OUT] = {0};
  mutable float h[RNET_MAX_HID] = {0};             // current hidden state (transient run memory)
  float lr = 0.1f;
  bool trained = false;                            // has BPTT run? (untrained = don't use/save)

  void config(int in, int hid, int out, uint32_t seed);
  void resetState() const;                         // zero the memory (call at run start)
  void step(const float* x, float* out) const;     // forward one tick; advances the memory
  int  argmaxStep(const float* x) const;           // step() + index of the strongest output

  // Backprop-through-time over one episode: X is T*nIn (row-major), act[t] is the target
  // action index at step t. Trains all weights; resets/uses a fresh memory internally.
  float trainEpisode(const float* X, const int* act, int T);
};

}  // namespace gb
