# NeuroBot — teaching ML inside GridBot (design spec)

> Status: **IMPLEMENTED on the `neurobot` branch** (2026-06-19). GridBot teaches imperative
> programming; NeuroBot is a late-game **graduation unlock** that teaches machine learning
> on the same CYD hardware, reusing the maze, editor, Arena, ESP-NOW trade, and determinism.
>
> **Shipped:** egocentric sensors (`game/Sensors`), the `N_NEURO` AST node + interpreter
> execution, four ML engines — `game/Perceptron`, `game/Net` (MLP+backprop), `game/QLearn`
> (tabular Q), `game/Evolve` (neuroevolution) — the **NeuroLab** hub with 5 lessons (one
> neuron / multi-class / hidden-layer-XOR / Q-learning / evolution), the **NEURO block** in
> the editor + its **training interface** (evolve a brain, "Use it"), brain **serialization**
> (persists + trades with the program), and **Arena battle** integration. 63 host tests pass.
> Remaining/optional: demonstration & distillation trainers, policy gradients (an advanced
> peek), the data/sorting prerequisite layer, and on-two-boards radio verification.

## One-line framing
**GridBot = *you write the rules*. NeuroBot = *you grow / train the rules*.**
The contrast (symbolic vs. learned AI) is itself the headline lesson. NeuroBot is a
**mode/unlock inside GridBot**, not a separate app.

## The pedagogical spine: unlock after mastery
Kids first master explicit programming (sequencing → loops → functions → sensing → **data
& algorithms like sorting**). Only *after* they've felt the pain of hand-writing rules does
ML get motivated: *"Some problems are easy to describe but hard to write rules for — that's
when you train a brain instead."* NeuroBot unlocks late, gated like the existing
backward→jump→repeat→func→sense ladder.

### The "data & algorithms" layer (prerequisite expansion)
GridBot's world is spatial; sorting/search are about *data*. To express them, add a small
**data layer**: value-blocks the robot can **carry**, a **compare** sensor ("which is
bigger?"), and **swap/place**. Sorting becomes embodied (walk the row, compare, swap =
bubble sort). The same layer unlocks search, max-finding, counting/accumulation — a family
of algorithm levels, with sorting as the flagship. This both deepens the "real programming"
curriculum and sets up the ML motivation (explicit sort logic is fiddly → "what if you
can't write the rule? train it").
*Open: ship the data layer before NeuroBot, or unlock NEURO on the existing navigation
world first (faster path to the headline feature)?*

## The NEURO block (the core mechanic)
A new AST node type, `N_NEURO`, sitting alongside FORWARD / REPEAT / IF / CALL. The kid
drops it into a **normal program**; double-tapping opens the "neuro interface" to train
*that block's* brain (like editing a function body, but you shape weights, not commands).

```
REPEAT_UNTIL at_goal {
  IF wall_ahead { turn_right }   // explicit code for the easy part
  NEURO                          // trained brain decides the tricky part
}
```

This is **neurosymbolic programming for kids** — symbolic scaffolding + a learned reactive
policy — and teaches *where ML helps vs. where explicit logic is better*.

### Architecture & engine fit
**ONE unified brain** (not separate nav/combat brains) — it's cleaner as a product: a single
brain the kid grows that both navigates and fights. In the campaign there are no enemies, so
the enemy sensors sit neutral and zap never fires — the same brain just *behaves* like a
navigator there. One brain to train, trade, and battle.
- **Brain:** tiny fixed MLP, ~**10 inputs → 8 hidden (tanh) → 5 outputs ≈ 120 weights**.
- **Inputs — EGOCENTRIC** (relative to the robot's heading, matching GridBot's core lesson;
  this is also what lets a brain *generalize* to new mazes):
  1. wall ahead (0/1)
  2. wall left (0/1)
  3. wall right (0/1)
  4. pit ahead (0/1)
  5. goal ahead-ness (−1…1: in front of vs behind me)
  6. goal right-ness (−1…1: to my right vs left)
  7. goal distance (0…1, normalized)
  8–10. enemy ahead-ness, enemy right-ness, enemy distance (neutral in the campaign).
  `ENEMY_AHEAD`/`ENEMY_NEAR` + `EnemyView` already exist in the interpreter.
- **Outputs / action set = 5:** forward / turn-L / turn-R / jump / **zap** (argmax → one move;
  zap is a no-op outside the Arena). **Backward is omitted** (redundant — a policy can always
  turn around; an extra output just muddies learning). Backward *stays in the campaign*
  (harmless, teaches "movement independent of facing").
- **Execution:** on hitting `N_NEURO`, the interpreter reads sensors at the robot's pose,
  forward-passes, argmax → executes one primitive. **Steppable & deterministic** — fits the
  explicit-stack interpreter and animates like any command.
- **Serialization:** weights save with the program (ProfileStore JSON, library, **ESP-NOW
  share**) → programs trade/battle *with the brain baked in*. **Weights freeze at battle
  time** so matches stay reproducible.

## Three ways to train a brain (the ML trichotomy)
The NEURO block is "a trainable brain"; the **training method is a choice that depends on
the problem.** Each maps to a natural game situation:

| Paradigm | "How it learns" | Game home | Status |
|---|---|---|---|
| **Supervised** (backprop) | a teacher shows the right answer | navigation — **distill your own algorithm** | **primary** |
| **Reinforcement** (tabular Q) | trial & error from reward | the maze — robot figures it out itself | pillar |
| **Evolution** | breed survivors, no within-life learning | the **Arena** — fight, winners breed | combat |

### 1. Supervised / backprop (PRIMARY — "learn from a teacher")
Backprop needs labels; the cleanest label source is the kid's own work:
- **Distillation (best idea in the design):** run the kid's *existing algorithm*, log every
  `(sensors → action)`, backprop the net to imitate it. Auto-generated perfect labels, no
  tedium. **The headline lesson:** the program is correct by construction; the net only
  *approximated* it → it matches on training mazes but fumbles a weird unseen one → "your
  code *knows* the rule; the brain *guessed* it." And an honest limit-as-feature: a reactive
  net can imitate a *reactive* algorithm (wall-follower distills perfectly) but **not** a
  *stateful* one (sorting needs memory) — teaching *why some problems need memory*.
- **Demonstration:** the kid drives; `(senses, action)` pairs train the net. Supplement.
- Compute: ~70 weights × hundreds of examples × hundreds of epochs = milliseconds; animate
  the loss curve dropping and the weights changing.

### 2. Reinforcement / tabular Q-learning (PILLAR — "learn from reward")
- **Tabular Q** over the discrete sensor-state × 4 actions: a few KB, trains in thousands of
  steps, **best visualization in the project** — color cells by learned value, arrow by best
  action, watch "good feelings" spread back from the battery. "The robot learns the maze by
  trial and error, no code, just reward."
- Teaches **exploration vs. exploitation** (ε-greedy = "sometimes try random so you don't
  get stuck"), and **reward shaping** (sparse goal-only learns slowly; shaped via the
  existing BFS `distanceToGoal` learns fast — can even demo **reward hacking**).
- **Deep Q** is the bridge: "the table got too big → approximate Q with a net," connecting RL
  back to the NEURO block and backprop. Advanced.
- **Policy gradients (REINFORCE):** parked as an *advanced peek only* — high-variance, slow,
  finicky; reads as "nothing's happening" on a kid timescale. Q-learning teaches the same
  "learn from reward" idea far more reliably.

### 3. Evolution / self-play (COMBAT — "no teacher, only win/lose")
- Fighting has no per-step right answer, only outcomes → evolution's domain. A population of
  fighter-brains battles (each other + house bots Rusty/Bolt/Vex/Ace); winners breed
  (Gaussian mutation, crossover, elitism), seeded → reproducible.
- **Combat brain = 5 outputs** (incl. zap), enemy-aware sensors (already in engine).
- **Fitness:** match outcome shaped with hits-landed / survival / shoved-into-pit /
  reached-goal-first (mostly already computed in the Arena tick engine).
- **Backprop can bootstrap** a fighter (distill a scripted house bot, or imitate your
  driving) → then **evolve** to actually get good. Pipeline: *bootstrap with backprop,
  sharpen with evolution.*
- **The arms-race endgame hook:** evolve vs. one bot → overfit to beating it; evolve vs. a
  ladder → robust; then **battle a friend's brain over radio, lose to their weird strategy,
  go evolve more.** That train → trade → battle → re-train loop is the retention engine and
  teaches co-evolution honestly.

## Radio (ESP-NOW): neurobits are fully trade & battle compatible
This falls out of the architecture with **zero new battle infrastructure**:
- A "neurobit" is just a **program whose `NEURO` node carries a weight payload** (~70
  floats). Programs already serialize and transfer over ESP-NOW for the existing
  trade/battle; the NEURO node's weights ride along in the same blob (add the payload to the
  wire format + an **architecture/version id** so both devices agree on the brain shape).
- The Arena already runs **two programs deterministically**; a `NEURO` block is deterministic
  given **frozen weights + sensors**, so a trained brain battles a friend's exactly like a
  scripted bot does today — same code path.
- So the full loop works over radio: **train → trade → battle → re-train.** Your evolved
  champion vs. their evolved champion; trade brains Pokémon-style (it slots into the existing
  trade mechanic). Weights freeze at match start → reproducible matches and replays.
- Caveat: keep the brain architecture **fixed (or versioned)** so a received neurobit always
  has a shape the receiver can run; reject/upgrade on version mismatch.

## The neuron lessons (build small → big)
Don't open on the full brain. Each lesson is small enough to *fully* visualize:
- **Lesson 1 — one neuron, binary (BUILT).** 2 inputs (wall, pit) → **1 output**, a
  go-vs-turn decision; backprop drives the error to ~0 and all 4 *situations* flip to ✓.
  *Note (a common misread):* the 4 rows on screen are the **4 situations** the neuron could
  see (the truth table of 2 yes/no sensors) — **not** 4 outputs. There is ONE output; 3 of
  the 4 situations are TURN simply because the rule is "wall OR pit." No hidden layer is
  needed because OR is *linearly separable* (the test proves it learns it).
- **Lesson 2 — a row of neurons, multi-class.** turn / jump / go = **3 output neurons,
  argmax** (e.g. "jump if pit, turn if wall, go otherwise" — pick one answer per situation so
  there's a clean target). Still no hidden layer if separable. Teaches "one neuron per action."
- **Lesson 3 — a hidden layer.** A decision a single line can't make (XOR-flavored); the
  hidden units carve up the space. (`test_nn` already asserts one neuron *fails* XOR.)
- **Lesson 4 — the full maze brain** (the 10→8→5 unified brain above).
- Always fully visualizable: neuron diagram, weights as edges, falling error bar, decisions
  flipping to ✓. If lesson 1 lands, the rest is mechanically more of the same.

## Teaching the math, age-gated
- **Everyone:** the *story* + the *visualization*. Backprop = "the brain guesses, sees how
  wrong it was, and every connection that pushed toward the wrong answer gets weaker, every
  one toward the right answer gets stronger." A falling loss curve + wiggling weights *is*
  the understanding.
- **Older / curious (opt-in "under the hood"):** gradient as a slope ("which way is
  downhill?"), learning rate as "step size," gradient descent as "roll downhill on the error
  landscape." A 12-year-old groks this; a 7-year-old gets the story. Same screen, two depths.

## Build order (sequenced; rich curriculum, big total scope)
1. **Single-neuron backprop lesson** — foundational, tiny, validates the teaching. *(first)*
2. **Tabular Q-learning maze** — best viz, "learn from reward," no net needed.
3. **NEURO block + distill-your-algorithm** — the headline neurosymbolic feature.
4. **Evolution self-play in the Arena** — the battle hook.
5. **Deep Q** — bridges RL and nets (advanced).
6. *(Later/optional)* policy gradients as a peek; the data/sorting layer if not shipped earlier.

## Honest caveats
- **Concept load is high** — the unlock gating protects beginners; each piece lands one idea.
- **Overfitting is frequent** with tiny data — don't hide it; make train/test split a lesson
  ("it memorized instead of learned").
- **Degenerate policies** (always-forward) can stall learning — curate tasks so the sensors
  actually matter.
- **Local minima / non-convergence** — frame honestly ("sometimes it gets stuck; try again").
- **Reactive limits** — a memoryless net can't do stateful algorithms; that's a feature to
  teach, not a bug to hide.
- **Engineering:** keep the NN math Arduino-free (in `src/`-engine) so it's host-unit-tested;
  float32, tanh/ReLU hidden, argmax/softmax out; seeded RNG for reproducible runs/battles.
