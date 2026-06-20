#include "game/Perceptron.h"
#include <math.h>

namespace gb {

static inline float sigmoidf(float z) { return 1.0f / (1.0f + expf(-z)); }

// A tiny LCG so weight init is deterministic and dependency-free (no <random>).
static inline uint32_t lcg(uint32_t& s) { s = s * 1664525u + 1013904223u; return s; }

void Perceptron::reset(uint32_t seed) {
  uint32_t s = seed ? seed : 1u;
  for (int i = 0; i < NN_MAX_IN; i++)
    w[i] = (int)((lcg(s) >> 8) % 1000) / 1000.0f - 0.5f;  // small weights ~[-0.5,0.5]
  b = (int)((lcg(s) >> 8) % 1000) / 1000.0f - 0.5f;
}

float Perceptron::forward(const float* x) const {
  float z = b;
  for (int i = 0; i < nIn; i++) z += w[i] * x[i];
  return sigmoidf(z);
}

float Perceptron::trainStep(const float* x, float target) {
  float pred = forward(x);
  float err = pred - target;               // d(MSE)/d(pred) = pred - target
  float dz = err * pred * (1.0f - pred);   // chain through the sigmoid derivative
  for (int i = 0; i < nIn; i++) w[i] -= lr * dz * x[i];  // step each weight downhill
  b -= lr * dz;
  return err * err;                        // squared error before this step
}

float Perceptron::trainEpoch(const float* X, const float* y, int n) {
  float sum = 0.0f;
  for (int i = 0; i < n; i++) sum += trainStep(&X[i * nIn], y[i]);
  return n > 0 ? sum / n : 0.0f;
}

}  // namespace gb
