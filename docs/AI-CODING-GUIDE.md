# GridBot — AI Coding Guide (train a brain)

*For kids, parents, and teachers — the sequel to the [Hand-Coding Guide](HAND-CODING-GUIDE.md).* There,
you **wrote the rules** with blocks. Here, you **train the rules**: you drop a **`+brain`** into your
program and teach it — by example, by reward, or by evolution — until it plays for you.

The punchline of the two guides together is the whole point of GridBot: **some jobs are easy to write
as rules and some are easier to *train*** — and a good engineer knows which is which. The hand-coding
guide showed where rules win (the maze, the battle). This one shows where **learning** wins (soccer) —
and how to actually do the training without the classic traps.

> Every result below was **measured on real devices** — two CYDs playing head-to-head in the
> deterministic Room (so the scores reproduce). The full log is in
> [TRAINING_FINDINGS.md](TRAINING_FINDINGS.md). NeuroBot unlocks at **campaign Level 28**.

---

## 1. Meet the brain (the `+brain` block)

In the editor, once you've graduated, a new block appears: **`+brain`**. Drop it into your program and
it runs a tiny **neural network** instead of a rule you wrote. Tap **`train brain >`** on its row to
open the trainer.

![The +brain block in the editor](img/neuro-block.png)

The brain is the **same tiny network for every game**: **10 senses → 8 hidden → 5 actions.** Each step
it reads its 10 senses, the numbers flow through, and it **does the strongest of its 5 actions**
(that's called *argmax*). You never write the rule — **training** sets the numbers (the *weights*) so
the right action comes out.

![What the brain senses](img/robot-brain.png)

**What it senses** (all *relative to the robot* — "ahead/right", like a driver, not a map): walls
ahead/left/right, a pit ahead, the **goal**'s bearing, and the **rival/ball**'s bearing. **What it
does:** forward, turn left, turn right, jump, zap. *Same brain, many jobs* — only **what it senses as
the goal** and **how you reward it** changes between a maze-solver, a fighter, and a striker.

> **Watch one neuron learn first.** In **NeuroLab → One neuron**, a single neuron guesses, sees how
> wrong it is (the **loss**), and nudges its weights until it says **"learned!"** Nobody told it the
> rule — it found it from **examples**. That's all training is, scaled up.
> ![One neuron learning](img/neuron-lesson.png)

---

## 2. The three ways to train

The trainer (**Arena → Train a fighter**, or **`train brain >`** on a `+brain` row) gives you three
engines. They are **not** interchangeable — each is strong somewhere and weak somewhere.

| Trainer | What it does | Strong because… | Watch out |
|---|---|---|---|
| **Teach** *(imitation)* | backprop the brain to **copy a scripted expert** (a perfect solver / hunter / dribbler) | **sharp in seconds** — competent almost immediately | capped at the expert; it imitates, it doesn't out-think a foe |
| **Q-Learn** *(reward)* | the brain tries over and over; a **reward** (reached the goal / scored) reinforces good moves | no teacher, no labels — just *"did it work?"* | **noisy**; needs many rounds; a *bad* reward can mislead it |
| **Evolve** *(neuroevolution)* | a **population** of brains plays; the best **breed + mutate**; repeat | discovers moves no expert was scripted for; **adapts to a specific foe** | **slow from scratch**, and on a *fixed* board it **overfits** |

![Q-learning spreads value back from the goal](img/neuro-qlearning.png) ![Evolution breeds the best brains](img/neuro-evolution.png)

> ### ⭐ The one rule that makes them work together: **build on the brain you have.**
> In GridBot's trainer, **Teach, Evolve, and Q-Learn all start from your current brain** — they never
> throw it away. So the winning move is almost always **Teach first to get competent, *then* Evolve or
> Q-Learn to sharpen.** The only thing that wipes a brain back to random noise is the explicit
> **"Fresh"** button. *(This single design choice is why "Teach → anything" actually builds on the
> Teach — it matters more than it sounds.)*

---

## 3. Maze — *teach a solver* 🧩 (and watch it over-fit)

**Goal:** train a brain to reach the battery. Open a maze level's `+brain` and tap **Teach** — in a
second or two it imitates a perfect maze-solver and walks to the goal.

![The maze trainer](img/neuro-train.png)

### The trap, and the fix — *generalisation*

Here's the lesson the hand-coding guide set up. A brain **trained on one maze memorises that one
maze**: we gave one such brain **16 mazes it had never seen** and it solved **1 / 16** — while a
3-rule hand-coded wall-follower solved **8 / 16**. For a maze, a *rule* generalises almost for free; a
brain has to be **taught to generalise**.

| Robot | Solved (16 unseen mazes) |
|---|---|
| Hand-coded wall-follower **+ jump** (3 rules) | **8 / 16** |
| Brain trained on **one** maze | **1 / 16** |
| Brain trained the **Teach** way across **many** mazes | climbs toward the rule's score |

**The fix:** train across **varied** boards, not one. In the trainer the **map changes** as it learns
(and the **"Generalize"** button pushes it onto fresh boards), so the brain learns the *idea* of
following walls instead of one path. The kid-scale version of the single hardest idea in ML —
**generalisation** — and you can *watch* a one-board brain freeze on a new map, then watch a
varied-trained one keep going.

> **Try it:** Teach a brain on a level, then hit **Generalize** / a new board and Run again. Did it
> solve it? Train a few more boards and try once more. *(This is exactly why real AI needs lots of
> varied data, not one perfect example.)*

---

## 4. Battle (Sumo) — *train a hunter* 🤖

**Goal:** train a brain to zap the rival out of the ring. **Teach** copies a greedy cone-chaser hunter,
and it's good immediately.

![The Arena trainer (Teach / Evolve / Q-Learn / Save / Fight)](img/neuro-arena-train.png)

### How it does

| Trained fighter | Result |
|---|---|
| **Teach** (imitate the hunter) | **beats Vex 6 / 6** on unseen rings |
| **Q-Learn** (reward a landed zap) | **~9 / 10** win-or-draw vs Vex on unseen rings |
| The **hand-coded** hunter, for contrast | goes **9-5-2** vs trained fighters |

Read those together: a trained fighter is **genuinely competitive** — but the hand-coded hunter still
edges it, because the trained bots are *imitating a hunter like that one*. Battle is a game with a
clear, correct rule, so **hand-coding holds its own and training merely matches it.** *(The real win
of training here is that a first-timer can press **Teach** and beat Vex without writing a single
rule.)*

> **The trainer's secret sauce (so Teach actually works in Battle):** the expert it copies is a
> **cone-chaser** that charges when the foe is in its forward wedge and turns to face it otherwise —
> strictly better than the reactive hand-coded hunter, which only advances when perfectly lined up.
> The brain learns the *better* policy, then you can **Evolve** it against a specific rival to pull
> ahead.

---

## 5. Soccer — *train a striker* ⚽ (where learning **wins**)

This is the one. Every hand-coded striker we tried **lost** to a trained one — the best hand-coded bot
won only **4–14%** of 64 games vs distilled strikers — and a *well*-trained striker (Teach→Evolve) beat
the best hand-coded bot **~92%** (59-5). Soccer rewards *finishing finesse* (aim the open corner, stand
in the exact spot) that's hard to write as rules but **learnable** — *if you train it well.*

Open the soccer trainer (**Arena → Train a fighter → Soccer**). The Brain-Cam view shows the *same*
`10→8→5` net, now sensing the **ball**, the **net**, and the **rival** — and the 5th output is
relabelled **`swap`**: there's no sumo zap on the pitch, so firing while facing the ball **trades places
with it** (turning it back toward goal). A trained striker can learn to use it.

![Training a soccer brain (Brain-Cam)](img/soccer-brain.png)

### Which recipe actually works? (one multi-seed eval)

Each recipe was trained from scratch on 6 seeds and judged over **72 matches** (6 × 12 kickoffs) vs the
same strong striker it trained on (`tools/bot_eval.cpp`). **Win-rate**, not a single scoreline — but read
the caveat below:

| Recipe | Win-rate vs a peer striker (72 games) |
|---|---|
| **Teach → Evolve** vs the opponent | **84%** — best here |
| **Teach** (imitate an expert dribbler) | **63%** — strong, in seconds |
| **Teach → Q-Learn** (refine for reward) | **44% (cone) / 26% (live)** — *didn't help; hurt* |
| **Evolve from scratch** | **13%** — much weaker |

> **Qualify it:** this is **one deterministic run** over a fixed set of 6 seeds vs **distilled
> strikers** (one opponent class), tiny `10→8→5` net, seconds of training. It's bit-reproducible but
> **not** an independent-sample confidence interval — treat these as directional, not exact constants.

The takeaways from this eval:

1. **Imitate an expert first.** Teach (63%) clearly beats evolving from random noise (13%) in the same
   time. If a good expert exists, copy it — don't start from nothing.
2. **More training isn't always better.** Reward-refining a *good* distilled striker with Q-Learn
   **didn't improve it** — both cone (44%) and live-opponent (26%) Q-Learn landed *below* Teach (63%).
   A solid imitation policy is fragile; a second objective can perturb it.
3. **Train well to win soccer.** Hand-coded strikers lose to trained ones; among recipes, refining a
   good imitation *against the opponent* (Teach→Evolve, 84%) did best. Soccer is where the *quality* of
   training pays off.

> **⚠️ A lesson from our own mistake — why one match isn't an eval.** An earlier on-device run found a
> tidy story: Q-Learn-vs-a-cone "regressed 0–7" and training vs the live opponent "fixed it 3–3." That
> was **one deterministic match**. Re-run over 72 games it **did not reproduce** (live-opponent Q-Learn,
> 26%, was worse than the cone, 44%). The deterministic Arena makes a single match
> *reproducible*, but reproducible ≠ representative — always average over seeds. (Full before/after in
> [TRAINING_FINDINGS.md](TRAINING_FINDINGS.md).)

The soccer trainer also helps you out: it **penalises a wrong-way push 2×** and **rewards a goalward
`zap`**, so a trained striker learns to **avoid own-goals** and use the swap — the move the
hand-coded `if ball^ {zap}` bot could only do blindly.

> **🩺 "My bot got *worse* after I trained it!"** — a real outcome, not a fail (you saw it above: a
> Q-Learn refinement dropped a strong striker from 63% to ~30%). When a refinement hurts, **go back to
> the Teach base and keep it**, or refine more gently — more training is not automatically better.

---

## 6. The neurosymbolic combo — *code **and** a brain together* 🧩🤖

You don't have to choose. The strongest GridBot programs are **neurosymbolic**: a **hand-written rule
for the easy part** and a **trained brain for the hard part**, in the *same* program.

```
repeat until goal {
    if pit { jump }   ← a rule: easy to write, never gets it wrong
    brain             ← a trained brain: drives the tricky bits
}
```

The pit jump is a one-line rule you'd never want to *train* (why risk it?). The driving is a judgement
call you'd never want to *write* (too fiddly). Put a rule where a rule is clear and a brain where a
brain is better — **that's exactly how a self-driving car is built**: hand-written safety checks plus a
learned net for the road. In the editor, tap a `+brain` row's **`train brain >`** to teach the brain
half, then Run the whole program.

---

## 7. Power tools (unlock later)

| Tool | What it adds | Use it when | Screenshot |
|---|---|---|---|
| **Memory (RNN)** | the brain feeds its hidden layer **back into itself** — it *remembers* | a maze with dead-ends where a reflex brain **loops** | ![rnn](img/neuro-rnn.gif) |
| **Pilot** (planner) | an outside **route planner** drops waypoints; the brain just **steers dot-to-dot** | big mazes where a purely reactive brain gets stuck | ![pilot](img/neuro-pilot.gif) |
| **Transfer** | start from a brain that already does *okay* on a new map, **fine-tune** to master it | you trained on map A and want map B fast | ![transfer](img/neuro-transfer.png) |
| **Self-play** | the brain fights a **frozen copy of itself**; the bar rises only when it gets better | training a champion with **no expert and no answer key** | — |

A useful caution from the device: **recurrent memory helps a maze but *hurts* a fast reflex battle**
(it's slower and the past doesn't matter) — picking the wrong tool is how real ML projects go wrong.
That's the **Right Tool** lesson: there's no single best method, only a best method *for this problem.*

---

## 8. Watch it think — Brain Cam

The clearest answer to *"what is it actually doing?"* Open **Brain Cam**: the live network, weight-lines
recolouring as it learns (green = push toward yes, red = toward no), the **loss** bar falling, and a
caption naming the **argmax** (it has five outputs and simply does the strongest). Tap any **neuron to
zoom** into its weights.

![Brain Cam — watch a network decide](img/brain-cam.png)

---

## 9. The payoff: *match the method* (the flip side of the hand-coding guide)

Put the two guides side by side and you get the whole course in one table:

| Game | Hand-coded (rules) | Trained (brain) | Who wins? |
|---|---|---|---|
| 🧩 **Maze** | wall-follower solves **8 / 16** unseen | needs **lots of varied** mazes to catch up (one-maze brain: **1 / 16**) | ✍️ **Rules** — a correct rule generalises for free |
| 🤖 **Battle** | hunter goes **9-5-2** vs trained | **Teach beats Vex 6/6**; competitive, *matches* the rule | 🤝 **Tie** — a clear rule is enough; training matches it |
| ⚽ **Soccer** | best dribbler **loses** to trained strikers (**4–14%** win, 64 games) | a **well-trained** striker wins decisively (Teach→Evolve **~92%**, 59-5 vs hand-coded) | 🧠 **Learning** — *if trained well*; finesse needs practice |

**That's why machine learning exists.** When a job is easy to describe as rules (find the wall, face the
foe), *write the rules* — clearer, faster, generalises. When a job is full of feel that's hard to put
into words (dribble past a defender and pick your corner), **train it.** You earned this guide by
hand-coding first, so you know *exactly* what the brain is learning — because you tried to write it
yourself.

---

## 10. Exercises — *be the trainer*

### 🧩 Maze
1. **Make it generalise.** Teach a brain on one level, then Run it on a fresh board (Generalize). It
   fails? Train two or three more boards and try again. How many boards until it solves a *new* one?
2. **One-board over-fit.** Deliberately Teach only on ONE map many times, then switch maps. Watch it
   freeze — that's over-fitting you can see.

### 🤖 Battle
1. **Teach vs Evolve.** Teach a fighter and save it. Now **Fresh**-scramble and **Evolve** from noise
   for the same time. Fight them. Which is better, and why is Teach so much faster to "good"?
2. **Refine the right way.** Teach a fighter, then **Evolve it against Vex specifically**. Does adapting
   to *that* opponent beat a generic Teach?

### ⚽ Soccer
1. **Run the winning recipe.** **Teach** a striker → **Save a foe** (copy a rival into your library) →
   **Evolve against that exact bot** → rematch. Did you flip the result?
2. **Does refining help?** Teach a striker (it's already strong), then **Q-Learn** it and rematch its
   Teach-only self. Often it's *no better or worse* — a good imitation policy is easy to perturb. (Run
   it a few times: the answer varies by seed — which is the point about not trusting one match.)
3. **Code + brain.** Build `until goal { if pit { jump } brain }`, train just the brain, and beat a
   level a pure brain (no jump rule) keeps falling on.

> **Classroom tournament (the capstone).** Everyone trains a striker, then **Arena → Radio → Room**
> gathers every device's bot into one shared, seed-identical bracket — every screen crowns the **same**
> champion, no internet, nothing leaves the room.

---

## For parents & teachers

- **No setup, no internet, no data leaves the room.** The brain trains **on the $10 board** in seconds.
- **The honest simplifications** (so you can answer *"is this really how it works?"*): the senses are
  handed to the brain as clean numbers (a real robot needs a camera — that's the *Perception* lesson);
  the brains are tiny (`10→8→5`, not billions); training takes seconds, not months. The **mechanism**
  (weights, layers, backprop, argmax, reward, evolution) is genuine; only the **scale** is small.
- **Three discussion questions that land it:**
  1. *Maze:* why does a brain need **lots** of mazes to do what one short rule does for free?
  2. *Refinement:* a striker was already strong after **Teach**; why might training it *more* (Q-Learn)
     make it **worse**? (more training ≠ better — a second objective can perturb a good policy.)
  3. *Evals:* one match said Q-Learn-vs-a-cone was bad; 72 matches said the opposite. Why is **one**
     deterministic game not an eval, even though it's perfectly reproducible?
  4. *The meta-question:* when would you **write** a rule, and when would you **train** one?
- **The meta-lesson:** AI isn't magic and it isn't always the answer. The skill is knowing **when** a
  problem wants *rules* and when it wants *learning*, knowing that **more training isn't automatically
  better**, and **never trusting a single run** — average over seeds.

*Every number here is reproducible — the full head-to-head log is in
[TRAINING_FINDINGS.md](TRAINING_FINDINGS.md); the hand-coded counterparts are in the
[Hand-Coding Guide](HAND-CODING-GUIDE.md).*
