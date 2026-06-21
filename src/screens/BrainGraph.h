// Shared "network graph" drawing used by Brain Cam AND the code trainer's brain view:
// inputs -> hidden -> outputs, with the connection lines coloured by WEIGHT (so they
// recolour live as backprop runs), nodes coloured by ACTIVATION, the rnn feedback matrix
// (every hidden -> every hidden + self-loops), and a tappable neuron for weight zoom.
#pragma once
#include "hal/Display.h"
#include "game/Program.h"   // Net, RNet, SENSOR_COUNT_FOR_BRAIN

namespace screens {

// Draw the graph for a brain (ff or rnn). in/hid/out are the current activations; action is
// the argmax output (ringed); sel is -1 or layer*100+idx (layer 1=hidden, 2=output) to zoom.
void drawBrainGraph(LGFX& g, const gb::Net* ff, const gb::RNet* rnn, bool useRnn,
                    const float* in, const float* hid, const float* out, int action, int sel);

// Hit-test a hidden/output neuron at (x,y). Returns true + fills layer(1/2)+idx.
bool brainGraphNodeAt(int x, int y, int nHid, int& layer, int& idx);

// A node's incoming weights / bias, for the zoom strip (branches on the brain type).
float brainWIn(const gb::Net* ff, const gb::RNet* rnn, bool useRnn, int i, int j);
float brainWOut(const gb::Net* ff, const gb::RNet* rnn, bool useRnn, int j, int k);
float brainBHid(const gb::Net* ff, const gb::RNet* rnn, bool useRnn, int j);
float brainBOut(const gb::Net* ff, const gb::RNet* rnn, bool useRnn, int k);

extern const char* const BRAIN_INLBL[gb::SENSOR_COUNT_FOR_BRAIN];
extern const char* const BRAIN_OUTLBL[5];

}  // namespace screens
