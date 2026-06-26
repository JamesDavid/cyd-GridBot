# GridBot — Hand-Coding Guide (before you switch to AI)

*For kids, parents, and teachers.* This guide shows how to **write the rules yourself** with the
block editor for all three games — **Maze**, **Battle**, and **Soccer** — so you understand what a
robot needs to think about *before* you ask the AI to learn it for you.

The big idea at the end is the best part: **some jobs are easy to hand-code and some are not** — and
that's exactly *why* machine learning exists. You'll feel it in your hands.

> Everything below is a real program you can build in the editor, and every result was measured by
> playing these exact bots against trained neural bots on the device's match engine. The numbers are
> at the bottom of each section.

---

## 1. How the editor thinks

Your robot runs its program **over and over, very fast**. Each loop it looks at its **senses**, then
does **one action**. That's it. Good robots are just good rules about *"what do I see → what do I do."*

**Actions** (one per step): `forward`, `turn left`, `turn right`, `jump` (hops 2 tiles, clears a
pit), `zap` (shoves a rival — Battle only).

**Senses** you can test in an `if` or `repeat until` block:

| What | Blocks | Works in |
|---|---|---|
| Walls | `wall` (ahead), `wall <`, `wall >`, `wall/pit` (either ahead) | all |
| Pit | `pit` | all |
| Goal | `goal` (am I standing on it?) | Maze |
| Foe / rival | `foe ^`, `foe near`, `foe <`, `foe >` | Battle **and Soccer** |
| Ball | `ball ^`, `ball <`, `ball >`, `ball near` | Soccer |
| Net (the goal you shoot at) | `net <`, `net >` | Soccer |

Two blocks do the looping and thinking:
- **`repeat until <sense>`** — keep doing the inside until the sense is true. We use
  `repeat until goal` for everything (in Battle/Soccer there's no goal, so it just runs the whole match).
- **`if <sense> { … }`** — do the inside only when the sense is true. You can join **two** senses
  with `&` (and) or `|` (or), e.g. `if ball < & net >`.

> **Important sensing note:** in Soccer the same `foe` blocks that find an enemy in Battle find the
> **other player**, and `net <` / `net >` point at *the goal you're attacking*. There is **no**
> "goal is that way" sense for the Maze — only `goal` (are you on it). That one missing sense is a
> big part of why the three games feel so different to hand-code.

---

## 2. Maze — *the wall-follower* 🧩 (hand-coding's home turf)

**Goal:** reach the battery. **The trick every maze-solver knows:** keep one hand on a wall and walk.
You will *always* find your way out — even on a maze you've never seen.

### Build this

```
repeat until goal {
    if wall { turn left }
    forward
}
```

That's the whole thing. "If there's a wall in front of me, turn; otherwise step forward." Because you
always turn the *same* way, you trace along the wall and sweep the maze until you hit the goal.

### Make it handle pits (use a function or just add a line)

Real mazes have **pits**. Walking into one = you fall. Add one rule:

```
repeat until goal {
    if wall { turn left }
    if pit  { jump }
    forward
}
```

Now you **jump pits** and **turn at walls**. This is a complete, general maze robot in *3 rules*.

### Why this is the magic lesson

We gave this exact program **16 mazes it had never seen**:

| Robot | Solved (unseen mazes) |
|---|---|
| Wall-follower **+ jump** (hand-coded, 3 rules) | **8 / 16** |
| Plain wall-follower (no pit rule) | 5 / 16 (falls in pits) |
| A **brain trained on one maze** | **1 / 16** |

The hand-coded **rule** generalises — it solves mazes it has never seen, because the *idea* is
correct. A brain that practised **one** maze just **memorised** it and gets lost everywhere else.
**This is the #1 thing to feel before doing AI:** a good rule beats a narrowly-trained brain.
*(The fix for the brain is to train it on **lots** of mazes — that's the real ML lesson in NeuroLab.)*

> Teacher note: ask *"why did adding `jump` help? why does the brain fail a new maze when it aced its
> own?"* That's the difference between **a rule** and **memorising**.

---

## 3. Battle (Sumo) — *the hunter* 🤖 (hand-coding still wins)

**Goal:** zap/shove the rival out; last bot standing wins. A great fighter is a **priority list**:
the most important rule first. The editor checks your `if`s top-to-bottom and skips the ones that
aren't true, so order = priority.

### Build this

```
repeat until goal {
    if foe ^ { zap }          ← rival right in front? ZAP it
    if pit  { turn right }    ← never charge into a pit
    if foe > { turn right }   ← rival off to my right? turn to face it
    if foe < { turn left }    ← rival off to my left? turn to face it
    if wall { turn right }    ← don't jam into a wall
    forward                   ← otherwise, charge in
}
```

Read it as a sentence: *"Zap if you can; never suicide into a pit; turn toward the foe; don't get
stuck on a wall; otherwise close the distance."* It **hunts** — it always turns to face the enemy and
shoves when close.

### How it does

We played this hand-coded hunter against trained neural fighters over 16 matches:

| Opponent | Hunter's record (W-D-L) |
|---|---|
| A **distilled** neural fighter (copied an expert) | **9 - 5 - 2** |
| A **Teach→Evolve** neural fighter (the strongest trainer recipe) | **9 - 5 - 2** |

**The hand-coded hunter *wins*.** Battle rewards a clear, correct rule — and the trained bots are only
*imitating* a hunter like this one, so the original is a step ahead. Hand-coding is genuinely
competitive here.

> Teacher note: have students **reorder** the rules (put `forward` first, or `zap` last) and watch the
> bot get worse. Priority order is the whole lesson.

---

## 4. Soccer — *the dribbler* ⚽ (where AI starts to win)

**Goal:** push the ball into the **net**. This is the hard one to hand-code, and that's the point.
Pushing a ball is tricky: you have to get **behind** it (on the far side from the net) so that when
you shove, it goes *toward* goal — not into your own net.

### First try (and why it's not enough)

The obvious bot just chases the ball and pushes:

```
repeat until goal {
    if ball < { turn left }
    if ball > { turn right }
    forward
}
```

It scores sometimes… but it shoves the ball **whatever way it's facing**, including into its *own*
goal. Against a real opponent it **loses every time**.

### The good version — *get behind the ball*

The fix uses the **net** sense. When you're right on the ball, if the net is off to one side you're
on the wrong side — so **turn away** from the net to swing *around* the ball until the net lines up in
front, *then* push:

```
repeat until goal {
    if ball ^ {                 ← I'm touching the ball
        if net < { turn right } ← net is left → I'm on the wrong side, orbit around
        if net > { turn left }
    }
    if ball < { turn left }     ← otherwise chase the ball
    if ball > { turn right }
    forward
}
```

This "circle behind, then push" robot is **much** stronger.

### How it does

Each row is **16 matches** against a different trained neural striker (goals = total over the 16):

| Trained-striker opponent | "Get behind the ball" |
|---|---|
| Distilled striker **A** | **draw** — 208–208 |
| Distilled striker **B** *(same recipe, different practice)* | **loses** — 128–320 |
| **Teach → Evolve** striker | **wins** — 288–208 (16–0) |
| **Max-trained** striker *(the strongest possible)* | **loses** — 80–352 |

So an honest read: a *well-designed* hand-coded soccer bot is **right at the edge of competitive**. It
can **tie** one trained striker and even **beat** the evolved one — but it **loses to others**, and the
strongest brain beats it **clearly** (80–352). It is **not** a robust win the way the maze and battle
bots are; across the board the trained brains have the edge. We tried *eight* hand-coded strategies
(including using the `foe` sense to aim away from the keeper — it made things **worse**, and a
"commit the shot" variant lost every match). The ceiling for simple reactive rules is "can hang with a
trained striker on a good day, but a notch below the best."

**Why?** A trained brain saw thousands of examples and learned *finishing finesse* — aiming at the
open corner of the net, hitting the exact spot to stand. Reactive blocks can't hold a plan in memory
(there are no variables), so they can't quite match it. **Soccer is the game where *learning* pays
off most** — which is the perfect moment to open NeuroLab.

---

## 5. The payoff: *match the method*

Here's the whole lesson on one line each:

| Game | Best hand-coded result vs a trained brain | Who wins? |
|---|---|---|
| 🧩 **Maze** | a 3-rule wall-follower solves **8×** more unseen mazes | ✍️ **Hand-coding** — a correct rule generalises |
| 🤖 **Battle** | the hunter goes **9-5-2** vs trained fighters | ✍️ **Hand-coding** — clear priorities win |
| ⚽ **Soccer** | the best dribbler **ties some** trained strikers, **loses to others**, and the strongest beats it 80–352 | 🧠 **Learning** — finesse needs practice |

**That's why machine learning exists.** When a job is easy to describe as rules (find the wall, face
the foe), *write the rules* — it's clearer, faster, and it generalises. When a job is full of feel and
judgement that's hard to put into words (dribble past a defender and pick your corner), **let the
robot practise** until it learns what you couldn't easily say.

### Try it yourself, in order
1. **Hand-code** the maze wall-follower. Watch it solve a maze you've never opened. *(You wrote a
   rule that works on the unknown — that's powerful.)*
2. **Hand-code** the battle hunter. Beat the computer. Reorder the rules and see it get worse.
3. **Hand-code** the soccer dribbler. Get a draw against the trained striker — then notice you can't
   quite win.
4. **Now open NeuroLab.** Use **Teach** to copy an expert, then **Evolve** or **Q-Learn** to sharpen
   it against a real opponent. Watch the soccer brain do the thing your blocks couldn't.

You'll have *earned* the AI — you know exactly what it's learning, because you tried to write it down
yourself first.

---

## For parents & teachers

- **No setup, no internet.** Everything is on the $10 device. Hand-coding uses only the block editor;
  the AI lives in NeuroLab.
- **Three discussion questions** that land the concept:
  1. *Maze:* why does one short rule solve mazes it's never seen, when the trained brain gets lost?
  2. *Battle:* why does the **order** of the rules matter so much?
  3. *Soccer:* what makes "push the ball to the net" so much harder to write down than "follow the wall"?
- **The meta-lesson:** AI isn't magic and it isn't always the answer. The skill is knowing **when** a
  problem wants *rules* and when it wants *learning*. Kids who hand-code first never treat the brain as
  a black box — they know what it's trying to do, because they tried to do it themselves.

*Reproduce any number here with `tools/bot_eval.cpp` (host build) — it plays these exact block
programs against trained brains on the real match engine.*
