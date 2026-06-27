# GridBot — Training-Technique Findings

*What we learned running real fighters head-to-head across two CYDs over ESP-NOW.*

Everything in the "on-device log" below is from **actual matches** between two boards (COM3 = profile
**AA**, blue; COM5 = profile **BB**, red) in the networked **Room**. The Arena is deterministic, so
each was *reproducible* — but **each was ONE match (N=1)**, and a single deterministic match is a point
estimate, not an eval. We later re-ran the claims over many seeds with `tools/bot_eval.cpp`; the
honest aggregate is up top here, and it **corrects** two of the on-device stories.

The bots are all the same tiny network: **10 senses → 8 hidden → 5 actions**. Only *how we trained
the weights* changes.

---

## ⚖️ Multi-seed reproduction (the verified numbers — `tools/bot_eval.cpp`)

> **How to read these numbers (qualifications).** They come from **one run** of `tools/bot_eval.cpp`
> over a **fixed set of seeds** on the **host** build. The Arena is deterministic, so the run is
> bit-reproducible (re-running gives the identical table) — but that is **NOT** a confidence interval
> over independent training; it's one pass over chosen seeds. Sample sizes are modest (**64 games** per
> hand-coded-vs-trained row = 4 opponent seeds × 16 kickoffs; **72 games** per recipe = 6 training seeds
> × 12 kickoffs), opponents are **distilled strikers** (one class of bot, not "all trained bots"), the
> net is tiny (`10→8→5`, seconds of training), and host floats may differ slightly from the device.
> Treat the win-rates as **directional, order-of-magnitude** evidence, not precise constants.
>
> *(Methodology fixes, 2026-06-27: **(a)** the soccer pitch geometry is fixed, so a match seed alone
> replayed the SAME game — both `bot_eval` and the device's `V` eval now vary the kickoff ball per seed,
> so soccer evals are genuinely multi-seed. **(b)** Neural soccer `zap` (the ball-swap) was previously
> nullified at match time and now executes — and the swap was changed to also turn the bot 180° so the
> ball ends up ahead of it. Each of these tweaks **re-shuffled the two reward-refined recipes** (Teach→Q
> and evolve-from-scratch) by 10–25 points and even flipped the cone-vs-live order — while leaving every
> *conclusion* below intact. That volatility is the point: the exact recipe percentages are soft.
> Earlier versions of this section are superseded.)*

| What | Result (win-rate / record) |
|---|---|
| 🧩 Maze: wall-follower+jump vs a one-maze brain, 16 unseen mazes | **8/16 vs 1/16** |
| 🤖 Battle: hand-coded hunter vs two trained fighters, 16 rings each | **9-5-2 / 9-5-2** |
| ⚽ Hand-coded strikers vs distilled strikers (64 games) | chaser **4%**, "get behind" **14%**, commit **14%**, behind+wall **6%** |
| ⚽ Best hand-coded (chaser) vs a *strongly*-trained striker | distill-20k **23%**, Teach→Evolve **7%** (≈92% loss) |
| Recipe — **Teach** only, vs a peer striker (72 games) | **63%** |
| Recipe — **Teach→Evolve** vs the opponent | **84%** |
| Recipe — **Teach→Q-Learn** (refine vs a cone / vs the live opponent) | **19% / 36%** |
| Recipe — **Evolve from scratch** | **4%** |

**What these support (kept qualified):**
1. **Maze & battle reward a rule** — the 3-rule wall-follower solved 8× more unseen mazes than a
   one-maze brain (8/16 vs 1/16), and the hand-coded hunter went 9-5-2 vs two trained fighters. *(Within
   this eval; n=16 each.)*
2. **Soccer rewards training** — every hand-coded strategy **lost** to distilled strikers (4–14% win over
   64 games), and a *well*-trained striker (Teach→Evolve) beat the best hand-coded bot **~92%** (59-5).
   The careful "get behind" dribbler (14%) did modestly better than the naive chaser (4%).
3. **Imitate first** — Teach (63%) clearly beat evolving from random noise (4%) in the same time.
4. **Refining against the opponent helped** — Teach→Evolve-vs-the-opponent (84%) was the strongest
   recipe, clearly above Teach-only (63%) *in this eval*.
5. **More training isn't automatically better** — reward-refining a good distilled striker with Q-Learn
   *reduced* its win-rate (cone 19%, live 36%, both well below Teach's 63%).

**What we retract:** the **cone-trap narrative** ("Q-vs-a-cone regresses to 0–7, training vs the live
opponent fixes it to 3–3"). That was a **single deterministic on-device match**, and the strong version
doesn't hold up: over 72 games **both** Q-Learn variants land well below Teach (cone 19%, live 36%), so
neither "fixes" anything. Worse, their **relative order is not stable** — small rule changes (kickoff
variance, the zap-swap, the swap's 180° turn) have flipped cone-vs-live more than once, so we claim it in
*neither* direction. The robust, repeatable finding is just **Q-Learn refinement here tended to hurt**;
the meta-lesson is that **one reproducible match isn't a representative eval — and even a 72-game eval
can't settle a close call between two weak options.** The detailed on-device log below is
kept as **history** (what we saw, once), not as measured law.

---

## The three teaching techniques

| Technique | What it is | Strength | Weakness |
|---|---|---|---|
| **Teach** (distillation / imitation) | Backprop a brain to **copy a scripted expert** (a hand-coded perfect dribbler / hunter). | **Sharp immediately** — competent after a couple seconds. | Capped at the expert; it imitates, it doesn't *out-think* a specific opponent. |
| **Evolve** (neuroevolution) | A population of brains plays; the best **breed + mutate**; repeat. No teacher. | Can discover things no expert was scripted for; adapts to a **specific opponent**. | **Slow from scratch**; on a *fixed* board it happily **overfits**. |
| **Q-Learn** (reinforcement) | One brain tries over and over; **reward** (a goal scored) reinforces good moves. | No teacher and no labels needed — just "did it score?". | Noisy; needs many episodes to be sharp. |

The interesting results came from **combining** them.

---

## Soccer: every combo we tried (AA's fighter vs BB's distilled striker)

We took one fixed strong opponent — **BB's Teach-distilled "fighter v2"** — and threw different
AA models at it in the deterministic Room. Same pitch, same opponent, same seed each time.

| AA's model | How it was trained | Rematch vs BB | Verdict |
|---|---|---|---|
| Over-Evolved (gen **256**, **fixed** board) | Evolve from scratch, one pitch, many generations | **broken** — *"just stays there"* | **Overfitting.** More generations on one board made it memorise that board and freeze elsewhere. |
| Varied-board Evolve (gen **128**) | Evolve from scratch, **ball moved every generation** | lost **1 – 7** | Generalises (it plays everywhere now, doesn't freeze) — but from-scratch evolution is still **weak**. |
| Teach (distill) | Imitate the expert dribbler | lost **4 – 5** | **Competent out of the box.** A one-goal game — distillation alone is already close. |
| Teach → **Q-Learn** *(stationary defender)* | Distill, then reward-refine vs a **cone** | lost **0 – 7** | **Regressed — refinement can HURT.** Trained against a static defender + a "score the ball" proxy, it overwrote the distilled policy and got *worse than Teach alone*. |
| Teach → **Q-Learn** *(LIVE opponent)* | Distill, then reward-refine while **running BB's actual brain** as a moving defender | **3 – 3** (lost on the tiebreak) | **Fixed by realism.** The same combo, against the *real* moving opponent, went from a 0–7 blowout to a dead heat — the refinement is competitive once it trains against what it'll actually face. |
| **Teach → Evolve vs BB** | Distill the expert, **then evolve *that* to beat BB** | **won 5 – 2** ✅ | **The winner.** Competent **+** adapted-to-the-opponent. |

Then we ran the **loser-levels-up loop**: BB saved AA's champion and trained the *same*
**Teach → Evolve vs AA** recipe to fight back.

| Matchup | Result |
|---|---|
| BB's `Teach → Evolve vs AA` **vs** AA's champion `fighter v4` | AA still won **5 – 2** |

So the recipe is strong, but **the first competent model held its ground** — BB couldn't overturn
it with the same approach. A bigger initial gap needs more than a like-for-like retrain.

### Follow-up: does *finishing* the Q-Learn matter? (more episodes)
The live-opponent Teach → Q-Learn run above was originally **saved mid-training** (~18 of 32
reward chunks). We re-ran it cleanly against a freshly-distilled striker and let **all 32 chunks
complete**, comparing it to a Teach-only brain on the identical pitch + opponent:

| AA model vs a distilled BB striker | Head-to-head |
|---|---|
| **Teach only** | **does not win** (a draw — "taught vs") |
| **Teach → Q-Learn, full 32 chunks vs the live striker** | **wins** ("taught wins!") |

**Letting the reward stage run to completion is what tipped it.** A half-finished Q-Learn pass
(the 18/32 save) is the worst of both worlds — it has perturbed the distilled policy without
finishing the refinement. The same recipe, **run to the end against the real moving opponent**,
turned a non-winning brain into a winner. *Episode count is not a footnote — a truncated reward
run can leave you mid-regression.* (Caveat: this is a clean re-creation with a new striker + seed,
so it complements rather than directly extends the **3–3** number above.)

---

## Battle (Sumo) arc — same loop, different outcome

| Stage | Result |
|---|---|
| Tournament 1 | **AA** beats BB |
| BB retrains (Evolve vs AA + Self) | — |
| Rematch | **BB wins — flipped** |

Battle *did* flip on a retrain where Soccer didn't — the battle skill gap was smaller, so a single
levelling-up pass was enough. **Different disciplines, different losers, different flip-ability.**

---

## The five things we actually learned

1. **Distillation beats evolution-from-scratch for getting competent fast.** Copying an expert
   (Teach) reached a one-goal game; evolving from random noise lost 1–7 in the same number of
   seconds. If a good "expert" exists, imitate it first.

2. **Evolving on a single fixed board overfits — visibly.** The gen-256 model literally *stayed
   still* in a real match: it had memorised one pitch so hard it was useless anywhere else. This is
   the classic over-fitting failure, and you can *watch* it.

3. **Varied training boards fix the over-fitting.** Moving the kickoff ball every generation forced
   the brain to play from anywhere — it stopped freezing. (Generalisation ≠ winning, though; it was
   robust but still weak.)

4. **Transfer learning wins — but only the *right* refinement. Teach → Evolve, not Teach →
   Q-Learn.** Distilling a competent striker and *then* evolving **that** brain against the specific
   opponent was the only AA model to beat BB (**5–2**). But distilling and then **Q-Learning** it
   made it *worse than Teach alone* (**0–7**). The difference is **alignment**:
   - **Evolve vs BB** selects directly for *"did you beat BB?"* — every generation pushes the
     competent brain toward the actual goal, so it compounds.
   - **Q-Learn** *does* sense the ball **and** the rival (it uses the full soccer senses, and the
     defender is a solid obstacle), but it optimises a **single-agent proxy reward** — "get the ball
     in the goal" — against a **stationary** defender. That's a *simpler world than the match*
     (Teach trains vs a **moving**, jittered rival). Its reward gradient **overwrote the distilled
     policy**, pulling the brain toward that narrower task — so it regressed. A refinement aimed at
     a simpler objective than your real one can *destroy* a good brain. The combo only compounds
     when the second stage pulls in the same direction as the first (Evolve-vs-the-actual-opponent
     does; score-the-ball-vs-a-cone doesn't).
   - **The fix was *realism*, not a different technique.** We made Q-Learn **run the real
     opponent's brain** as a moving defender (instead of a static cone). The *identical* Teach →
     Q-Learn recipe then went from **0–7 to 3–3** — a dead heat. **Training against what you'll
     actually face matters as much as which algorithm you pick.**
   (The enabler for *any* of this was making the trainer **seed from the current brain** by default
   instead of restarting from noise — so Teach → anything actually builds on the Teach.)

5. **Determinism makes it science.** Because the Room replays byte-identically across both boards,
   every result above is repeatable — you can re-run a matchup and get the same scoreline, which is
   exactly what let us compare techniques fairly.

---

## What changed in the trainer because of this

The findings reshaped the trainer's default behaviour:

- **Transfer learning is now the default.** **Teach**, **Evolve**, and **Q-Learn** all **build on
  the current brain** — Teach a dribbler then Evolve or Q-Learn to sharpen it, or load a saved
  fighter and refine it. Nothing throws your brain away behind your back.
- **Restarting is explicit.** A **"Fresh"** (scramble) button is the *one* way to deliberately wipe
  the brain back to random noise.
- **Save foes** copies an opponent's fighter into your library as **`<owner> <fighter>`** (e.g.
  `AA fighter v4`), so you can train against the bot that just beat you — the heart of the
  loser-levels-up loop.

### A note on a variant we *didn't* run
"Teach → Evolve" seeds the refinement from the **expert copy**, not from the **losing** brain.
Warm-starting from the *loser's* weights and transfer-learning them toward the expert is a distinct,
untested technique — a good next experiment now that all training seeds from the loaded brain.

---

## Caveats (so you can trust the numbers)

- Training depth was modest (seconds of on-device Teach / ~50–250 evolve generations) — these are
  *relative* comparisons, not absolute skill ceilings.
- All matches were driven through synthetic taps; we used distinct avatar colours (AA blue / BB red)
  and Field-picker checks so every champion was read unambiguously.
- "Win/loss" is one deterministic match per matchup. Because it's deterministic, one match *is* the
  answer for that exact pairing + seed — but a different seed could shift a close game.
