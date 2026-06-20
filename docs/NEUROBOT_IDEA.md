# NeuroBot — a neural-network–teaching sibling to GridBot (design notes)

> Status: **idea / discussion only.** No code yet. Captured 2026-06-19 so the design
> isn't lost. GridBot teaches *imperative programming*; NeuroBot would teach *machine
> learning* on the same CYD hardware and reuse most of the engine.

## The one-line framing
**GridBot = *you write the rules*. NeuroBot = *you grow / train the rules.***
That symbolic-vs-learned-AI contrast is itself the headline lesson, and the two could
share one app (and battle each other in the Arena).

## Is the CYD up to it?
Comfortably — the constraint is pedagogy/visualisation, not compute.
- A brain of **7 inputs → 6 hidden (tanh) → 4 outputs ≈ 70 weights ≈ 280 bytes**.
  Inference is microseconds.
- **Neuroevolution** of a 24-genome population over ~20 generations ≈ tens of thousands
  of tiny forward passes — sub-second; can be animated live.
- Population of 50 genomes ≈ 20 KB RAM. No PSRAM, no TensorFlow.
- Hand-roll a tiny float MLP for **transparency** (TFLite-Micro would be an opaque
  black box and overkill). Keep RNG **seeded** → reproducible runs / replays / battles,
  matching GridBot's determinism ethos.

## How does the learner "program" a brain? (three on-ramps)
1. **Hand-wire weights** — sliders on a tiny net; watch behaviour change. One lesson
   ("a neuron is a weighted sum that fires"), not the whole game.
2. **Learn-by-demonstration (supervised)** — the kid *drives* the robot through a few
   mazes; each `(sensors → action)` pair is a training example; train the brain to
   imitate, then drop it in an **unseen** maze. Teaches supervised learning + the
   train/test (generalisation) idea.
3. **Neuroevolution (selection + mutation)** — a herd of random brains runs the maze;
   the ones that get furthest breed with mutation; repeat. Watch a swarm go from
   spastic to competent.

**Recommendation:** neuroevolution is the **heart** (most visual, no math wall, ties to
the Arena). Demonstration is a strong **second mode**. Skip reward-based RL for v1 —
finicky and slow to converge, which reads as "nothing's happening."

## MVP — the neuroevolution vertical slice
**Pitch:** "Watch random bots learn to reach the battery," and see *why* by watching the
brain light up.

- **Inputs (7):** wall ahead / left / right, pit ahead, goal direction `sign(Δrow)`,
  `sign(Δcol)`, normalised distance.
- **Hidden:** 6 neurons, `tanh`.
- **Outputs (4):** forward / turn-L / turn-R / jump → **argmax** each tick. (Same action
  space as GridBot, fed into the same maze tick engine.)
- **Fitness** (run ~60 ticks, then score):
  ```
  fitness = (start_distance − end_distance)   // progress toward goal (uses BFS distanceToGoal)
          + 50 if reached the battery
          − 0.1 × steps_taken                 // efficiency nudge
          − 5 if it bonked / fell
  ```
  Closeness-to-goal as the gradient is what makes evolution work without backprop.
- **GA loop:** population 24 → score all → select top 6 → breed 24 (copy a parent, add
  Gaussian noise per weight at mutation rate ≈ 0.1, occasional crossover, **elitism**
  keeps the best 1–2). A solver should emerge in **10–20 generations**.

## What's on screen (the visualisation *is* the product)
Three toggleable views:
1. **The swarm** — all 24 bots running on the maze at once, best highlighted ("natural
   selection" feel).
2. **The brain** — nodes + edges; inputs light up with the current senses, edge colour =
   weight sign, thickness = magnitude, the winning output flashes each tick. **Watch it
   think.** Non-negotiable — without it, it's just "numbers go up."
3. **The progress curve** — best/average fitness per generation, climbing.

Kid-tunable knobs: population size, mutation rate, **which sensors are enabled** (turn
off the goal-direction sensor and watch it get much harder → "sensors matter"), and
generations-per-tap.

## Reuse from GridBot (~70%)
Maze generation, sensors, the action tick, dirty-rect rendering, the Arena, and
especially **ESP-NOW trade/battle** — a trained brain is a few hundred floats, so it
slots straight into the existing Pokémon-style trade. **Trade and battle evolved
brains** with a friend is the killer feature; your evolved champion vs theirs.

## Suggested lesson arc (if it grows past the sandbox)
1. One neuron, manual weights — "it adds inputs and fires."
2. Hand-wire a reflex (a wall-follower *as weights*) — bridges from the rule game.
3. Demonstration — drive, then let it imitate you.
4. Evolution — random herd → breed the best to reach the battery.
5. Generalisation — train on some mazes, test on unseen ones (overfitting = "it
   memorised the maze instead of learning to solve mazes").
6. Sensor design / mutation rate as tunable knobs.

## Honest risks
- **Abstraction** — "adjust weights" is opaque for younger kids; evolution and
  demonstration are the graspable on-ramps; raw weight-tweaking/backprop are for older
  learners. Gate by age/mode.
- **Convergence luck** — evolution can stall on a bad seed; curate the MVP maze and
  defaults so progress shows in the first ~10 generations or it reads as broken.
- **Black-box-ness** — invest in the brain-activation view or it's "magic happens."
- **Scope** — a sibling product / major mode, not a small feature (but big engine reuse).

## Open decisions (for the user)
- **(a)** Which learning mechanic is the heart? (recommendation: evolution.)
- **(b)** Target age / how much math to expose.
- **(c)** Standalone app vs a **"Brain" mode** inside GridBot sharing the Arena.
  (recommendation: a Brain mode inside GridBot.)
- **(d)** Single "watch it learn" sandbox vs a full lesson campaign.
  (recommendation: sandbox MVP first, grow into a campaign once the core moment lands.)
