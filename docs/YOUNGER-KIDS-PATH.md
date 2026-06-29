# GridBot — Younger Kids Path (Ages 6–8)

### *The same five days, shrunk to one big idea a day, in 15-minute chunks a grown-up can run with no prep.*

This is the **gentle on-ramp** to GridBot for **ages ~6–8**. It's both a **prequel** (do it first, then graduate
to the full course when the kid is ready) and a **parallel path** (a calmer way through the same five days for a
kid who isn't ready for win-rate tables and trainer taxonomy).

It teaches the **exact same arc** as the grown-up materials — drive, think, compete, learn, champion — but each
day is **one big idea, one thing to say out loud, one win, and one optional grown-up explanation.** Nothing here
contradicts the full course; it just gets there in smaller steps.

- **Audience:** ages ~6–8, one kid per **$10 CYD** touchscreen (or sharing one with a grown-up driving).
- **A grown-up runs it.** No coding or AI experience needed — every day gives you the **sentence to say** and the
  **questions to ask**. You do not have to understand the network to run Day 4.
- **Time:** **15 minutes at a time.** Each day is a few **quest cards** of ~15 minutes. Do one, stop, come back.
  A "day" might take a week of short sittings — that's the point.
- **When to graduate:** when the kid starts asking *why* it works (not just *that* it worked), move them to the
  **[5-Day Curriculum](CURRICULUM.md)** or the all-in-one **[Complete Course](COURSE.md)**. The full course keeps
  every number, table, and CSTA/AI4K12 standard this path leaves out.

> **The one rule for grown-ups.** The robot almost always did **exactly what the program said** — even when the
> result looks wrong. So the fix is never "the robot is broken," it's "let's find the block that told it to do
> that." Say that a lot. It's the whole computational-thinking lesson hiding inside a kids' game.

---

## The week at a glance (younger version)

One big idea per day. The right-hand column is the grown-up stuff we **skip on purpose** at this age — it's all
waiting in [COURSE.md](COURSE.md)/[CURRICULUM.md](CURRICULUM.md) when they're ready.

| Day | Big idea (say this) | Younger Kid Version | Skip Until Later |
|---|---|---|---|
| **1** 🚗 | "The robot follows my list." | Make a list of moves, fix one wrong turn | CSTA terms, star-efficiency chasing |
| **2** 🧠 | "The robot can check before it moves." | Check wall, then turn | the formal word "algorithm", *why* it generalises |
| **3** 🧩 | "Rules need an order." | First matching rule wins | functions, win-rate tables |
| **4** 🤖 | "Training means trying and getting better." | Press **Teach**, watch it practise and improve | Teach vs Q-Learn vs Evolve taxonomy |
| **5** 🏆 | "We test which bot is best." | Try bots, keep the one that wins | statistics, "one match can fool us" caveats |

> **Powers unlock as you climb levels** — jump at **Lv 6**, loops at **Lv 10**, sensing (`if`/`until`) at **Lv 15**,
> functions at **Lv 20**, training at **Lv 28**. Younger kids climb slower, and that's fine: the **lessons**
> (Learn → CodeLab / NeuroLab) teach each idea on their own, so you never have to wait for a level to do the day.

---

## Day 1 — 🚗 Drive: *"The robot follows my list"*

**Win condition:** the kid makes the robot reach the goal on a level *by themselves*.

**Say it (one sentence):** *"The robot follows your list, one step at a time."*

### 15-minute quest cards
1. **Make it move.** Learn → **CodeLab → "1 · Move"**. Tap **Run**. Watch forward → turn → forward reach the goal.
2. **Make your own list.** Play **Level 1**. Drag blocks to make a path. Tap **Run**.
3. **Fix one mistake.** When it bonks, **change one block** and Run again. (Not the whole list — *one* block.)
4. **Use jump.** Around **Level 6**, a pit appears. Add a **jump** to hop it.

**Do (grown-up):** before each Run, have the kid **point along the path with a finger** where the robot will go.

**Ask:** *"Point where it will go… ready? Run."* and after a bonk: *"Which block told it to do that?"*

**Non-verbal checkpoint:** can the kid **trace the next move with a finger** before pressing Run?

> **Grown-up word:** a list of steps in order is a **sequence**. (You don't need to say this to the kid.)
> **Optional grown-up explanation:** order matters — `forward then turn` lands somewhere different from
> `turn then forward`. That's the first real idea in all of programming.

---

## Day 2 — 🧠 Think: *"The robot can check before it moves"*

**Win condition:** the kid builds a robot that **looks for a wall and turns** — and it works on a maze they've
never seen.

**Say it:** *"The robot can check first — 'is there a wall?' — and then decide."*

### 15-minute quest cards
1. **Use repeat.** Learn → **CodeLab → "3 · Repeat"**. Replace five forwards with one **repeat** block. Same path,
   shorter list.
2. **Make it react.** **CodeLab → "4 · Sense"**: build **"if wall, turn left, then forward"** inside a repeat.
3. **The magic trick.** Run that *same* robot on **a different maze**. It still finds its way! Don't change a block.
4. **Try a third maze.** Same robot, new maze. It works again.

**Do (grown-up):** when it solves the *second* maze untouched, make a big deal of it. *"You didn't change anything
and it still worked!"*

**Ask:** *"How did it know where to turn if we didn't tell it this maze?"* (Because it **checks the wall** each
step instead of memorising one path.)

**Non-verbal checkpoint:** can the kid make **one program work on two different mazes** without editing it?

> **Grown-up word:** a rule that works on mazes it's never seen is an **algorithm**. (Say "a rule that works again"
> with the kid.)
> **Optional grown-up explanation:** this is the leap from *memorising a path* to *a rule that adapts* — the most
> important idea in the whole course. Don't rush it; it's worth a whole week of 15-minute sittings.

---

## Day 3 — 🧩 Compete: *"Rules need an order"*

**Win condition:** the kid hand-codes an **Arena** bot (a fighter or a soccer bot) and wins a match — then sees
that **the order of the rules changes everything.**

**Say it:** *"When you have lots of rules, the robot checks them top to bottom. The first one that fits wins."*

### 15-minute quest cards
1. **Make a fighter.** Arena → build the **hunter**: *if the other bot is in front, **zap**; otherwise chase it.*
   Fight **vs Computer** and win.
2. **Break it on purpose.** Move **forward** to the **top** of the list. Watch it get dumb (it charges instead of
   zapping). *"Why did moving one block break it?"*
3. **Put it back.** Move the rules back to a good order. It's smart again.
4. **Make a soccer bot.** Build the **dribbler**: get **behind the ball** so a push sends it toward the *other*
   goal. (Good news: you **can't score on yourself** — if you shove it the wrong way, your robot **turns itself
   around** with the ball. You just have to get behind it.)

**Do (grown-up):** physically have the kid **read the rules top-to-bottom out loud** like the robot does.

**Ask:** *"Did the robot disobey, or did it follow your rules in the order you put them?"* (It obeyed — the
*order* was the bug.)

**Non-verbal checkpoint:** can the kid **find the one block** that's in the wrong place and move it?

> **Grown-up word:** the top-to-bottom rule is **priority** (or "precedence"). Functions (naming a group of
> blocks) live in the full course — skip them here.
> **Optional grown-up explanation:** hand-written rules are great when the rule is clear ("follow the wall"). Soccer
> needs *finesse* that's hard to write down — which is exactly why we train it tomorrow.

---

## Day 4 — 🤖 Learn: *"Training means trying and getting better"*

> **For ages 6–8, Day 4 is one idea: pressing a button makes the robot practise and get better.** That's it. The
> grown-up course splits training into **Teach / Q-Learn / Evolve** and measures each — **save all of that for
> older kids** (or treat it as an optional "boss level"). Here, we press **Teach**, watch, and cheer.

**Win condition:** the kid presses **Teach**, **watches the robot get better**, and beats yesterday's
hand-coded bot with the trained one.

**Say it:** *"Instead of writing every rule, we let the robot **practise** — and it gets better."*

### 15-minute quest cards
1. **Watch one neuron learn.** Learn → **NeuroLab → "One neuron"**: it guesses, sees it was wrong, and **nudges
   itself.** That nudge, a million times, is "training."
2. **Press Teach.** Arena → **Train a fighter → Soccer → Teach.** Watch the **learning curve** climb as it
   practises copying a good player.
3. **Before vs after.** Field the **trained** soccer bot against **yesterday's hand-coded one.** The trained one
   plays better.
4. **Name your bot and save it.** It's *theirs* now.

**Do (grown-up):** narrate the curve: *"See the line going up? That's it getting better at soccer while we watch."*

**Ask:** *"Did we write the rules for this one, or did it **learn** them by practising?"*

**Non-verbal checkpoint:** after watching both play, can the kid **point to the bot that learned** vs the one we
hand-wrote?

> **Grown-up word:** the robot's "brain" is a **neural network** — "a brain with knobs it can change." Training
> nudges the knobs.
> **Optional grown-up explanation:** some jobs are easy to *write* (follow the wall) and some are easier to *show*
> by example (dribble a ball). Day 4 is the kid's first taste of "let it learn the hard one."

---

## Day 5 — 🏆 Champion: *"We test which bot is best"*

**Win condition:** the kid runs a **tournament** and crowns a champion — and can say which bot they'd pick.

**Say it:** *"We don't guess which bot is best — we **play them** and find out."*

### 15-minute quest cards
1. **Make a striker.** Arena → **Train a fighter → Soccer → Teach**, then **Save** it.
2. **Lost a match? Train again.** If a bot beats yours, **save that bot**, train against **it**, and **rematch.**
   (The game replays matches *exactly*, so "better" really means better — not luck.)
3. **Run a tournament.** **vs Computer → Tournament** plays all your saved bots and crowns a winner.
4. **(Two devices) Class cup.** **Arena → Radio → Room** gathers every nearby robot into **one** tournament and
   every screen crowns the **same** champion.

**Do (grown-up):** let the kid **predict the winner first**, then run it and see if they were right.

**Ask:** *"Would you bring your hand-coded bot or your trained bot to the tournament? Why?"*

**Non-verbal checkpoint:** can the kid **choose between a hand-coded bot and a trained bot** after watching both,
and give a reason?

> **About "which is best":** keep it simple — *"This rule worked on lots of mazes," "The trained soccer bot
> usually wins," "One match can trick us, so we play a few."* The exact win-rate tables (Teach 20%, Teach→Evolve
> 47%, and so on) live in the **teacher layer** ([TRAINING_FINDINGS.md](TRAINING_FINDINGS.md) /
> [COURSE.md](COURSE.md)) — you don't need numbers to make the point at this age.

---

## 🛟 Parent Rescue Guide

Quick fixes for the moments that actually happen. The theme is always **shrink the problem and get a win.**

| If the kid… | Do this |
|---|---|
| …is **frustrated / melting down** | Go **back one level** and get a quick, easy win. End on a success. |
| …keeps **changing everything** at once | New rule: **change only one block**, then Run. Watching *one* change teaches cause-and-effect. |
| …**doesn't get "heading"** (which way it faces) | **Stand up and be the robot.** Walk it out — turn your whole body, then step. |
| …just wants to **mash buttons** | Require **"prediction before Run"**: they must point where it'll go *before* pressing Run. |
| …**loses interest** in the training day | Train a **funny/silly bot**, give it a goofy name, and run a quick tournament. Fun first. |
| …says **"the robot is broken / cheating"** | Read the blocks together top-to-bottom: it **obeyed** — find the block that said so. |
| …is **stuck on a maze** | Switch to the **lesson** (CodeLab/NeuroLab) for that idea; come back to the level after. |

---

## 🃏 "Say / Do / Ask" cards — one per day

Print these (or keep them on a phone). One card is everything a grown-up needs to run that day — no lesson
planning. Each card is a ~15-minute sitting; repeat the day across several sittings.

> **Day 1 — Drive**
> - **Say:** "The robot follows your list, one step at a time."
> - **Do:** Beat Levels 1–6. Point along the path before each Run.
> - **Ask:** "Which block told it to do that?"
> - **Celebrate:** "You made it follow your plan!"

> **Day 2 — Think**
> - **Say:** "The robot can check first, then decide."
> - **Do:** Build "if wall, turn." Run it on **two** different mazes unchanged.
> - **Ask:** "How did it know, if we didn't tell it this maze?"
> - **Celebrate:** "You wrote one rule that works everywhere!"

> **Day 3 — Compete**
> - **Say:** "The first rule that fits wins — so order matters."
> - **Do:** Win a match. Move one block to break it, then move it back.
> - **Ask:** "Did it disobey, or follow your rules in order?"
> - **Celebrate:** "You found the block in the wrong spot!"

> **Day 4 — Learn**
> - **Say:** "We let the robot practise, and it gets better."
> - **Do:** Press **Teach**, watch the curve climb, beat the hand-coded bot.
> - **Ask:** "Did we write the rules, or did it learn them?"
> - **Celebrate:** "You **trained** a robot!"

> **Day 5 — Champion**
> - **Say:** "We play the bots to find out which is best."
> - **Do:** Save a striker, run a Tournament, crown a champion.
> - **Ask:** "Which bot would you bring, and why?"
> - **Celebrate:** "You ran a whole tournament!"

---

## When you're ready for more

This path is deliberately small. Everything it skips — the *why* behind generalisation, functions, the
Teach/Q-Learn/Evolve trade-offs, the measured win-rates, and the CSTA/AI4K12 standards map — is waiting in:

- **[CURRICULUM.md](CURRICULUM.md)** — the same five days, one page, for ages ~7–13.
- **[COURSE.md](COURSE.md)** — the all-in-one guide a grown-up can teach straight from.
- **[HANDBOOK.md](HANDBOOK.md)** — a page per lesson, the full "why."

Same robot, same five days — just the next size up.
