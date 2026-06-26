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
| **28** | **NeuroBot** (`+brain` block) + **Teach** | Brainiac, Mind Over Maze, Battle-Ready | machine learning |
| **31** | **Draw & tag** (label a path, train to it) | — | supervised data / labels |
| **34** | **Evolve** (neuroevolution) | — | learning without a teacher |
| **37** | **Pilot** (planner + follower) | — | search + control |
| **40** | **Memory brain** (RNN) | — | sequence memory |

The **"aha" moment** is Levels 15–22: a kid discovers that **one general program (a wall-follower) solves mazes it has never seen**. That's the jump from "memorising steps" to "an algorithm." Everything before builds the tools; everything after (Arena, NeuroBot) lets them use the tools creatively.

The NeuroBot **training tools also unlock one at a time** (28–40) rather than all at graduation, so each new way to train a brain arrives with its own lesson (surfaced by a **"Learn it"** link on the level intro) instead of a wall of buttons. The lessons themselves stay open in **Learn** from day one for kids who want to run ahead.

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

> **📘 Deep dive — the [Hand-Coding Guide](HAND-CODING-GUIDE.md).** *The* bridge from CodeLab to
> NeuroLab. Have students hand-code a bot for each Arena game and play it against a trained brain —
> the measured punchline *is* the "match the method" lesson (NeuroLab 17): **Maze & Battle → hand-coding
> wins** (a 3-rule wall-follower solves **8× more unseen mazes** than a one-maze brain; the priority-rule
> hunter goes **9-5-2** vs trained fighters), **Soccer → learning wins** (the best hand-coded dribbler
> only *draws* the trained striker; finishing finesse needs practice). Kids earn NeuroLab by feeling
> exactly where rules run out — then open it knowing *what* the brain is trying to learn. Every number
> is reproducible from `tools/bot_eval.cpp`.

### NeuroLab (train the rules)

**1 · One neuron** — *watch it learn.* Fully worked in **§5**.

**2 · Backprop, step by step** — *learn one nudge at a time.*
- **Big idea:** training isn't magic — it's a tiny loop: **LOOK** (the neuron guesses) → **SCORE** (how wrong) → **BLAME** (which weights to nudge, and why an OFF input *doesn't* change) → **NUDGE** (apply it; the guess gets closer). One example per "Next step", training a real neuron by hand.
- **Ask:** "Why didn't *that* weight change?" *(its input was OFF — only inputs that were ON get the blame.)* "Did the guess get closer or further after the nudge?" **Watch for:** this is the *mechanism* under every "Teach"/Train button in the game. **Words:** *gradient, nudge, learning rate.* (Companion to CodeLab's *Debug* — same do-one-thing-and-look rhythm.)

**3 · Perception** — *the senses we hand the brain.*
- **Big idea:** every other lesson **hands** the brain clean senses ("wall ahead = 1"). This shows where they come from: the robot's **immediate surroundings** (the tiles right next to it) turned into the numbers that *are* the brain's **input layer** — `wall ahead`, `wall left/right`, `pit ahead`, `goal bearing`. The honest catch: in this game we just **hand** the brain those numbers. A **real** robot would need a **camera and sensors** to work them out from the world — *that's* perception, the hard part real robots spend the most effort on.
- **On screen:** the surroundings on the left, the sense values they produce dropping into the brain's input layer on the right; tap **Next view** to cycle situations (corridor, wall ahead, pit ahead, goal ahead…). Later phases note that **choosing which things to sense is an engineering decision**, and that these are the robot's **local** surroundings — *not a top-down map of the whole world.*
- **Ask:** "In real life, how would a robot *know* there's a wall ahead — who tells it?" / "Why does it only sense what's right next to it, instead of the whole map?" **Watch for:** in a self-driving car this is the hard 90% — the world doesn't hand you "wall ahead = 1"; a camera + perception net has to produce it. **Words:** *perception, sensor, input layer, egocentric (local) vs. global.*

**4 · Hidden layer** — *what one neuron can't.*
- **Big idea:** some rules are impossible for a single neuron — you need a **hidden layer** in between. The lesson uses a real one: **"turn at a corner" — turn when *exactly one* side (left **or** right) is open**, but go straight in a corridor (both walls) *or* a junction (both open). One neuron can't learn that; a hidden layer can. *(This is the classic **XOR** problem — the same shape — but framed as something the bot actually does.)*
- **Ask:** "It literally can't learn this with one neuron, no matter how long. Why might 'more layers' help?" **Watch for:** this is *the* reason deep learning is "deep." **Words:** *hidden layer, non-linear, deep.*
> **For the classroom:** the most rigorous lesson — it *proves* a limitation, then *removes* it. AI4K12 *Learning / Representation*.

**5 · Many actions** — *go / turn / jump.*
- **Big idea:** real choices aren't yes/no — the brain has **several outputs** and **picks the strongest** (that's *argmax*).
- **Ask:** "Three outputs light up a bit. How does it decide which one to actually do?" **Words:** *multi-class, output, argmax.*

**6 · Robot brain** — *meet its senses.*
- **Big idea:** a tour of the *actual* brain every NeuroBot uses: **10 senses → 8 hidden → 5 actions**, and the senses are **relative to the robot** (ahead/right are signed: minus = behind/left).
- **Ask:** "Why does it sense 'goal to my *right*' instead of 'goal in the north-east'?" *(everything is from the robot's point of view — like a driver, not a map.)* **Watch for:** the same `10→8→5` brain solves mazes **and** fights in the Arena — one brain, many jobs. **Words:** *input, hidden, output, egocentric.*

**7 · Data & labels** — *learn from examples.*
- **Big idea:** **Teach** doesn't invent the right move — it **copies an expert**. Each example = *what it saw* + *the right move* (a **label**). The loop: run → find where it **failed** → add examples there → train again. More and better examples = a smarter brain. **The data is the real work.**
- **On screen:** an example counter ticks up as it learns; a new maze exposes situations it never saw; adding examples there fixes it.
- **Ask:** "Where did the 'right answers' come from? What happens if the examples are bad or unfair?" **Watch for:** this is *imitation learning* and the *data engine* — and the doorway to a values conversation about biased data. **Words:** *data, label, example, imitation.*

**8 · Q-learning** — *learn from reward.*
- **Big idea:** no teacher, no code — the robot **tries the maze over and over** and value spreads back from the goal until it has a map of "good moves." It **explores** (tries things) then **exploits** (keeps the best).
- **Ask:** "Notice the colours spread *backwards* from the battery. Why would the tile next to the goal become 'good' first?" **Words:** *reward, reinforcement learning, explore vs exploit, policy.*

**9 · Tuning the grid** — *turn the knobs, change how it learns.*
- **Big idea:** learning has **dials**, and the dial settings matter as much as the method. Two knobs sit on the Q-learning heatmap: **explore (ε)** — how often it tries something random instead of the best-known move — and **step (α)** — how big a nudge each lesson makes. Set explore to **0** and it gets **stuck** repeating one path and never finds the goal; turn explore **up** and value suddenly spreads and it solves. This is *hyperparameter tuning* — the part of ML that's more cooking than math.
- **On screen:** the same Q heatmap, now with ▲/▼ steppers for ε and α; nudge them, hit reset/run, watch the colours spread faster, slower, or not at all.
- **Ask:** "With explore at zero it never solves — why would *never trying anything new* trap it?" / "Turn the step knob way up. Does faster always mean better, or does it get jittery?" **Watch for:** there's a *sweet spot* — too little exploring and it's stuck, too much and it never settles; too small a step and it crawls, too big and it overshoots. **Words:** *hyperparameter, explore/exploit (ε), learning rate (α), tuning.*
> **For the classroom:** this is the lesson that exposes the levers Appendix A says are usually hidden. Have students *predict* the effect of each knob before turning it, then test. AI4K12 *Learning*; great bridge to "why does training real AI take so much trial and error?"

**10 · Tuning a real net** — *the same knobs, on a real brain.*
- **Big idea:** *Tuning the grid* turned the dials on a Q table; this turns them on a **real neural network doing backprop**. The net learns the "turn at a corner" task (the Hidden-layer job) while you watch the **loss curve**: with a good **learn rate** it slides smoothly to zero; **too low** and it barely moves (crawls); **too high** and the curve goes red and *won't settle* (thrashes). **Rounds** = how long it trains.
- **On screen:** a learn-rate ladder + a rounds knob, the loss curve, and the four corner cases flipping to ✓ as it learns. The same three knobs live in the Arena trainer's **"Knobs"** panel, where they tune a real fighter.
- **Ask:** "Watch the loss with a tiny learn rate, then a huge one. Which actually learns — and why is *bigger not always better*?" **Watch for:** there's a sweet spot; too-big steps overshoot and never settle. **Words:** *learning rate, loss, epoch, converge, overshoot.*
> **For the classroom:** the backprop twin of the *Tuning the grid* lesson — together they show that *every* learning method has knobs. Pair with the Arena **Knobs** panel so students feel the same dials change a real bot. AI4K12 *Learning*.

**11 · Evolution** — *breed the best (robot babies).*
- **Big idea:** start with a crowd of random brains; the ones that get furthest **breed**; repeat. No teacher, no gradient — just survival of the fittest.
- **On screen:** the population evolving on the maze + a climbing fitness curve. A **"How?"** button opens a breeding explainer: the population ranked by **fitness** (the top few glow — that's **selection**), a two-parents-→-baby diagram (**crossover** — "mix two winners' DNA"), and **mutation** (random tweaks). The headline: *the best robots make robot babies.*
- **Ask:** "Nothing is being 'taught' here. How does it still get better?" / "Why take **two** parents instead of just copying the best one?" **Words:** *evolution, population, fitness, selection, crossover, mutation.*

**12 · Transfer** — *reuse skills, new maze.*
- **Big idea:** train on maze A, then on a **brand-new maze B it already does okay** — it learned *general* skills, not that one maze. A quick fine-tune masters B. (And it watches for the opposite failure — **over-fitting**, memorising one maze.)
- **Ask:** "How can it be partway through a maze it has *never seen*?" **Words:** *transfer, fine-tune, generalise, over-fit.*

**13 · Brain Cam** — *watch a brain think.*
- **Big idea:** an X-ray of a live network. Tap **Teach** and watch **backprop animate** — the weight-lines recolour as it learns, the loss bar falls. Tap any **neuron to zoom** into its weights. Flip **plain ↔ rnn**, change the **map** (it transfers), and **Save** a trained brain to your library.
- **Ask:** "Tap a neuron. What do those numbers mean? Watch them change while it learns." A caption notes the **argmax**: the brain has five outputs and simply **does the strongest one**. **Words:** *backprop, activation, weight, argmax.* The clearest answer to "what is it actually doing?"

**14 · Pilot** — *plan + steer (the self-driving split).*
- **Big idea:** a reactive brain only senses what's nearby, so it gets stuck. Add a **route planner** that lays down waypoints; now the brain just **steers dot-to-dot** and solves it. **The planner decides *where*; the brain decides *how*** — exactly how a self-driving stack splits the *map route* from the *neural net that handles the road*.
- **Ask:** "Which job is the map doing? Which job is the brain doing? Why can't either do it alone?" **Words:** *planner, controller, waypoint, hierarchy.* *(In the **Train a fighter** Pilot trainer the kid can even **hand-place the waypoints** — tap tiles to draw the route the brain then learns to follow.)*

**15 · Memory (RNN)** — *a brain that remembers.*
- **Big idea:** a plain brain decides from *senses now*, so in a dead-end it forgets it's been there and **loops**. A **recurrent** brain feeds its hidden layer **back into itself** (memory), so it remembers its trail and **backs out**. The only change is one feedback path.
- **Ask:** "Same maze, same teacher — why does only the one with memory escape?" **Words:** *memory, recurrent, state.* (Pilot added an *outside* planner; the RNN grows memory *inside* the brain.)

**16 · Self-play** — *get better by beating yourself.*
- **Big idea:** how do you train a champion when there's **no expert to copy and no perfect answer key** — just "did I win?" You make the brain **fight a frozen copy of itself**. Every time a challenger out-fights the current champion, it **takes the crown**, and the *next* challenger has to beat that tougher version. The opponent keeps getting harder *because you keep getting better* — an **arms race** with no teacher at all. (This is the core trick behind AlphaGo / AlphaZero.)
- **On screen:** an **"upgrades" meter** climbs each time a new best dethrones the champ; sometimes "champ held on" — the bar that *doesn't* move is the point: the bar got harder to move.
- **Ask:** "There's no teacher and no answer key here — so what is it learning *from*?" / "Why does it get harder to earn an upgrade the longer it runs?" **Watch for:** the opponent isn't fixed — it's *you, a moment ago*. Beating a moving target is what makes it strong. **Words:** *self-play, champion, arms race, co-evolution.*
> **For the classroom:** contrast directly with *Data & labels* (imitation needs an expert) and *Q-learning* (reward needs a goal). Self-play needs **neither a teacher nor a hand-set reward** — only an opponent, which it generates itself. AI4K12 *Learning*. Pairs with the **Self** sparring partner in the Arena trainer.

**17 · The Right Tool** — *match the method to the problem (capstone).*
- **Big idea:** the whole curriculum in one question: **there is no single best method — there's a best method *for this problem.*** A 4-question quiz puts the choices side by side — *memory* for a maze you have to remember, a *plain reflex* brain for a fast battle, *Teach* when you already know the right answer, *Q-learn* when all you have is win/lose. Picking the wrong tool (memory for a reflex fight, a teacher when you have no answer key) is how real ML projects go wrong.
- **On screen:** four scenarios; tap the method you'd use; instant green/red with a one-line *why*.
- **Ask:** "Why would *memory* actually **hurt** a fast reflex battle?" *(it's slower and the past doesn't matter — extra baggage.)* / "When is there simply no expert to copy, so Teach can't work?" **Watch for:** this ties the knot between *Q-learning*, *Memory*, *Data*, and *Self-play* — each was right *somewhere*. **Words:** *inductive bias, fit-for-purpose, when-methods-fail.*
> **For the classroom:** the natural end-of-unit assessment. Have students justify each answer aloud — the *reasoning* is the learning, not the colour. AI4K12 *Learning / Reasoning*. A great "what did we actually learn?" wrap-up.

### Arena — *put a trained brain to work*

Not a lesson, but where training pays off. **Home → Arena → Train a fighter.** **Teach**, **Q-Learn**, or **Evolve** a brain (with an optional **Memory/RNN** toggle) to win matches, sparring up a difficulty ladder (**Bolt → Coil → Spin → Vex → Ace → Self**), watching a live **learning-curve** sparkline, then **Save** it and pick it as **your bot** in a **Race**, **Soccer**, or **Battle** (Sumo). It even fine-tunes to each new board so it actually wins. The last rung, **Self**, is the *Self-play* lesson in action — it spars against an evolving copy of itself.
- **Ask:** "You trained a brain to solve mazes — now it's fighting. Same brain, different job. How?"
- **Soccer — the same brain, a new game.** A walled pitch with a goal at each end and a ball in the middle: step on the ball to **shove it one tile**, dribble it into the opponent's net to score, and a live scoreline counts the goals in a timed match. It's the *same* `10 → 8 → 5` brain — it just senses the **ball** as its objective and the **goal** and **rival** as bearings — so you can **Teach / Q-Learn / Evolve** a soccer bot, or **hand-code** one with the `ball-ahead` / `net-left` sense blocks (and **and/or** conditions, e.g. *if ball-ahead **and** net-left*). A pre-trained **house team** (Strika, Dribbla, Volley, Nutmeg, Boots) gives a beatable first match. Great for the "one brain, many jobs" point: a maze brain, a fighter, and a striker are the **same network shape** trained on different objectives.
  - **Ask:** "It learned mazes, then fighting, now soccer — what stays the same about its brain, and what changes?" *(the network is identical; only what it senses as the 'goal' and how it's rewarded changes.)*
- **Tournaments.** **vs Computer → Tournament** runs a field of your saved fighters as either a **Cup** (single-elimination bracket) or a **Ladder** (round-robin), and you pick the **discipline** — Race, Soccer, or Battle. Every match is deterministic and replayed on screen, so the bracket is real, not random. The natural capstone after kids have trained a few fighters; pair it with the radio **class tournament** in §7.
- **Knobs (optional, advanced).** A **"Knobs"** button hides three real hyperparameters — **learning rate**, **rounds**, and **explore** (mutation for Evolve, exploration for Q-Learn). They're tucked away so they're never in a beginner's path, but they let a curious student feel a setting change a *real* fighter's training — the hands-on partner to the **Tuning a real net** lesson.

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
- **Trade & battle.** Students swap brains and race, play soccer, or sumo them. Comparing a friend's bot to yours is the best debugging prompt there is.
- **Class tournament (the Room).** **Arena → Radio → Room** gathers *every* nearby device's fighter into one shared bracket; the host picks the **discipline** (Race, Soccer, or Battle) and starts it, and each board replays the *same* matches from a shared seed — so every screen shows the identical Cup and crowns the same champion, no central scoreboard needed. Verified on two boards; bring more for a real class bracket. The natural capstone after the fighter trainer.
- **Fleet learning (Data lesson tie-in).** Several students each contribute examples/brains → the honest, kid-scale version of "more, varied data = a smarter brain," with zero internet.
- **Collect & assess.** The teacher pulls students' bots in to review who used what (the Stats screen tracks brains-trained / fighters-saved / win rate).

**Setup card:** two CYDs powered on, in the same room → **Arena → Radio** (or the trade prompt). Keep them close. If a transfer stalls, move them nearer and retry. No pairing codes, no Wi-Fi, no setup for IT to approve.

**Nametag screensaver (classroom nicety).** Leave a device untouched on a menu for a minute and it turns into a big **name card** — the player's robot avatar, name, and star count — so a shared device shows whose turn it is at a glance. Any touch wakes it back to where it was. It only appears on calm menu screens, never mid-game or mid-training.

---

## 8. Appendix A — Honest simplifications

So you can answer "is this *really* how it works?" with confidence. GridBot is true in spirit and simplifies for clarity. Where it bends:

- **The senses are handed over (mostly).** Outside the *Perception* lesson, the brain receives clean numbers, not pixels. Real perception is much harder and is the bulk of real-robot effort — which is the whole reason the *Perception* lesson exists.
- **The brains are tiny.** `10→8→5` (a few hundred numbers). Real models have millions to billions. The *mechanism* (weights, layers, backprop, argmax) is genuine; the *scale* is not.
- **Training takes seconds, not months.** Mazes are small and the "expert" is a perfect maze-solver, so learning is fast. Real training is enormous and noisy.
- **"Teach" copies a perfect solver.** That's real *imitation learning / distillation*, but real experts (human drivers) are imperfect and inconsistent.
- **No real physics or sensors.** A grid, not a road; no slip, weather, or sensor noise.
- **The math is mostly hidden on purpose.** No raw equations or gradients are shown — appropriate for the age. The *knobs* themselves, though, are now exposed where they teach something: the **Tuning the grid** / **Tuning a real net** lessons and the Arena trainer's optional **Knobs** panel let older kids turn learning rate, explore, and rounds and watch the effect. (The underlying calculus stays hidden.)

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
| NeuroLab Q-learning / Tuning the grid / Tuning a real net / Evolution | Learning | AI4K12 "Learning from data/experience" |
| NeuroLab Transfer / Data & labels | Learning | AI4K12 "training data"; data-ethics LOs |
| NeuroLab Pilot / Memory | Representation & Reasoning + Learning | AI4K12 "reasoning & search" |
| NeuroLab Self-play / The Right Tool | Learning | AI4K12 "Learning"; method selection / inductive bias |
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
| **hyperparameter** | a knob you set before training (how much to explore, how big a step) | a setting that controls learning, not learned by it |
| **self-play** | get better by beating an old copy of yourself | training against a frozen/co-evolving version of the agent |
| **crossover** | two parents make a baby that mixes their DNA | recombining two parents' parameters to form an offspring |
| **transfer** | reuse skills on something new | fine-tuning a pre-trained model |
| **generalise** | works on stuff it never saw | performs on held-out data |
| **over-fit** | memorised, can't handle new | fits training data, fails to generalise |
| **perception** | turning what it sees into meaning | mapping raw input → features/labels |
| **label** | the right answer for an example | the target in supervised learning |
| **recurrent (RNN)** | a brain with memory | network with hidden-state feedback |
| **neurosymbolic** | rules + a trained brain together | hybrid symbolic + learned system |

---

*Made to sit next to the game. Questions, corrections, or a lesson that didn't land? That's data — file it.*
