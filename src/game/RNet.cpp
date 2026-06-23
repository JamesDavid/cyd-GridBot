#include "game/RNet.h"
#include <math.h>

namespace gb {

// Same cheap LCG-ish seeded RNG style as Net, for reproducible brains.
static inline float frand(uint32_t& s) {
  s = s * 1664525u + 1013904223u;
  return ((s >> 8) & 0xFFFF) / 32768.0f - 1.0f;   // ~[-1,1]
}

void RNet::config(int in, int hid, int out, uint32_t seed) {
  nIn = in; nHid = hid; nOut = out;
  uint32_t s = seed ? seed : 1u;
  for (int j = 0; j < nHid; j++) {
    for (int i = 0; i < nIn; i++)  wih[j][i] = frand(s) * 0.5f;
    for (int k = 0; k < nHid; k++) whh[j][k] = frand(s) * 0.3f;
    bh[j] = 0.0f;
  }
  for (int m = 0; m < nOut; m++) {
    for (int j = 0; j < nHid; j++) who[m][j] = frand(s) * 0.5f;
    bo[m] = 0.0f;
  }
  resetState();
}

void RNet::resetState() const { for (int j = 0; j < nHid; j++) h[j] = 0.0f; }

static inline float sigmoidf(float x) { return 1.0f / (1.0f + expf(-x)); }

// Shared BPTT scratch (both trainEpisode and trainEpisodeQ use these; never run concurrently, so
// one set keeps static DRAM down on the ESP32). H[t+1] = hidden after step t; O = outputs.
static float H[RNET_MAX_T + 1][RNET_MAX_HID];
static float O[RNET_MAX_T][RNET_MAX_OUT];
static float gWih[RNET_MAX_HID][RNET_MAX_IN], gWhh[RNET_MAX_HID][RNET_MAX_HID];
static float gbh[RNET_MAX_HID], gWho[RNET_MAX_OUT][RNET_MAX_HID], gbo[RNET_MAX_OUT];

void RNet::step(const float* x, float* out) const {
  float nh[RNET_MAX_HID];
  for (int j = 0; j < nHid; j++) {
    float z = bh[j];
    for (int i = 0; i < nIn; i++)  z += wih[j][i] * x[i];
    for (int k = 0; k < nHid; k++) z += whh[j][k] * h[k];
    nh[j] = tanhf(z);
  }
  for (int j = 0; j < nHid; j++) h[j] = nh[j];
  for (int m = 0; m < nOut; m++) {
    float z = bo[m];
    for (int j = 0; j < nHid; j++) z += who[m][j] * h[j];
    out[m] = sigmoidf(z);
  }
}

int RNet::argmaxStep(const float* x) const {
  float out[RNET_MAX_OUT];
  step(x, out);
  int best = 0;
  for (int m = 1; m < nOut; m++) if (out[m] > out[best]) best = m;
  return best;
}

float RNet::trainEpisode(const float* X, const int* act, int T) {
  trained = true;
  if (T <= 0) return 0.0f;
  if (T > RNET_MAX_T) T = RNET_MAX_T;

  // ---- forward pass, storing per-timestep hidden states (h[-1] = 0); shared scratch ----
  for (int j = 0; j < nHid; j++) H[0][j] = 0.0f;
  float loss = 0.0f;
  for (int t = 0; t < T; t++) {
    const float* x = X + t * nIn;
    for (int j = 0; j < nHid; j++) {
      float z = bh[j];
      for (int i = 0; i < nIn; i++)  z += wih[j][i] * x[i];
      for (int k = 0; k < nHid; k++) z += whh[j][k] * H[t][k];
      H[t + 1][j] = tanhf(z);
    }
    for (int m = 0; m < nOut; m++) {
      float z = bo[m];
      for (int j = 0; j < nHid; j++) z += who[m][j] * H[t + 1][j];
      O[t][m] = sigmoidf(z);
      float tgt = (m == act[t]) ? 1.0f : 0.0f;
      loss += (O[t][m] - tgt) * (O[t][m] - tgt);
    }
  }

  // ---- gradient accumulators (shared scratch) ----
  for (int j = 0; j < nHid; j++) {
    for (int i = 0; i < nIn; i++)  gWih[j][i] = 0.0f;
    for (int k = 0; k < nHid; k++) gWhh[j][k] = 0.0f;
    gbh[j] = 0.0f;
  }
  for (int m = 0; m < nOut; m++) { for (int j = 0; j < nHid; j++) gWho[m][j] = 0.0f; gbo[m] = 0.0f; }

  // ---- backward through time ----
  float dhNext[RNET_MAX_HID] = {0};
  for (int t = T - 1; t >= 0; t--) {
    const float* x = X + t * nIn;
    float dh[RNET_MAX_HID];
    for (int j = 0; j < nHid; j++) dh[j] = dhNext[j];
    for (int m = 0; m < nOut; m++) {
      float tgt = (m == act[t]) ? 1.0f : 0.0f;
      float dz = (O[t][m] - tgt) * O[t][m] * (1.0f - O[t][m]);   // sigmoid + MSE
      gbo[m] += dz;
      for (int j = 0; j < nHid; j++) { gWho[m][j] += dz * H[t + 1][j]; dh[j] += who[m][j] * dz; }
    }
    float dz[RNET_MAX_HID];
    for (int j = 0; j < nHid; j++) dz[j] = dh[j] * (1.0f - H[t + 1][j] * H[t + 1][j]);  // tanh'
    for (int j = 0; j < nHid; j++) {
      gbh[j] += dz[j];
      for (int i = 0; i < nIn; i++)  gWih[j][i] += dz[j] * x[i];
      for (int k = 0; k < nHid; k++) gWhh[j][k] += dz[j] * H[t][k];
    }
    for (int k = 0; k < nHid; k++) {           // gradient flowing to the previous hidden state
      float s = 0.0f;
      for (int j = 0; j < nHid; j++) s += whh[j][k] * dz[j];
      dhNext[k] = s;
    }
  }

  // ---- update (clip to keep BPTT stable) ----
  const float clip = 2.0f;
  auto upd = [&](float& w, float g) {
    if (g > clip) g = clip; else if (g < -clip) g = -clip;
    w -= lr * g;
  };
  for (int j = 0; j < nHid; j++) {
    for (int i = 0; i < nIn; i++)  upd(wih[j][i], gWih[j][i]);
    for (int k = 0; k < nHid; k++) upd(whh[j][k], gWhh[j][k]);
    upd(bh[j], gbh[j]);
  }
  for (int m = 0; m < nOut; m++) { for (int j = 0; j < nHid; j++) upd(who[m][j], gWho[m][j]); upd(bo[m], gbo[m]); }
  return loss / T;
}

float RNet::trainEpisodeQ(const float* X, const int* act, const float* qt, int T) {
  trained = true;
  if (T <= 0) return 0.0f;
  if (T > RNET_MAX_T) T = RNET_MAX_T;

  for (int j = 0; j < nHid; j++) H[0][j] = 0.0f;   // shared BPTT scratch
  float loss = 0.0f;
  for (int t = 0; t < T; t++) {
    const float* x = X + t * nIn;
    for (int j = 0; j < nHid; j++) {
      float z = bh[j];
      for (int i = 0; i < nIn; i++)  z += wih[j][i] * x[i];
      for (int k = 0; k < nHid; k++) z += whh[j][k] * H[t][k];
      H[t + 1][j] = tanhf(z);
    }
    for (int m = 0; m < nOut; m++) {
      float z = bo[m];
      for (int j = 0; j < nHid; j++) z += who[m][j] * H[t + 1][j];
      O[t][m] = sigmoidf(z);
    }
    float e = O[t][act[t]] - qt[t];
    loss += e * e;
  }

  for (int j = 0; j < nHid; j++) {
    for (int i = 0; i < nIn; i++)  gWih[j][i] = 0.0f;
    for (int k = 0; k < nHid; k++) gWhh[j][k] = 0.0f;
    gbh[j] = 0.0f;
  }
  for (int m = 0; m < nOut; m++) { for (int j = 0; j < nHid; j++) gWho[m][j] = 0.0f; gbo[m] = 0.0f; }

  float dhNext[RNET_MAX_HID] = {0};
  for (int t = T - 1; t >= 0; t--) {
    const float* x = X + t * nIn;
    float dh[RNET_MAX_HID];
    for (int j = 0; j < nHid; j++) dh[j] = dhNext[j];
    for (int m = 0; m < nOut; m++) {
      // ONLY the taken action carries error (toward its TD target); the rest are untouched
      float err = (m == act[t]) ? (O[t][m] - qt[t]) : 0.0f;
      float dz = err * O[t][m] * (1.0f - O[t][m]);
      gbo[m] += dz;
      for (int j = 0; j < nHid; j++) { gWho[m][j] += dz * H[t + 1][j]; dh[j] += who[m][j] * dz; }
    }
    float dz[RNET_MAX_HID];
    for (int j = 0; j < nHid; j++) dz[j] = dh[j] * (1.0f - H[t + 1][j] * H[t + 1][j]);
    for (int j = 0; j < nHid; j++) {
      gbh[j] += dz[j];
      for (int i = 0; i < nIn; i++)  gWih[j][i] += dz[j] * x[i];
      for (int k = 0; k < nHid; k++) gWhh[j][k] += dz[j] * H[t][k];
    }
    for (int k = 0; k < nHid; k++) {
      float s = 0.0f;
      for (int j = 0; j < nHid; j++) s += whh[j][k] * dz[j];
      dhNext[k] = s;
    }
  }

  const float clip = 2.0f;
  auto upd = [&](float& w, float g) {
    if (g > clip) g = clip; else if (g < -clip) g = -clip;
    w -= lr * g;
  };
  for (int j = 0; j < nHid; j++) {
    for (int i = 0; i < nIn; i++)  upd(wih[j][i], gWih[j][i]);
    for (int k = 0; k < nHid; k++) upd(whh[j][k], gWhh[j][k]);
    upd(bh[j], gbh[j]);
  }
  for (int m = 0; m < nOut; m++) { for (int j = 0; j < nHid; j++) upd(who[m][j], gWho[m][j]); upd(bo[m], gbo[m]); }
  return loss / T;
}

}  // namespace gb
