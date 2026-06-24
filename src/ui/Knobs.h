// GridBot / NeuroBot — the shared "Training knobs" overlay. Both trainers (Arena fighter
// and Neuro maze) expose the SAME advanced hyperparameters behind a "Knobs" button, so the
// stepper UI + value semantics live in one place instead of being copy-pasted per screen.
// LR + Rounds feed the backprop engines (Teach/Q-Learn); Explore feeds Evolve's mutation
// scale and Q-Learn's epsilon. Defaults are the tuned baseline a beginner never has to touch.
#pragma once
#include "ui/UI.h"

namespace ui {

struct Knobs {
  float lr      = 0.30f;   // learning rate (Teach + Q-Learn); RNN uses a damped third of it
  int   rounds  = 1;       // training-length multiplier (Evolve gens / Teach epochs / Q chunks)
  float explore = 1.0f;    // exploration: x Evolve mutation scale, x Q-Learn epsilon

  bool isDefault() const { return lr == 0.30f && rounds == 1 && explore == 1.0f; }

  void draw() const;            // paint the full-screen knobs overlay
  bool handleTap(int tx, int ty);  // step a knob; returns true when "Done" closes the overlay
};

}  // namespace ui
