# GridBot — The Grown-Up Guide

*A handbook for parents and teachers, side-by-side with the game.*

GridBot is a maze-coding game on a $10 touchscreen robot-brain (the CYD / ESP32). Kids start by **writing the rules** (drag-and-drop code) and graduate to **training the rules** (real neural networks that learn, on the device, with no internet). This guide helps the grown-up next to them turn screen time into understanding — whether you're a parent who's never coded or a teacher mapping to standards.

You do **not** need to know how to code or how AI works to use this. Each lesson below gives you a one-sentence **big idea**, the **questions to ask**, and the **one thing to watch for**. Teachers get extra notes (objective, time, standard, assessment). Everything is optional and self-paced.

> **This is a living document.** It ships in the same repository as the firmware, so when a screen changes, this guide changes with it. If something here doesn't match what's on the device, trust the device and file an issue.

---

## 1. The big picture — what is your kid actually learning?

Two ideas, taught as two parallel tracks the kid can feel the difference between:

| | **CodeLab** (symbolic AI) | **NeuroLab** (machine learning) |
|---|---|---|
| The deal | *You* write the rules | *You* train the rules from examples |
| Looks like | blocks: forward, repeat, if | a network that learns and changes |
| When it's great | clear, rule-followable problems | fuzzy "I know it when I see it" problems |
| The punchline | you can *read* exactly what it does | nobody hand-wrote it — it *learned* |

The genius of putting them next to each other is that kids meet the **real trade-off at the heart of modern AI**: write what's easy to write, *train* what's hard to write — and combine them. (That combination has a name, *neurosymbolic AI*, and it's literally how a self-driving car is built: hand-written rules for the clear stuff, learned neural nets for the judgment calls.)

### The spine: "How does a self-driving car work?"

If you want one throughline for the whole curriculum, use this. A simplified self-driving stack has these stages, and GridBot teaches most of them — here's the map (also a handy "what to play next"):

| Stage of self-driving | What it means | GridBot lesson |
|---|---|---|
| **See** (perception) | turn raw camera pixels into "wall / car / lane" | NeuroLab **Perception** |
| **Sense-from-the-car's-view** | everything is relative to *me* (ahead/left) | NeuroLab **Robot brain** |
| **Map & route** | where am I, which way to the goal | NeuroLab **Pilot** (planner) |
| **Drive** (control) | actually steer, moment to moment | NeuroLab **Pilot** (the brain) |
| **Remember** | hold what just went out of view | NeuroLab **Memory (RNN)** |
| **Learn from drivers** | copy expert examples; fix mistakes; retrain | NeuroLab **Data & labels** |
| **Mix rules + learning** | rules for the easy parts, a net for the rest | CodeLab **Brain** |

### Standards lens (for teachers): the AI4K12 "Five Big Ideas"

GridBot maps cleanly onto the de-facto US K-12 AI framework:

- **Perception** — *Perception* lesson, *Robot brain* (senses).
- **Representation & Reasoning** — CodeLab (symbolic rules), *Q-learning*, *Pilot* (search/planning).
- **Learning** — the heart of NeuroLab (backprop, evolution, transfer, data, RNN). GridBot is exceptionally strong here.
- **Natural Interaction** — lightly touched (the game *is* the interface).
- **Societal Impact** — the place to bring your own discussion (see *Talking about it* in §6).

CSTA mappings are noted per lesson and collected in §8.

---

## 2. How to use this guide

Each lesson page has two layers:

- **Parent layer** (everyone): *Big idea · On screen · Ask · Watch for · Words.* Read it in 60 seconds, sit down next to the kid, ask the questions.
- **Teacher layer** (the indented *For the classroom* block): objective, ~time, CSTA/AI4K12 tag, a discussion prompt, an unplugged extension, and a quick assessment.

Look for the **📡 Try together** boxes — optional two-player / whole-class activities using GridBot's built-in radio (see §7). Everything works solo too; radio is never required.

A fully-worked example of the complete teacher format is in **§5 (Sample lesson)**.

---

## 3. Quick start

1. **Flash it:** open the web flasher (`https://jamesdavid.github.io/cyd-GridBot/`) in Chrome/Edge, plug the CYD in by USB, click install. No app store, no account.
2. **Make a player:** tap *New Player*, name it, pick a colour. (Profiles are local to the device.)
3. **Find the lessons:** Home → **Learn** → **CodeLab** or **NeuroLab**. Lessons are always available — a kid can explore them before the matching game level unlocks.
4. **Play the campaign:** Home → **Play**. New powers unlock as the kid climbs (see §4). Each new power gets a gentle, guaranteed-win intro level first.

**No internet, no accounts, no data leaves the device.** The only networking is short-range radio between two CYDs in the same room (§7). This is a feature: it's safe for classrooms and it's a teachable contrast to cloud AI.

---

## 4. The learning curve (campaign progression)

Powers unlock on a deliberately compressed curve so the big payoff — *sensing* — arrives fast:

| Level | Unlocks | Badge to earn | Concept |
|---|---|---|---|
| 1 | Forward (guaranteed-win intro) | First Steps | sequence |
| 2–5 | Turn | Bright Spark (3 stars) | sequencing |
| **6** | **Jump** (intro level first) | Hopper | handling a special case |
| **10** | **Repeat** | Looper, Explorer (Lv10) | loops / efficiency |
| **15** | **Sense** (`if` / `until`) + **Arena** opens | Sixth Sense | conditionals, reacting |
| **20** | **Functions** | Architect, Veteran (Lv20) | abstraction, naming |
| **28** | **NeuroBot** (`+brain` block) | Brainiac, Mind Over Maze, Battle-Ready | machine learning |

The **"aha" moment** is Levels 15–22: a kid discovers that **one general program (a wall-follower) solves mazes it has never seen**. That's the jump from "memorising steps" to "an algorithm." Everything before builds the tools; everything after (Arena, NeuroBot) lets them use the tools creatively.

> **Generalist badge** (endgame): train *one* brain that clears a gauntlet of 10 fresh mazes it never saw. This is the kid-scale version of the single hardest idea in ML — *generalisation*.

---

## 5. Sample lesson (the full teacher format)

This is what a complete, classroom-ready lesson page looks like. The rest of the lessons (§6) use a condensed version of the same template.

### NeuroLab 1 — One neuron: *watch it learn*

**Big idea.** A single artificial "neuron" can **learn a rule from examples** by nudging numbers (its *weights*) until it stops making mistakes. Nobody programs the rule — it's discovered.

**On screen.** A 2-input neuron (sees *wall*, sees *pit*) wired to one output (*turn?*). Four example situations sit on the right. Tap **step** to run one round of learning, or **Train ×50** to fast-forward. Watch the connection lines change colour (green = push toward yes, red = push toward no) and thicken as the weights grow; watch the **loss** bar shrink to zero and the four examples flip from ✗ to ✓. When the loss is tiny it says **"learned!"** Tap **Reset** to scramble the weights and learn again from a different start.

**Ask.**
1. "Before you tap anything — is it any good? Why not?" *(random weights = random guesses.)*
2. "What is the bar at the bottom measuring? Which way do we want it to go?" *(loss = how wrong it still is; down.)*
3. "It learned the rule — but did anyone *tell* it the rule, or did it figure it out from the four examples?"
4. "Hit Reset. Does it always learn the same way?" *(different starting point, same destination — there's more than one set of weights that works.)*

**Watch for (misconception).** Kids (and adults) think the neuron is "told" the answer. It isn't told the *rule*; it's shown *examples* (situations + the right action) and it adjusts itself to fit them. That distinction — **rules vs. examples** — is the whole point of machine learning.

**Words.** *neuron* (a tiny decision-maker), *weight* (how much one input matters — the number that changes), *bias* (its baseline lean), *loss* (how wrong it still is), *training* (nudging the weights to lower the loss).

> **For the classroom**
> - **Objective:** explain that an ML model learns parameters from labelled examples to minimise error.
> - **Time:** 10–15 min. **Standards:** AI4K12 *Learning*; CSTA 2-AP-10 (decomposition isn't it — use **3A-AP-15**, "model phenomena," at HS, or **1B-AP-10** "patterns" for younger as a stretch).
> - **Do it:** project the device. Predict the loss curve before training. Train once. Reset and train again; note the weights differ but the behaviour matches.
> - **Discussion:** "If a neuron learns from examples, where do good examples come from? Who picks them?" (sets up the *Data & labels* lesson, and a values conversation about biased data).
> - **Unplugged extension:** "Guess the rule" — you secretly pick a rule ("clap if it's a fruit"); call out items; students adjust their guess each time. They *are* the neuron; each correction is one training step.
> - **Assessment (exit ticket):** *"In your own words, what's the difference between programming a rule and training one?"* Look for: training = learning from examples, no rule written by hand.

**📡 Try together.** Two devices: one student trains the neuron, Resets, and challenges a partner to predict how many ×50 rounds the *new* random start will need. (Friendly intuition-building about how starting point affects training.)

---

## 6. The lessons

Condensed pages. Same shape as the sample: **Big idea · On screen · Ask · Watch for · Words**, with a *For the classroom* line where it adds something.

### CodeLab (write the rules)

**1 · Move** — *forward & turn.*
- **Big idea:** a program is a **sequence** — steps run in order.
- **On screen:** tap **Run**, watch the robot follow forward/forward/turn/forward to the goal.
- **Ask:** "What happens if two steps swap places?" **Watch for:** order matters (forward-then-turn ≠ turn-then-forward). **Words:** *sequence, command.*

**2 · Jump** — *leap a pit.*
- **Big idea:** a special command for a special situation.
- **Ask:** "What if you jump when there's no pit? What if you forget to?" **Words:** *jump.* Unlocks Level 6.

**3 · Repeat** — *do steps again and again.*
- **Big idea:** a **loop** does the same steps N times so you don't write them N times.
- **Ask:** "How many forwards would this be without the loop? Why is the loop better than copy-paste?" **Watch for:** efficiency *and* readability, not just typing less. **Words:** *loop, repeat.* Unlocks Level 10.

**4 · Sense** — *react with `if` / `until`.*
- **Big idea:** the robot can **make a decision** based on what it senses ("if wall ahead, turn"). Tap the keyword chip to toggle **`if`** (react once) vs **`until`** (keep going until). This is the door to a *general* program.
- **Ask:** "This same program with no changes — could it solve a maze you've never seen? Why?" **Watch for:** the leap from a fixed list of moves to a rule that adapts. **Words:** *condition, if, until, sensor.* Unlocks Level 15 (and the Arena).
> **For the classroom:** this is the curriculum's hinge. The *wall-follower* (`until goal { if wall { turnL } forward }`) is the first real **algorithm** — it generalises. Have students test one program on three different mazes. CSTA 1B-AP-10 / 2-AP-10 (control structures).

**5 · Functions** — *name steps, call them.*
- **Big idea:** **abstraction** — bundle steps under a name and reuse it; the program gets shorter *and* clearer.
- **Ask:** "What would you name this group of steps? Where else could you call it?" **Words:** *function, call, abstraction.* Unlocks Level 20.

**6 · Brain** *(neurosymbolic)* — *code + a trained brain.*
- **Big idea:** you don't have to choose. Here a hand-written **rule jumps the pit** (easy to write) while a **trained brain drives** (hard to write). Two kinds of intelligence in one program.
- **On screen:** the program reads `until goal { if pit { jump } brain }`; tap Run and watch the split do the job.
- **Ask:** "Which part would be a pain to write as rules? Which part is obviously a rule?" **Watch for:** this is exactly how big AI systems are built (and how a self-driving car mixes rule-based safety checks with learned driving). **Words:** *neurosymbolic, brain block.*

**7 · Debug** — *find the bug, fix one thing.*
- **Big idea:** debugging is a skill: **read what went wrong → guess why → change one thing → test.**
- **On screen:** a program that turns the wrong way and misses the goal. Tap **Run** (it fails), then **Fix it** (one turn flips L↔R) and Run again (it solves).
- **Ask:** "Before you tap Fix it — what's your guess for the bug? How could you test your guess?" **Watch for:** change *one* thing at a time. **Words:** *bug, debug, hypothesis.*
> **For the classroom:** model "computational thinking" out loud — narrate the read→hypothesise→test loop. Best paired with any campaign level the kid is stuck on.

### NeuroLab (train the rules)

**1 · One neuron** — fully worked in **§5**.

**2 · Many actions** — *go / turn / jump.*
- **Big idea:** real choices aren't yes/no — the brain has **several outputs** and **picks the strongest** (that's *argmax*).
- **Ask:** "Three outputs light up a bit. How does it decide which one to actually do?" **Words:** *multi-class, output, argmax.*

**3 · Hidden layer** — *what one neuron can't.*
- **Big idea:** some rules (the classic **XOR**: "on if *exactly one* of A, B") are impossible for a single neuron — you need a **hidden layer** in between. Watch one neuron fail and a network with a hidden layer succeed.
- **Ask:** "It literally can't learn this with one neuron, no matter how long. Why might 'more layers' help?" **Watch for:** this is *the* reason deep learning is "deep." **Words:** *hidden layer, XOR, deep.*
> **For the classroom:** the most rigorous lesson — it *proves* a limitation, then *removes* it. AI4K12 *Learning / Representation*.

**4 · Robot brain** — *meet its senses.*
- **Big idea:** a tour of the *actual* brain every NeuroBot uses: **10 senses → 8 hidden → 5 actions**, and the senses are **relative to the robot** (ahead/right are signed: minus = behind/left).
- **Ask:** "Why does it sense 'goal to my *right*' instead of 'goal in the north-east'?" *(everything is from the robot's point of view — like a driver, not a map.)* **Watch for:** the same `10→8→5` brain solves mazes **and** fights in the Arena — one brain, many jobs. **Words:** *input, hidden, output, egocentric.*

**5 · Q-learning** — *learn from reward.*
- **Big idea:** no teacher, no code — the robot **tries the maze over and over** and value spreads back from the goal until it has a map of "good moves." It **explores** (tries things) then **exploits** (keeps the best).
- **Ask:** "Notice the colours spread *backwards* from the battery. Why would the tile next to the goal become 'good' first?" **Words:** *reward, reinforcement learning, explore vs exploit, policy.*

**6 · Evolution** — *breed the best.*
- **Big idea:** start with a crowd of random brains; the ones that get furthest **breed**; repeat. No teacher, no gradient — just survival.
- **Ask:** "Nothing is being 'taught' here. How does it still get better?" **Words:** *evolution, fitness, selection, mutation.*

**7 · Transfer** — *reuse skills, new maze.*
- **Big idea:** train on maze A, then on a **brand-new maze B it already does okay** — it learned *general* skills, not that one maze. A quick fine-tune masters B. (And it watches for the opposite failure — **over-fitting**, memorising one maze.)
- **Ask:** "How can it be partway through a maze it has *never seen*?" **Words:** *transfer, fine-tune, generalise, over-fit.*

**8 · Brain Cam** — *watch a brain think.*
- **Big idea:** an X-ray of a live network. Tap **Teach** and watch **backprop animate** — the weight-lines recolour as it learns, the loss bar falls. Tap any **neuron to zoom** into its weights. Flip **plain ↔ rnn**, change the **map** (it transfers), and **Save** a trained brain to your library.
- **Ask:** "Tap a neuron. What do those numbers mean? Watch them change while it learns." **Words:** *backprop, activation, weight.* The clearest answer to "what is it actually doing?"

**9 · Pilot** — *plan + steer (the self-driving split).*
- **Big idea:** a reactive brain only senses what's nearby, so it gets stuck. Add a **route planner** that lays down waypoints; now the brain just **steers dot-to-dot** and solves it. **The planner decides *where*; the brain decides *how*** — exactly how a self-driving stack splits the *map route* from the *neural net that handles the road*.
- **Ask:** "Which job is the map doing? Which job is the brain doing? Why can't either do it alone?" **Words:** *planner, controller, waypoint, hierarchy.*

**10 · Memory (RNN)** — *a brain that remembers.*
- **Big idea:** a plain brain decides from *senses now*, so in a dead-end it forgets it's been there and **loops**. A **recurrent** brain feeds its hidden layer **back into itself** (memory), so it remembers its trail and **backs out**. The only change is one feedback path.
- **Ask:** "Same maze, same teacher — why does only the one with memory escape?" **Words:** *memory, recurrent, state.* (Pilot added an *outside* planner; the RNN grows memory *inside* the brain.)

**11 · Perception** — *raw squares → meaning.*
- **Big idea:** every other lesson **hands** the brain clean senses ("wall ahead = 1"). But where do those come from? A **perception net looks at the raw squares** the robot can see and *decides* "wall ahead? yes/no." Turning raw input into meaning is the part real robots spend the most effort on.
- **On screen:** the robot's-eye view (raw squares ahead/left/right) feeding a little net that lights up the answer; tap **Next view** to try more.
- **Ask:** "The robot just sees coloured squares. How does it get from *squares* to the word *'wall'*?" **Watch for:** in a self-driving car this is the hard 90% — cameras give pixels, a net turns them into "car / lane / person." **Words:** *perception, raw input, feature.*

**12 · Data & labels** — *learn from examples.*
- **Big idea:** **Teach** doesn't invent the right move — it **copies an expert**. Each example = *what it saw* + *the right move* (a **label**). The loop: run → find where it **failed** → add examples there → train again. More and better examples = a smarter brain. **The data is the real work.**
- **On screen:** an example counter ticks up as it learns; a new maze exposes situations it never saw; adding examples there fixes it.
- **Ask:** "Where did the 'right answers' come from? What happens if the examples are bad or unfair?" **Watch for:** this is *imitation learning* and the *data engine* — and the doorway to a values conversation about biased data. **Words:** *data, label, example, imitation.*

### Arena — *put a trained brain to work*

Not a lesson, but where training pays off. **Home → Arena → vs Computer → Train a fighter.** Teach or Evolve a brain to win matches, sparring up a difficulty ladder (**Bolt → Coil → Spin → Vex → Ace**), **Save** it, then pick it as **your bot** in a Race. It even fine-tunes to each new battle board so it actually wins.
- **Ask:** "You trained a brain to solve mazes — now it's fighting. Same brain, different job. How?"

### Talking about it (the Societal Impact strand)

GridBot teaches *how* AI works; the *should* is your conversation. Natural prompts, tied to lessons:
- After **Data & labels:** "If the examples were unfair, would the brain be unfair? Who's responsible for the examples?"
- After **Pilot / Memory:** "A self-driving car uses exactly these pieces. What should it do when it's *not sure*? Who's responsible if it's wrong?"
- After **Brain Cam:** "We can watch every weight here. Real AIs have *billions*. Is it okay to use something nobody can fully read?"

---

## 7. Going multiplayer — the radio (ESP-NOW)

Two or more CYDs in the same room can trade and battle bots over short-range radio — **no internet, no accounts, nothing leaves the room.** That privacy is exactly why it's classroom-friendly, and it's a great teachable contrast to cloud AI. It's always **optional**; everything works on one device.

What the radio sends: a whole bot — its program **and its trained brain** (including recurrent/memory brains). A bot you receive joins your library as *your bot* to battle or train against.

**Classroom / two-player activities** (drop these in as **📡 Try together** moments):
- **Teacher "boss bot."** The teacher trains a NeuroBot and sends it to everyone; each student must train one that beats it. Instant, self-levelling challenge — and it makes *train → save → battle* purposeful.
- **Starter-brain handout.** Teacher sends a half-trained brain; students **fine-tune** it to their own maze. This is the *Transfer / Data* lesson made social ("you inherited the teacher's skills — now generalise").
- **Trade & battle.** Students swap brains and race/sumo them. Comparing a friend's bot to yours is the best debugging prompt there is.
- **Class tournament.** A bracket of student-trained fighters — the natural capstone after the fighter trainer.
- **Fleet learning (Data lesson tie-in).** Several students each contribute examples/brains → the honest, kid-scale version of "more, varied data = a smarter brain," with zero internet.
- **Collect & assess.** The teacher pulls students' bots in to review who used what (the Stats screen tracks brains-trained / fighters-saved / win rate).

**Setup card:** two CYDs powered on, in the same room → **Arena → Radio** (or the trade prompt). Keep them close. If a transfer stalls, move them nearer and retry. No pairing codes, no Wi-Fi, no setup for IT to approve.

---

## 8. Appendix A — Honest simplifications

So you can answer "is this *really* how it works?" with confidence. GridBot is true in spirit and simplifies for clarity. Where it bends:

- **The senses are handed over (mostly).** Outside the *Perception* lesson, the brain receives clean numbers, not pixels. Real perception is much harder and is the bulk of real-robot effort — which is the whole reason the *Perception* lesson exists.
- **The brains are tiny.** `10→8→5` (a few hundred numbers). Real models have millions to billions. The *mechanism* (weights, layers, backprop, argmax) is genuine; the *scale* is not.
- **Training takes seconds, not months.** Mazes are small and the "expert" is a perfect maze-solver, so learning is fast. Real training is enormous and noisy.
- **"Teach" copies a perfect solver.** That's real *imitation learning / distillation*, but real experts (human drivers) are imperfect and inconsistent.
- **No real physics or sensors.** A grid, not a road; no slip, weather, or sensor noise.
- **The math is hidden on purpose.** No equations, gradients, or learning-rate knobs are exposed — appropriate for the age, but it means kids don't yet "own" those levers. (A great direction for older students.)

None of these make the lessons *wrong* — they make them *legible*. Saying them out loud builds trust and is itself good science.

## 9. Appendix B — Standards map

| GridBot | AI4K12 Big Idea | CSTA (illustrative) |
|---|---|---|
| CodeLab Move/Jump/Repeat | — | 1A-AP-10, 1B-AP-10 (sequence, loops) |
| CodeLab Sense (`if`/`until`) | Representation & Reasoning | 1B-AP-10, 2-AP-10 (control structures) |
| CodeLab Functions | — | 2-AP-14 (modularity / abstraction) |
| CodeLab Brain (neurosymbolic) | Learning + Reasoning | 3B-AP-08 (libraries / composing systems) |
| CodeLab Debug | — | 2-AP-17, 3A-AP-21 (testing & debugging) |
| NeuroLab One neuron / Many actions / Hidden layer | Learning | 3A-AP-15 (models); AI4K12 LO "ML" |
| NeuroLab Robot brain / Perception | Perception | AI4K12 "Computers perceive via sensors" |
| NeuroLab Q-learning / Evolution | Learning | AI4K12 "Learning from data/experience" |
| NeuroLab Transfer / Data & labels | Learning | AI4K12 "training data"; data-ethics LOs |
| NeuroLab Pilot / Memory | Representation & Reasoning + Learning | AI4K12 "reasoning & search" |
| *Talking about it* prompts | Societal Impact | 2-IC-20, 3A-IC-24 (impacts, bias) |

*(CSTA codes are guidance, not certification — adapt to your grade band.)*

## 10. Appendix C — Glossary (two columns)

| Word | For a kid | For a grown-up |
|---|---|---|
| **algorithm** | a set of steps that always works | a general procedure, not a fixed answer |
| **loop** | do it again and again | iteration / control structure |
| **condition** | "if this, then that" | branching on sensor state |
| **function** | steps you name and reuse | named, reusable sub-routine (abstraction) |
| **neuron** | a tiny decision-maker | a weighted sum + nonlinearity |
| **weight** | how much an input matters | a learned parameter |
| **bias** | its baseline lean | the constant term |
| **loss** | how wrong it still is | the error being minimised |
| **training** | nudging it until it's right | gradient descent on the loss |
| **backprop** | how it figures out which knob to turn | backpropagation of error gradients |
| **hidden layer** | the in-between thinking | intermediate representation (enables non-linear) |
| **argmax** | pick the strongest choice | take the highest-scoring output |
| **reward** | points for doing well | the RL objective signal |
| **policy** | its plan for what to do | learned action-selection mapping |
| **transfer** | reuse skills on something new | fine-tuning a pre-trained model |
| **generalise** | works on stuff it never saw | performs on held-out data |
| **over-fit** | memorised, can't handle new | fits training data, fails to generalise |
| **perception** | turning what it sees into meaning | mapping raw input → features/labels |
| **label** | the right answer for an example | the target in supervised learning |
| **recurrent (RNN)** | a brain with memory | network with hidden-state feedback |
| **neurosymbolic** | rules + a trained brain together | hybrid symbolic + learned system |

---

*Made to sit next to the game. Questions, corrections, or a lesson that didn't land? That's data — file it.*
