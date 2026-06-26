# GridBot — 5-Day Curriculum: *from zero to a trained soccer-player bot*

A week-long (or five-session) plan that takes a kid with **no coding experience** to a robot they
**trained themselves** to play soccer — and, just as importantly, an understanding of *when to write
the rules and when to let the robot learn them*.

- **Audience:** ages ~7–13, one kid per **$10 CYD** (or pairs sharing one).
- **Time:** **5 days, 1–2 hours each** (≈ 7–10 hours total).
- **Setup:** none — it's all on the device. No internet, no accounts. For two-device days you just
  need two CYDs in the same room.
- **This plan distills three companion docs** — read them for depth:
  the [README](../README.md) (the what), the [HANDBOOK](HANDBOOK.md) (the full teacher guide, with a
  page per lesson), and the [Hand-Coding Guide](HAND-CODING-GUIDE.md) (writing bots by hand).

> **How features unlock.** Powers arrive as you climb campaign levels — **jump (Lv 6) · loops (Lv 10)
> · sensing `if`/`until` + the Arena (Lv 15) · functions (Lv 20) · NeuroBot training (Lv 28)**. So the
> campaign *is* the spine: each day plays a few levels to earn the day's new power, then uses it. Pace
> is ~5–6 levels per session; a kid who's flying can race ahead, a kid who's stuck can slow down —
> the **2-/3-star** scoring rewards coming back to a level with a cleaner program.

---

## The week at a glance

| Day | Theme | New power | They can… | The big idea |
|---|---|---|---|---|
| **1** | 🚗 **Drive** | move / turn / **jump** | steer the robot through a maze by hand | a **program is a sequence** |
| **2** | 🧠 **Think** | **loops** + **sensing** (`if`/`until`) | write **one** program that solves mazes it's never seen | a **rule generalises** (an *algorithm*) |
| **3** | ⚔️ **Compete** | **functions** + the **Arena** | hand-code a **battle** bot and a **soccer** bot | clear rules win — *until they don't* |
| **4** | 🤖 **Learn** | **NeuroBot** (Teach / Q-Learn / Evolve) | **train** a brain instead of writing rules | when a job is feel, not rules, **let it practise** |
| **5** | 🏆 **Champion** | the trainer's knobs + the **Room** | train a soccer **striker** and run a **tournament** | iterate, measure, and *match the method* |

---

## Day 1 — 🚗 Drive: *make it move*

**Goal:** the robot obeys a list of steps you write, and you solve a maze by hand.

1. **Pick a player** (avatar + name) — this saves their progress.
2. **CodeLab → "1 · Move"** (Learn → CodeLab): tap **Run**, watch forward/turn/forward reach the goal.
3. **Play campaign levels 1–9.** Drag command blocks, tap **Run**, read the failure, fix one block,
   re-run. **Jump unlocks at Lv 6** — use it to clear a pit.
4. **Star chase:** beat a level, then try to beat it again with **fewer blocks** for 3 stars.

**Ask:** *"What happens if two steps swap places?"* (forward-then-turn ≠ turn-then-forward — **order
matters**). **Checkpoint:** the kid can plan a turn-by-turn path and debug a wrong turn.

---

## Day 2 — 🧠 Think: *make it solve mazes it's never seen*

**Goal:** the leap from *memorising steps* to *a rule that adapts* — the heart of the whole course.

1. **CodeLab → "3 · Repeat"** (loops unlock at **Lv 10**): replace five forwards with one loop.
   *Why is a loop better than copy-paste?* (efficiency **and** readability.)
2. **CodeLab → "4 · Sense"** (`if` / `until` unlock at **Lv 15**): the robot **reacts** to what it
   senses. Build the **wall-follower**: `until goal { if wall { turn left } forward }`.
3. **The "aha":** run the *same* wall-follower on **three different mazes** without changing a block.
   It solves all three. *Why?* Because it's a **rule**, not a memorised path.
4. **CodeLab → "5 · Functions"** (Lv 20): name a group of steps and reuse it — the program gets
   shorter *and* clearer.

**Ask:** *"This program, unchanged — could it solve a maze you've never opened? Why?"*
**Checkpoint:** the kid explains *why* the wall-follower generalises. *(This is the curriculum's
hinge — don't rush it.)*

---

## Day 3 — ⚔️ Compete: *hand-code a fighter and a striker*

**Goal:** use sensing to build **Arena** bots by hand — and feel where hand-coding starts to strain.
Work from the **[Hand-Coding Guide](HAND-CODING-GUIDE.md)** (it has on-device screenshots of each program).

1. **Battle (Sumo):** build the **hunter** — a priority list: *zap if the foe's ahead; never charge a
   pit; turn to face the foe; otherwise close in.* Fight **vs Computer** and win. **Reorder the rules**
   (put `forward` first) and watch it get dumb — *order is the whole lesson.*
2. **Soccer:** build the **dribbler** — `if ball^` (touching the ball) check the **net** and circle
   behind it before pushing, so you don't shove it into your *own* goal. Try the **`zap`-swap**:
   `if ball^ { zap }` makes your robot **trade places with the ball** to turn it around.
3. **The honest moment:** the hand-coded striker *plays*, but a good opponent beats it. Hand-coding
   nailed the maze and the fight; soccer needs **finesse** that's hard to write down.

**Ask:** *"Why is 'follow the wall' easy to write as a rule, but 'dribble past a defender and pick
your corner' is not?"* **Checkpoint:** a working hand-coded soccer bot **and** the realisation that
it's not enough — which *earns* Day 4.

---

## Day 4 — 🤖 Learn: *stop writing the rules, start training them*

**Goal:** reach **NeuroBot** (unlocks at **Lv 28**) and train a brain from examples and reward.

1. **NeuroLab lessons** (Learn → NeuroLab) — small enough to *watch*: **One neuron** (it guesses,
   sees the error, nudges), **Perception** (the 10 senses the brain gets), **Many actions**
   (go/turn/jump), and **The Right Tool** (*match the method to the problem*).
2. **Arena → Train a fighter → Soccer.** Try all three engines and watch the **learning-curve**:
   - **Teach** — *imitate an expert dribbler* (sharp in seconds).
   - **Q-Learn** — *reward* a goal; no teacher, just "did it score?".
   - **Evolve** — *the best brains breed and mutate.*
3. **Code vs brain:** field a **trained** soccer brain against yesterday's **hand-coded** one.

**Ask:** *"It learned mazes, then fighting, now soccer — what stays the same about its brain, and what
changes?"* (the network is identical — `10 → 8 → 5` — only what it senses as the goal and how it's
rewarded changes.) **Checkpoint:** a trained brain that out-plays the hand-coded bot at soccer.

---

## Day 5 — 🏆 Champion: *train a striker, then a tournament*

**Goal:** refine a real soccer **striker**, then crown a champion across the class.

1. **Train a striker properly:** **Teach** a dribbler, then **Q-Learn** to sharpen it. The trainer
   **penalises pushing the ball the wrong way (toward your own net) 2×** and **rewards a goalward
   zap**, so the brain learns to **avoid own-goals** and use the **swap**. **Save** it as your fighter.
   *(Tune the **Knobs** — learning rate, rounds, explore — and watch the loss fall, or thrash.)*
2. **Level-up loop:** lost a match? **Save foes** (copy the winner into your library), train against
   *it*, and **rematch**. Because the Arena is **deterministic**, a rematch is exact — so improvements
   are real, not luck.
3. **Tournament** — the capstone:
   - One device: **vs Computer → Tournament** (a Cup bracket or Ladder of your saved fighters).
   - The class: **Arena → Radio → Room** gathers *every* nearby CYD's striker into **one shared
     bracket** — every screen replays the identical Cup from a shared seed and crowns the **same
     champion**. No central scoreboard needed.

**Ask:** *"Your hand-coded bot vs your trained bot — which would you bring to the tournament, and
why?"* **Checkpoint (the finish line):** a **soccer-player bot the kid trained themselves**, fielded
in a real bracket — and a kid who can say *when* they'd write the rules and *when* they'd train them.

---

## What they walk away with

- **Coding:** sequence → loops → conditionals → functions → debugging (CSTA-aligned; see the HANDBOOK
  §9 standards map).
- **AI literacy (AI4K12 "Five Big Ideas"):** what a network *senses*, how it *learns* (imitation,
  reward, evolution), and the meta-skill of **matching the method to the problem** — the difference
  between a *rule* and a *trained* behaviour, felt in their own hands.
- **A robot they made and trained** that plays soccer — and the confidence that AI isn't magic.

## Teacher notes

- **Pairs work great:** one drives, one reads the failure aloud — narrating the *read → guess → change
  one thing → test* loop is the real computational-thinking lesson.
- **Falling behind on levels?** The lessons (CodeLab/NeuroLab) teach the *concepts* independently of
  the campaign grind — run the lesson even if a kid hasn't unlocked that level yet, then let the
  campaign reinforce it.
- **Going long?** Day 5 alone can fill a "science fair" afternoon: everyone trains a striker, then a
  Room tournament decides it.
- **Every number a kid will see** (this bot beats that one) is **reproducible** — the Arena is
  deterministic, so results are comparisons, not coin-flips.
