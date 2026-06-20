# GridBot — teach a kid to code, one maze at a time

**A pocket-sized "program your robot through the maze" game for the $10 Cheap Yellow
Display.** Kids snap together commands — *forward, turn, jump, loop, function, sense* —
hit **RUN**, and watch their little robot try to reach the battery. It bonks a wall? They
fix one line and try again. It never ends, it gets harder, and new programming powers
unlock as they go.

| ![Gameplay](docs/img/gameplay.gif) |
|:--|
| **Write a program, watch it run.** Your robot reaches the green **battery** by following the commands you snap together, dropping a glowing **breadcrumb trail** so you can see exactly where it went. Brick walls block you, dark squares are pits you fall into, and the robot's little arrow shows which way "forward" is — everything is relative to *its* heading, which is the whole point: kids learn to think like the robot. (Levels change theme as you climb — this is the *Cavern*.) |

> ### ⚡ [**Flash it from your browser →**](https://jamesdavid.github.io/cyd-GridBot/)
> Got a CYD? Plug it in and install GridBot straight from Chrome/Edge — no clone, no
> toolchain. (Powered by ESP Web Tools; see the [flasher page](docs/index.html).)
> **Need the board?** [Grab a CYD (ESP32-2432S028R) on Amazon](https://a.co/d/05H98aWQ) (~$10–15).

---

## Why this exists

Coding for kids usually means a tablet app or a website. GridBot puts it on a **real
object you can hold** — a cheap touchscreen you can leave on a shelf, hand to a kid, and
say "get the robot to the battery." No account, no internet, no ads. Just a robot, a maze,
and the dawning realisation that *you can tell it exactly what to do.*

The magic moment GridBot is built around: a kid writes
`REPEAT-UNTIL at-the-goal { if wall-ahead, turn; forward }` once, and it solves a maze it
has **never seen** — and then *another* maze, and another. That's not memorising a path.
That's an **algorithm**. A seven-year-old just discovered the wall-follower. That's the
hook, and the whole difficulty curve is designed to lead there.

### Why not just use a tablet app?

- **It's a thing, not a tab.** Dedicated cheap hardware that lives in the real world beats
  one more icon on a locked-down tablet. It feels like a gadget, because it is one.
- **Relative control teaches sequencing.** The robot moves along *its own heading*, so
  "forward" depends on where it's facing — kids have to run the program in their head.
  (An absolute N/S/E/W pad would be easier and teach nothing; we deliberately don't.)
- **It grows with the kid.** Five command tiers unlock over the first ~22 levels —
  Backward, Jump, Repeat loops, Functions, and Sensing — so a 5-year-old and a 10-year-old
  both have something to chew on, on the same device.
- **It's a real piece of engineering on a $10 board.** Deterministic procedural mazes that
  are *always solvable*, an explicit-stack interpreter (no recursion, fully steppable), a
  pixel-art editor, an ESP-NOW radio link, and a from-scratch UI — all on a no-PSRAM ESP32.

---

## Feature tour

### Pick a player

| ![Profile select](docs/img/profile-select.png) |
|:--|
| **Choose a Player.** Each kid gets their own robot, level, stars, and badges — saved to flash, so it's all there next time. Tap a card to play; tap the big **STATS / EDIT** strip for everything else. Up to six players + a **New Player** card. |

| ![New player](docs/img/profile-create.png) |
|:--|
| **Make a robot.** A big-key **QWERTY** keyboard (tuned for finger-on-resistive-glass), a name up to 8 letters, and a row of **eight differently-coloured robots** to pick from. Each kid also gets a hidden global ID so they can recognise each other over the radio later. |

### Write code, run it, fix it

| ![Code view](docs/img/code-view.png) |
|:--|
| **The Code view.** Your robot sits in the middle of its own d-pad: tap **↑ forward**, **↺/↻ turn**, **↓ backward**. The four corners are *growth slots* that fill in as you unlock powers — here all four are lit: **Jump**, **Repeat**, **Call F1**, and **Sense**. Tap to add commands to the program list on the right (it scrolls — no limit). **CLR / DEL / RUN** sit under the pad. The centre robot faces the maze's real start direction so you can reason about "forward" before you run. |

| ![A built program](docs/img/code-program.png) |
|:--|
| **A real program.** Snap blocks together and they stack up in the list — here: *forward, turn R, jump,* then a *repeat 2* loop with a *forward* **nested inside it** (note the **`1, 2, 3, 4, 4a`** numbering — `4a` lives inside loop `4`). Each block is colour-coded (loops carry a clear **`R`** badge); tap a block to select it, and a `REPEAT` shows a big button that **cycles its count 2→3→4→5**, while an `IF`/`UNTIL` cycles its condition. **`+ add inside`** and **`+ add here`** slots make it obvious where your next step lands. The list scrolls — no length limit. Tap **RUN** and the robot runs your code. |

| ![Maze preview](docs/img/maze-preview.png) |
|:--|
| **Study the maze.** Every level opens by showing you the board for a couple of seconds — *learn the layout* — before flipping to the Code view. You can flip back any time with the **VIEW** toggle (or press-and-hold to peek without losing your place). |

| ![Failure feedback](docs/img/fail.png) |
|:--|
| **Bonk!** Run a bad program and the robot turns red and shakes right where it went wrong — *"Bonk! a wall — tap to fix."* Tap, and you land back in the Code view with the **exact failing line flashed red**. Gentle, specific, never punishing. Fix one line, run again. |

| ![You win](docs/img/win.png) |
|:--|
| **You win!** Reach the battery and you score **1–3 stars** based on how short your program was — looping and functions are *rewarded*, because a `REPEAT 5` counts as two lines, not five. Then it's straight on to the next, slightly harder level. Forever. |

### Powers unlock as you climb

| ![Level intro](docs/img/level-intro.png) |
|:--|
| **Level intro.** A quick card before each level announces anything newly unlocked ("New: Sensing!") and any **badge** you just earned. Past the sensing tier, the **ARENA** opens up. |

| ![Full toolset](docs/img/tiers.png) |
|:--|
| **The full toolset** (a sensing-tier level). All four corner slots are filled, **Backward** is lit, and the program pane has the **MAIN / F1 / F2** function tabs plus **Save▸Lib / Load◂Lib** for the solution library (and a **`+brain`** button once NeuroBot unlocks). Here a `repeat` block is **selected**, showing the big **`repeat 2`** button you tap to cycle its count; an `IF`/`UNTIL` shows a button that cycles its condition (**wall / pit / goal**). |

### Stats & badges

| ![Stats](docs/img/stats.png) |
|:--|
| **Stats.** Level reached, total stars, win rate, streak, and a **command-usage bar chart** — locked commands are greyed with *when* they unlock ("unlock Lv 22"). Up top: **Badges 5/13**. From here you can **Edit name**, **Draw sprite**, or (behind two confirmations) delete the player. |

| ![Badges gallery](docs/img/badges.png) |
|:--|
| **16 badges to collect.** First Steps, Bright Spark (3 stars), Hopper (your first Jump), Looper, Architect (a Function), Sixth Sense, Champion (an arena win), On Fire (5-streak), Unstoppable (10-streak), Explorer (Lv 10), Veteran (Lv 20), Artist (draw a sprite), Star Collector, and the **NeuroBot** trio — **Brainiac** (train a brain), **Mind Over Maze** (clear a level *with* a brain), and **Battle-Ready** (save a brain as an Arena fighter). Gold medal = earned; grey = a hint for how to get it. |

### Themed worlds

| ![Meadow biome](docs/img/biome-meadow.png) |
|:--|
| **Meadow** (early levels) — green floors and hedges. |

| ![Nebula biome](docs/img/maze-preview.png) |
|:--|
| **Nebula** (sensing tier) — purple space-brick. Five biomes (Meadow → Cavern → Glacier → Circuit → Nebula) shift the palette as you climb, so progress *feels* like a journey. |

### Coins & gems to collect

| ![Collectibles](docs/img/collectibles.png) |
|:--|
| **Grab loot on the way to the goal.** Gold **coins** sit right on the route — scoop them up for spendable currency. Bright cyan **gems** (✦) are the real prize: they sit on a *detour off the path*, so you have to steer your robot out of its way to grab one — and collecting every gem on a board pays an **all-clear bonus**. Spend it all in the **shop** on robot colours and emojis. |

### Draw your own robot

| ![Pixel editor](docs/img/pixel-editor.png) |
|:--|
| **A KidPix-style pixel editor.** Draw your own 16×16 **character** *and* **goal** with pencil / eraser / fill / **mirror**, an 8-colour palette, and a big red **BOOM** button that blows the canvas up with a sound when you want to start over. Your custom robot then shows up in the actual game — and can be **traded to a friend** over the radio. |

### Arena — battle your bots

| ![Arena menu](docs/img/arena-menu.png) |
|:--|
| **Arena** (unlocks after the sensing tier). Pick a match type — **Race** (first to the goal) or **Sumo** (shove your opponent into a pit) — and an opponent: the built-in AI, **Hotseat** for two kids on one device, or **Radio** for two CYDs. |

| ![Pick an opponent](docs/img/arena-pick.png) |
|:--|
| **Battle-bots with personalities.** Face off against **Rusty** ("charges blindly"), **Bolt** ("fast & straight"), **Vex** ("hunts & shoves"), or **Ace** ("solves the maze" — a real navigator that plots a path through the board on the fly). Any program you save to your **library** in the campaign joins this roster as your own fighter. |

| ![Arena match](docs/img/arena.gif) |
|:--|
| **Watch it play out.** Two robots run their programs simultaneously on a shared board, tick by tick — they jostle for the path, collisions bounce them back, and pits knock them out. Here *Rusty* and *Bolt* dash to a dead-even **photo finish** (equal bots on a mirrored, fair board correctly tie — a smarter or faster bot wins). The whole match is **deterministic** — no randomness during play — so a rematch is identical and replays are free. |

| ![Radio](docs/img/radio.png) |
|:--|
| **Radio friend (ESP-NOW).** Two GridBots within range can **battle** — both screens run the *same* deterministic match from a shared seed — or **trade**, Pokémon-style: send a friend your favourite bot (and your custom robot art) and get theirs into your library. |

| ![Puzzle Race](docs/img/puzzle-race.png) |
|:--|
| **Puzzle Race — same maze, beat the clock.** A code-authoring contest instead of a live battle: both players see the *same* maze and get **45 seconds** to write a program for it (hotseat — Player 1 locks in, then passes the device). When the dust settles, each robot runs and is scored by how close it gets to the goal — **whoever ends up nearest wins** (reach it outright for the cleanest win). A tense little "can you out-code your friend?" mode that rewards thinking under pressure. |

| ![Puzzle Race result](docs/img/puzzle-race-result.png) |
|:--|
| The result screen scores each player by how close their robot got to the battery. |

### NeuroBot — stop *writing* the rules, start *training* them

> A late-game **graduation** (unlocks with the sensing tier). GridBot teaches *you write the
> rules*; **NeuroBot teaches you grow them** — the contrast between symbolic and learned AI is
> itself the lesson. It reuses the whole engine: the same maze, editor, Arena, and radio trade.

| ![NeuroLab](docs/img/neurolab-hub.png) |
|:--|
| **NeuroLab** — a lesson for each way machines actually learn, each small enough to *watch*. |

| ![Watch a neuron learn](docs/img/neuro-neuron.gif) |
|:--|
| **Backprop** — a neuron guesses, sees how wrong it is, and nudges its weights. Tap *Train* and watch the error fall and every example flip ✗→ok. (Then: multi-class go/turn/jump, and a hidden layer to crack **XOR** — what one neuron provably can't.) |

| ![Q-learning value spreads](docs/img/neuro-qlearning.gif) |
|:--|
| **Reinforcement (Q-learning)** — no code, no teacher: the robot tries the maze over and over and **value spreads back from the battery**, with arrows showing the policy it discovered. |

| ![Evolution](docs/img/neuro-evolution.gif) |
|:--|
| **Evolution** — a herd of random brains; the ones that get furthest **breed**. Watch the best one's path improve and the fitness curve climb, generation by generation. |

| ![The NEURO block](docs/img/neuro-block.png) |
|:--|
| **A brain is a block in your program.** Drop a **`brain`** into the normal editor next to your loops and ifs — *neurosymbolic* programming: explicit code for the easy parts, a trained brain for the tricky bit. It drops in **already wrapped in `repeat until at goal { brain }`** (a lone brain makes only *one* move), so it drives the whole maze the moment you've trained it. |

| ![Train the brain](docs/img/neuro-train.png) |
|:--|
| **The neuro interface.** Open it from the **`train brain >`** line under a brain block. **Teach** it (distil the optimal solver into the brain by backprop — reliable) or **Evolve** it (no teacher, just a score), watch its path solve the maze, then **Use it**. The trained brain saves *with* your program — so it persists and **trades & battles over the radio** like any other bot. |

| ![Train a fighter vs AI](docs/img/neuro-arena-train.png) |
|:--|
| **Train a fighter for the Arena.** A new **"Train a fighter vs AI"** mode off the Arena menu: evolve (or Teach) a brain to **beat an AI opponent on a real arena board** — fitness comes from *winning actual matches*, not just reaching the goal. **Save** it to your library and it shows up as a pickable Arena fighter, ready to battle a friend's bot over the radio. So kids can *prep their bots for battle*. |

---

## Under the hood (cross-cutting)

- **Always-solvable procedural mazes.** Every level is generated from `hash(player, level)`
  — never stored — by carving a guaranteed solution path, decorating with walls/pits at
  difficulty-scaled density, and BFS-verifying. Thousands of seeds tested; never an
  unsolvable board. Jump levels add single-tile pit gaps that *need* a jump.
- **A real interpreter.** Programs are an AST (commands, `REPEAT n`, `REPEAT_UNTIL`, `IF`,
  `CALL F1/F2`). Execution uses an **explicit call stack**, not C++ recursion, so it's
  steppable (animate one command at a time, or single-step to debug) and memory-bounded.
  A step cap stops runaway sense-loops with a friendly "your loop never finished."
- **One AST everywhere.** Player programs, saved library scripts, and AI opponents are the
  *same* shape — so the bot you build in the campaign drops straight into the arena.
- **Tiny art, big personality.** Robots and batteries are vector-drawn (a few hundred bytes
  of code, scalable to any tile size), plus optional 16×16 custom sprites — no SD card, no
  PROGMEM bitmap dump.
- **Made for fingers on glass.** ≥40px touch targets, debounced taps, press beeps, an RGB
  LED that flashes green on a win and red on a fail, and a tone for every tap/step.
- **Robust persistence.** Profiles, stats, badges, the resume slot, the solution library,
  and custom sprites are JSON on LittleFS, rebuilt-index-on-write so an interrupted save
  can't brick the menu.
- **A headless dev loop.** A USB-serial screenshot + tap-injection harness
  (`scripts/shot.py`, `scripts/drive.py`) lets the whole UI be driven and photographed
  without a finger — every screenshot in this README was captured that way.

---

## Get one running

GridBot runs on the **CYD (Cheap Yellow Display, ESP32-2432S028R)** — a ~$10–15
touchscreen ([grab one on Amazon](https://a.co/d/05H98aWQ)). Short version:

```bash
git clone <this repo> && cd cyd-GridBot
# PlatformIO (see HARDWARE_SETUP.md for the venv path on Windows)
platformio run -e cyd_gridbot -t upload --upload-port <your port>
```

Full build instructions, the parts list, the toolchain setup, and the board's hard-won
quirks (display orientation, brick-wall colours, touch calibration, the no-PSRAM budget)
live in **[HARDWARE_SETUP.md](HARDWARE_SETUP.md)** so this page stays a showcase.

It is **fully offline** — no WiFi, no accounts, no data leaves the device.

---

## Technical challenges — overcome & still open

**Overcome**
- **Always-solvable generation** on a deterministic seed (carve-path → decorate → BFS
  verify → reroll), with fairness rules (the robot never spawns facing a wall; pit gaps
  only appear once Jump is unlocked).
- **Steppable interpreter without recursion** — an explicit frame stack drives loops,
  conditionals, and functions, animatable one primitive at a time.
- **A from-scratch UI on a no-PSRAM ESP32** — no full framebuffer; dirty-rect tile
  rendering and a one-tile sprite keep RAM use tiny (firmware uses ~8% of RAM).
- **The CYD display quirks** — landscape-native MV=0 orientation, the dual-USB R/B colour
  order, touch calibration that comes out point-reflected at rotation 6 (inherited from
  the known-good config, see `CYD-ESP32-2432S028R.md`).
- **A USB-serial screenshot/automation loop** so the game could be playtested and
  photographed headlessly — it even caught a bad test fixture the first time it ran.
- **A deterministic multi-bot arena** — same inputs → byte-identical match log, proven by
  test, which hands you fair rematches and replays for free.

**Still open / known limits**
- Arena AI bots are simple; a left-turn "wall-follower" only truly solves spirals, so
  decisive races rely on *different* bots and a hazard-strewn board (symmetric identical
  bots correctly draw).
- ESP-NOW radio battle/trade is built but **hardware-pending** — it needs two physical
  boards to verify end-to-end.
- The on-screen mini-map and fog levels are reserved but not built.

---

## What's next

**Recently shipped:** ✅ **NeuroBot** — a whole ML-teaching mode (backprop / Q-learning /
evolution lessons, a trainable **brain block** in the editor, brains that persist & battle) ·
✅ **Puzzle Race** (shared-maze, beat-the-clock coding contest) ·
✅ **smarter arena bots** ("Ace" navigates the board) so Races are more often decisive ·
✅ **coin & gem collectibles + a sprite-colour & emoji shop** · ✅ a **chiptune menu theme** +
win fanfare + badge chime · ✅ smooth movement tween + a **win celebration** (confetti
& star fly-in) · ✅ five **level biomes** ·
✅ a 16-badge **achievements gallery** (incl. NeuroBot badges) · ✅ **breadcrumb trails** · ✅ Sumo (PUSH) ·
✅ a one-click **online flasher**.

Still on deck (full list in **[BACKLOG.md](BACKLOG.md)**):

- More **mode types** — relay, co-op (two bots must both reach goals), king-of-the-hill —
  building on the Puzzle Race / Race / Sumo set.
- The **Zap** verb (FIRE), AI-roster opponents, and library-bot **tournaments**.
- **Daily shared-seed challenges** and a leaderboard.
- **Verify the radio link** end-to-end on two boards (battle + Pokémon-style trade are
  built but hardware-pending), and an optional **mini-map** in the Code view.

---

## Status & license

Working firmware, verified on a real CYD (the campaign, all five unlock tiers, arena,
pixel editor, and achievements all run on hardware). Built phase-by-phase from `SPEC.md`
following `IMPLEMENTATION_STEPS.md`; ~40 host unit tests plus an on-device self-test gate
every change.

**License: [PolyForm Noncommercial 1.0.0](LICENSE).** Free for personal, hobby, research,
and educational use — clone it, build it, modify it, share it. **Commercial use or
reselling this (or derivatives) needs a written license.** Third-party components keep
their own licenses: **LovyanGFX**, **ArduinoJson**, and the Arduino-ESP32 core.

Built with [Claude Code](https://claude.com/claude-code).
