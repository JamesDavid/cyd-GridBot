#include "game/Net.h"
#include <math.h>

namespace gb {

static inline float sigmoidf(float z) { return 1.0f / (1.0f + expf(-z)); }
static inline uint32_t lcg(uint32_t& s) { s = s * 1664525u + 1013904223u; return s; }
static inline float rnd(uint32_t& s) { return (int)((lcg(s) >> 8) % 1000) / 1000.0f - 0.5f; }

void Net::config(int in, int hid, int out, uint32_t seed) {
  nIn = in < NET_MAX_IN ? in : NET_MAX_IN;
  nHid = hid < NET_MAX_HID ? hid : NET_MAX_HID;
  nOut = out < NET_MAX_OUT ? out : NET_MAX_OUT;
  reset(seed);
}

void Net::reset(uint32_t seed) {
  uint32_t s = seed ? seed : 1u;
  for (int j = 0; j < NET_MAX_HID; j++) {
    for (int i = 0; i < NET_MAX_IN; i++) w1[j][i] = rnd(s);
    b1[j] = rnd(s);
  }
  for (int k = 0; k < NET_MAX_OUT; k++) {
    for (int j = 0; j < NET_MAX_HID; j++) w2[k][j] = rnd(s);
    b2[k] = rnd(s);
  }
}

void Net::forward(const float* x, float* out, float* hid) const {
  float h[NET_MAX_HID];
  for (int j = 0; j < nHid; j++) {
    float z = b1[j];
    for (int i = 0; i < nIn; i++) z += w1[j][i] * x[i];
    h[j] = tanhf(z);
    if (hid) hid[j] = h[j];
  }
  for (int k = 0; k < nOut; k++) {
    float z = b2[k];
    for (int j = 0; j < nHid; j++) z += w2[k][j] * h[j];
    out[k] = sigmoidf(z);
  }
}

int Net::argmax(const float* x) const {
  float out[NET_MAX_OUT];
  forward(x, out);
  int best = 0;
  for (int k = 1; k < nOut; k++) if (out[k] > out[best]) best = k;
  return best;
}

float Net::trainStep(const float* x, const float* target) {
  float h[NET_MAX_HID], out[NET_MAX_OUT];
  forward(x, out, h);

  // output deltas (MSE * sigmoid'), and the loss before the update
  float d2[NET_MAX_OUT], loss = 0.0f;
  for (int k = 0; k < nOut; k++) {
    float err = out[k] - target[k];
    loss += err * err;
    d2[k] = err * out[k] * (1.0f - out[k]);
  }
  // hidden deltas (backprop through w2, then tanh'). Use w2 BEFORE updating it.
  float d1[NET_MAX_HID];
  for (int j = 0; j < nHid; j++) {
    float s = 0.0f;
    for (int k = 0; k < nOut; k++) s += d2[k] * w2[k][j];
    d1[j] = s * (1.0f - h[j] * h[j]);
  }
  // updates
  for (int k = 0; k < nOut; k++) {
    for (int j = 0; j < nHid; j++) w2[k][j] -= lr * d2[k] * h[j];
    b2[k] -= lr * d2[k];
  }
  for (int j = 0; j < nHid; j++) {
    for (int i = 0; i < nIn; i++) w1[j][i] -= lr * d1[j] * x[i];
    b1[j] -= lr * d1[j];
  }
  return loss / nOut;
}

float Net::trainEpoch(const float* X, const float* Y, int n) {
  float sum = 0.0f;
  for (int i = 0; i < n; i++) sum += trainStep(&X[i * nIn], &Y[i * nOut]);
  return n > 0 ? sum / n : 0.0f;
}

}  // namespace gb
