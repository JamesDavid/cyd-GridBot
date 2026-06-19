# GridBot — Specification

*Working title; rename freely. A single-player, kid-facing "program your character through the maze" game for the Cheap Yellow Display (CYD), built with PlatformIO/Arduino. Design doc only — no implementation here. Intended to be handed to Claude Code as the build brief.*

---

## 0. One-paragraph concept

Boot into a profile menu (same pattern as the Rock-Paper-Scissors app): pick an existing kid or create a new one, each with their own saved stats and progress. After selecting a player, the kid enters an endless, procedurally-generated sequence of grid mazes. There are two views with a one-tap toggle between them (hold to peek): a **Maze view** (the board, character, goal, and pitfalls) and a **Code view** (a directional control pad wrapped around the character — Forward, Backward, Turn Left, Turn Right — plus Clear and Run). The kid assembles a program, hits Run, and watches their character execute it step-by-step. If the character bonks a wall, falls in a pit, or doesn't reach the goal, they edit the program and try again. Levels get progressively harder — bigger grids, more pits, longer solutions — and new command blocks (Jump, Repeat loops, Functions, and Sensing) **unlock as the kid advances**. Every run feeds per-kid statistics. It never ends.

---

## 1. Target hardware & platform

- **Board:** CYD 2.8" **2-USB variant** (ESP32-2432S028R w/ USB-C + micro-USB). ESP32-WROOM-32, 320×240 landscape, resistive touch (XPT2046), no PSRAM.
- **Framework:** Arduino via **PlatformIO** (`platform = espressif32`, `framework = arduino`, `board = esp32dev`).
- **Display/touch library:** **LovyanGFX** (consistent with the Overhead sky-dashboard project), using a manual `LGFX` config class. LovyanGFX handles both the panel and the XPT2046 touch in one config, which avoids juggling two SPI libraries.

### 1.1 ⚠️ 2-USB display-driver gotcha (read first)
The 2-USB CYD frequently ships with an **ST7789** panel (not the ILI9341 of the original single-micro-USB board) and commonly needs **color inversion on** and/or **BGR color order**. **Do not re-derive this — inherit the exact, known-good panel + touch init from the working Rock-Paper-Scissors project on the same board.** If symptoms appear: blank/white screen → wrong driver; inverted colors (white↔black) → toggle `invertDisplay`; red/blue swapped → flip RGB/BGR order; mirrored axes or offset image → fix rotation + panel offset (ST7789 often needs an X/Y offset).

### 1.2 Reference pin map (confirm against the RPS config)
| Function | GPIO |
|---|---|
| TFT MOSI / SCLK / MISO | 13 / 14 / 12 |
| TFT CS / DC / RST | 15 / 2 / -1 |
| TFT Backlight (active high) | 21 |
| Touch (XPT2046) CLK / CS / DIN / OUT / IRQ | 25 / 33 / 32 / 39 / 36 |
| SD card CS / MOSI / SCK / MISO | 5 / 23 / 18 / 19 |
| RGB LED (active LOW) R / G / B | 4 / 16 / 17 |
| LDR (analog, optional) | 34 |
| Speaker / audio out (DAC1) | 26 |
| Boot button | 0 |

Touch is on its **own SPI bus** from the TFT — keep them separate in the LGFX config. Touch **must be calibrated** (resistive); store the calibration in NVS once and reuse.

---

## 2. Dependencies (PlatformIO `lib_deps`)
- `lovyan03/LovyanGFX` — display + touch
- `bblanchon/ArduinoJson` — profile/stats serialization
- LittleFS (built-in) — profile & calibration storage
- (Optional) a light tone/beep helper for GPIO 26 audio feedback — or hand-rolled `ledcWriteTone`.

No networking. Fully offline.

---

## 3. High-level architecture

A small **screen/state machine** drives the app. One active "screen" owns drawing + touch handling; transitions are explicit.

```
BOOT
 └─> PROFILE_SELECT ──(new)──> PROFILE_CREATE ──> PROFILE_SELECT
        │ (pick)
        └─> LEVEL_INTRO ──> GAME ⇄ {tab: MAZE_VIEW | CODE_VIEW}
                              │ (run)
                              └─> EXECUTING ──> WIN ──> LEVEL_INTRO(next)
                                            └─> FAIL ──> CODE_VIEW
        (back from anywhere) ──> PROFILE_SELECT
```

### 3.1 Module breakdown
- **`App` / state machine** — owns the current screen, dispatches `update()` + `handleTouch()`.
- **`ProfileStore`** — load/save/list/create kid profiles on LittleFS.
- **`MazeGen`** — deterministic procedural maze generation from a seed; **guarantees solvability**.
- **`Maze`** — runtime grid model (tiles, start, goal, character pose).
- **`Program`** — the kid's authored code as an **AST** (supports nesting for loops/functions).
- **`Interpreter`** — executes the Program against the Maze with an explicit **call stack**; steppable and animatable.
- **`UI`** — shared widgets (buttons, tab bar, status bar, scroll list, toasts), tuned for big fingers on resistive touch.
- **`Stats`** — per-profile counters + star scoring.
- **`Assets`** — character sprites, tile glyphs, command icons (compiled-in PROGMEM bitmaps).

---

## 4. Profile system

Mirrors the RPS app's "pick a player / make a new one" flow.

- **PROFILE_SELECT:** grid of up to ~6 profile cards (name + chosen character avatar + level reached) plus a **"+ New Player"** card. Tap to play; long-press (or a small gear) for delete/rename later.
- **PROFILE_CREATE:** name entry + character pick.
  - **Name entry** on a 320×240 resistive screen: avoid a full QWERTY (touch targets too small). Use a **simple on-screen keyboard with large keys**, or a constrained picker (scroll A–Z to build a short name, max ~8 chars). Decision flagged in §16.
  - **Character pick:** choose a character from the roster (§4.1). The choice sets both the player sprite *and* the matching goal token. Stored as an index.
- Profiles persist across reboots. Selecting a profile loads its progress and stats.

### 4.1 Character roster & themed goals
The chosen avatar is more than a portrait — it selects a **matched (character, goal-token) pair**, so the goal tile shows *that character's* prize instead of a generic flag. Purely cosmetic: **zero** effect on generation, solvability, or the interpreter — only the player sprite and the goal sprite change.

| # | Character | Goal token |
|---|---|---|
| 0 | Mouse | cheese |
| 1 | Rabbit | carrot |
| 2 | Dragon | egg |
| 3 | Cat | fish |
| 4 | Bee | flower |
| 5 | Robot | battery |
| 6 | Astronaut | rocket / star |
| 7 | Frog | fly |

- **Extensible:** adding a character = appending one `{name, charSprite, goalSprite}` entry to the `Assets` roster and bumping the picker. The roster index is what `profile.avatar` stores.
- **Facing without flipping the art:** rotating a whole mouse/dragon sprite to face "south" looks wrong (upside-down). Keep each character **upright** and show heading with a small **nose / arrow indicator** (optionally mirror horizontally for E vs W). The interpreter tracks the true `facing`; the sprite just gets a direction cue.
- **Collectible tie-in (if §16 item 3 lands):** en-route collectibles can be **mini versions of the goal token** (mouse grabs cheese bits, rabbit grabs baby carrots) — keeps the art set tiny and the theme tight, echoing your usual coin/shop loop.

---

## 5. Data model

### 5.1 Profile (one JSON file per kid)
```jsonc
{
  "id": "u01",
  "name": "Theo",
  "avatar": 2,              // roster index (§4.1): sets player + goal-token art
  "level": 14,             // highest level reached (current)
  "seedBase": 73219,        // per-profile RNG salt so two kids see different mazes
  "unlocks": { "jump": true, "repeat": true, "func": false, "sense": false },
  "stats": { ...see §9... },
  "settings": { "sound": true, "animSpeed": 1 },
  "work":    { "level": 14, "program": { /* AST */ } },        // current-level resume slot (§11.1)
  "library": [ { "name": "Wall Hugger", "program": { /* AST */ } } ]  // named, reusable (§11.1)
}
```
- An **index file** (`/profiles/index.json`) lists `{id, name, avatar, level}` for fast menu rendering without opening every profile.

### 5.2 Why mazes are NOT stored
Mazes are **regenerated deterministically** from `seed = hash(profile.seedBase, levelNumber)`. This means: zero maze storage, perfectly reproducible levels, and a kid can replay/resume a level exactly. Only the level number + per-profile salt need to persist.

### 5.3 Tile types
`FLOOR`, `WALL`, `PIT`, `START`, `GOAL`, and (later tiers) `COLOR_A/COLOR_B` for conditionals, `COIN`/`STAR` for optional collectibles.

### 5.4 Program as an AST (forward-compatible with all tiers)
Even though Tier 1 only emits a flat list, model the program as a tree from the start so loops/functions don't require a rewrite:
```
Node = { type: CMD,          cmd: FWD|BACK|TURN_L|TURN_R|JUMP }
     | { type: REPEAT,       count: N,  body: [Node...] }   // count loop
     | { type: REPEAT_UNTIL, cond: C,   body: [Node...] }   // sense loop
     | { type: IF,           cond: C,   body: [Node...] }   // sense branch (no ELSE in v1)
     | { type: CALL,         func: F1|F2 }
C (condition) = WALL_AHEAD | PIT_AHEAD | AT_GOAL
Program = { main: [Node...], funcs: { F1: [Node...], F2: [Node...] } }
```

---

## 6. Maze generation (must always be solvable)

Two-phase generator keyed by the level seed:

1. **Carve a guaranteed solution path.** Place START and GOAL at a distance scaled to difficulty. Carve a connected FLOOR path between them (recursive-backtracker maze, or a biased random walk with a fixed minimum number of turns). This path is the *solvability guarantee* — it can never be removed.
2. **Decorate.** On non-path cells, randomly assign WALL or PIT at difficulty-scaled densities. PITs may be placed **adjacent to** the solution path (so a wrong Forward drops the kid in) but **never on it**.
3. **Verify (cheap safety net).** Run a BFS/flood-fill from START; assert GOAL is reachable across FLOOR (treating PIT/WALL as blocked, but allowing a 1-gap PIT crossing only once Jump is unlocked). If verification ever fails, re-roll with `seed+1`. With the carved-path approach this should essentially never trigger, but the assert protects against edge cases.

**Fairness rules**
- Before Jump is unlocked, the solution path must be PIT-gap-free (walkable with Forward/Turn only).
- After Jump unlocks, the generator may place **single-tile PIT gaps on the path** that require exactly one Jump to clear (never a 2-wide gap, which Jump can't clear).
- Always leave a 1-tile margin so the character is never spawned facing immediately into a wall with no first move.

---

## 7. Difficulty progression & tiered unlocks

Difficulty is a function of level number. Suggested curve (tunable constants, not hard rules):

| Levels | Grid | Pit density | New mechanic unlocked | Par hint |
|---|---|---|---|---|
| 1–3 | 4×4 | none | **Tutorial:** Forward, Turn L/R | generous |
| 4–8 | 5×5 | low | **Backward** unlocks (Tier 1.5); dead-ends / longer paths | generous |
| 9–14 | 6×5 | low–med | **Jump** unlocks (single pit gaps appear) | moderate |
| 15–24 | 7×6 | med | **Repeat N** loop block unlocks | tighter |
| 25–39 | 8×6 | med | **Functions (F1)** unlock | tight; loops rewarded |
| 40–54 | up to ~10×8 | med–high | **Functions F1+F2** | tight |
| 55+ | ~10×8 + fog / multi-maze | med–high | **Sensing** unlocks (see §7.1) | tight; algorithm-style |

- **Grid scaling cap:** tiles must stay ≥ ~26px so they're readable and the character animation is legible. That caps the largest grid at roughly **10×8** in landscape (see §10).
- Unlocks are **sticky** per profile (`profile.unlocks`) and gated by level so a returning kid keeps their toolset.
- "Endless" = the difficulty constants keep climbing; past the table, hold the grid cap and increase path length, turn count, pit density, and lower the par instruction count.
- **Sensing can arrive earlier as an option:** `REPEAT_UNTIL WALL_AHEAD` ("walk until you hit a wall") is so intuitive it could be introduced before functions if playtesting with Theo suggests it. Tier ordering is a knob, not a contract.

### 7.1 Sensing levels: the generalization twist (the conceptual payoff)
Sensing blocks are **pointless if the kid can see the whole maze** — they'd just hardcode the path and never need to ask "is there a wall?" Sensing only earns its place when one program must work *without* foreknowledge. Two level styles unlock that, either of which can back the Sensing tier:
- **Fog:** the character only reveals a tile or two ahead, so the kid *must* sense rather than memorize.
- **One program, many mazes (recommended):** the same code runs against 2–3 different boards and must beat **all** of them. This turns `REPEAT_UNTIL AT_GOAL { IF WALL_AHEAD turn; FORWARD }` into a genuine **wall-follower algorithm the kid discovers** — not a memorized path. This is the high point of the whole game; design the Sensing levels around it.
- **Generation note:** for multi-maze levels, `MazeGen` emits a small set from consecutive seeds that share the START facing and a goal reachable by the same hand-rule, so one correct general program clears the whole set.

---

## 8. The programming model & interpreter

### 8.1 Command set by tier
- **Tier 1:** `FORWARD`, `TURN_LEFT`, `TURN_RIGHT`
- **Tier 1.5:** `BACKWARD` — held back one tier because the youngest kids sometimes read "back" as "turn around and go," so let pure sequencing land first.
- **+Jump tier:** `JUMP` (moves 2 tiles forward; legal only if the landing tile is FLOOR/GOAL and the gap is a single PIT)
- **+Repeat tier:** `REPEAT n { ... }` (count loop, n ∈ 2..maybe 5; body is a sub-sequence)
- **+Function tier:** define `F1` (and later `F2`) bodies; `CALL F1` block. **No recursion** (a function may not call itself or a function that calls it) to keep the stack bounded and the game age-appropriate.
- **+Sensing tier:** condition blocks over `WALL_AHEAD | PIT_AHEAD | AT_GOAL`, in two forms:
  - `REPEAT_UNTIL <cond> { ... }` — **lead with this**; "walk forward until a wall" is how a kid narrates a maze, and it's useful on its own without nesting.
  - `IF <cond> { ... }` — react-and-continue (e.g. "if wall ahead, turn"). **No `ELSE` in v1** — it doubles the nesting and UI, and IF-inside-a-loop covers nearly everything.
  - Pairing is the point: `WALL_AHEAD` ↔ turn, `PIT_AHEAD` ↔ jump, `AT_GOAL` ↔ loop terminator. `ON_COLOR` is intentionally omitted unless/until colored tiles exist.
  - Only meaningful alongside fog or multi-maze levels — see §7.1.
  - `ENEMY_AHEAD` / `ENEMY_NEAR` also exist, but only in **Arena mode** (§18), where a second bot is on the board.

### 8.2 Character pose & semantics
- Pose = `{row, col, facing∈{N,E,S,W}}`.
- `FORWARD`: target = one tile in `facing`. WALL or edge → **bonk fail** (highlight the offending instruction). PIT → **fall fail**. FLOOR/GOAL → move.
- `BACKWARD`: move one tile **opposite** `facing` **without** changing `facing` (the mouse backs up). Same WALL/edge → bonk, PIT → fall rules as Forward.
- `TURN_LEFT` / `TURN_RIGHT`: rotate `facing` 90°. **Relative**, not absolute — Forward/Backward always move along the current heading. Never fails. (An absolute-direction variant — 4 buttons = N/S/E/W, no heading — exists but is *not* used; relative control is what teaches sequencing.)
- `JUMP`: target = two tiles in `facing`. Legal only if the intermediate tile is a single PIT (or FLOOR) and the landing tile is FLOOR/GOAL; otherwise fail.
- **Conditions** (for `IF` / `REPEAT_UNTIL`) read the current pose and never move or fail — they only gate a body: `WALL_AHEAD` = tile in `facing` is WALL or edge; `PIT_AHEAD` = tile in `facing` is PIT; `AT_GOAL` = current tile is GOAL.
- **Win:** character occupies the GOAL tile after any step (and, if collectibles are enabled, all required coins collected first).

### 8.3 Execution engine
- An **explicit interpreter with a call stack** (frames for the main program, each REPEAT iteration, and each function CALL). Do **not** use the C++ call stack/recursion to walk the AST — an explicit stack keeps it steppable and bounds memory.
- **Steppable:** `step()` advances exactly one primitive command and returns an outcome (`OK`, `WIN`, `BONK`, `FELL`, `DONE_NO_WIN`). The UI calls `step()` on a timer to animate (`animSpeed` controls the interval, e.g. 250–600 ms/step).
- **Run modes:** **Run** (auto-step to completion/failure with animation), **Step** (one command per tap, for debugging — great teaching tool), **Reset** (return character to START, keep program), **Clear** (empty the program).
- **Step cap (now load-bearing):** count loops are bounded and recursion is disallowed, but `REPEAT_UNTIL` **can spin forever** if its condition is never met. The hard cap (e.g. 500 primitive steps) is what stops it, reported as `DONE_NO_WIN` — which reads to the kid as "your loop never finished," useful feedback.
- `IF` and `REPEAT_UNTIL` reuse the **same frame mechanism** as `REPEAT`/`CALL`: push a body frame; for `REPEAT_UNTIL`, re-check the condition at the top of each pass. No new engine machinery beyond evaluating a condition.
- **On failure:** stop, flash the failing instruction in the Code view, shake/redden the character, and bounce the kid to the Code view to edit. Gentle, not punishing.

---

## 9. Statistics (per profile)

Tracked in `profile.stats`, updated each run/level:
- `levelsCompleted`, `currentLevel`
- `totalRuns`, `totalWins`, `bonks`, `falls`
- `starsTotal` and per-level best (see scoring)
- `bestInstructionCount` per level (fewest primitives to win — rewards loops/functions)
- `commandsUsed` histogram (FWD/BACK/TURN/JUMP/REPEAT/CALL/SENSE) — nice for a "what did I use most" screen
- `totalPlaySeconds`, `currentStreak` (consecutive levels won on first try)

**Star scoring (per level):** compare the kid's winning **instruction count** (count blocks, with a loop counting as its written size, not its expansion — this is what rewards loops) against a **par** derived by the generator (e.g., shortest-path-based).
- ≤ par → ★★★
- ≤ par × 1.5 → ★★
- win at all → ★

A simple **stats screen** (reachable from the profile menu or a pause button) shows level reached, total stars, win rate, and the command histogram as a little bar chart.

---

## 10. UI / screen layout (320×240 landscape)

**Global chrome**
- **Top status bar (~22px):** profile name + avatar, `Lv 14`, star count, a small ⏸/back button.
- **Bottom bar (~30px):** a single **VIEW toggle** that flip-flops Maze ⇄ Code (its label/icon shows the view you'll switch *to*) plus a separate **▶ RUN** button. The toggle stays in the same spot for muscle memory, and **press-and-hold it to peek** at the maze, releasing to snap back to the Code view exactly where you left off — so a kid can check the layout without losing their scroll position or place in the program. RUN switches to the Maze view and animates.
- Usable middle band ≈ **320 × ~188px**.

**Maze view**
- Grid centered in the middle band. Tile px = `floor(min(320/cols, 188/rows))`, clamped to ≥26px. Draw walls as solid blocks, pits as dark holes, **goal as the character's themed token (cheese / carrot / egg…, per §4.1)**, character as the chosen avatar sprite kept upright with a small nose/arrow cue for `facing`.
- During execution, the character animates tile-to-tile; redraw only dirty tiles (see §12) to keep it smooth.

**Code view — the control pad** (the character sits in the middle of its own d-pad; tapping a direction appends that command to the program)

```
┌────────────────────┬──────────────────┐
│         ↑          │ PROGRAM    3/par │
│   [J?] forward [J?]│ 1  ↑             │
│    ↺    🐭    ↻    │ 2  ↻             │
│   turnL      turnR │ 3  ↑             │
│         ↓          │ 4  ↑             │
│   [J?]  back   [J?]│ 5  ↺   (scroll)  │
│ ┌───────┐ ┌───────┐│ …                │
│ │ CLEAR │ │ RUN ▶ ││                  │
│ └───────┘ └───────┘│                  │
└────────────────────┴──────────────────┘
   ~156px control pad      ~164px list
```

- **3×3 control pad (~52px cells, 156×156):** up = `FORWARD` (↑), down = `BACKWARD` (↓, **greyed/locked until Tier 1.5**), left = `TURN_LEFT` (↺), right = `TURN_RIGHT` (↻), character avatar in the center cell. **Straight arrows for move (↑↓), curved arrows for turn (↺↻)** so a kid never misreads "turn left" as "slide left."
- **The four corners are the growth slots `[J?]`.** Empty in the simple game; they fill as tiers unlock — `JUMP`, then `REPEAT`, then `CALL F1`, then a `SENSE` block — up to four extra commands with **zero relayout**. Locked slots show a greyed lock that hints at what's coming.
- **CLEAR / RUN (~52px tall)** sit under the pad: Clear empties the program; Run switches to the Maze view and animates. (`Step` and `Reset` live on the Maze view during execution.)
- **Program list (right, ~164px, scrollable):** placed blocks in order. ~30px rows → ~7 visible + scroll. Tap a row to select; selected block can be **deleted**, `REPEAT` gets +/– for its count, and sense blocks get a small **condition picker** (WALL / PIT / GOAL). Header shows **count / par**.
- **Nesting = thin "E-bracket," not Scratch puzzle pieces.** Each body (`REPEAT` / `IF` / `REPEAT_UNTIL` / function) is indented ~10–12px and wrapped by a **1–2px bracket** down its left edge with short top and bottom caps (a sideways-E / `[`), **color-coded by construct** (loop / sense / function). Cheap to draw and groups the body clearly without eating the narrow 164px width. **Cap nesting at ~3 levels** so indent + brackets stay legible — deeper logic belongs in a function (one `CALL` row) anyway.
- **Editing functions:** a `MAIN | F1 | F2` toggle (appears once functions unlock) switches which body the list edits. The main list shows `CALL F1` as a single row, so the body lives off-screen and the list never crowds — functions actually *help* the small screen. Once the library unlocks, **Save▸Lib / Load◂Lib** controls sit beside this toggle (§11.1).

All targets ≥ ~40px (the 52px pad cells and buttons clear this comfortably).

- **Optional persistent mini-map (default off):** for kids who'd rather not flip or peek at all, a small non-interactive maze thumbnail (showing walls, pits, goal, and the character's start) can sit in a corner of the Code view. Trivially legible at early grid sizes (4×4–6×5); at the ~10×8 cap the tiles get tiny (~6px), so it's a glance aid, not a substitute for the full Maze view. It costs program-list width/rows, so it's a tradeoff — leave it off by default and consider it a per-profile setting.

**Touch ergonomics (resistive, kid fingers):** every interactive target ≥ **~40px**. Prefer fewer, bigger controls + scrolling over dense layouts. Debounce taps; add a short press animation + a beep so taps feel registered on the slightly-laggy resistive panel.

---

## 11. Game loop & flow

1. **LEVEL_INTRO:** brief card — "Level 14" + any newly-unlocked block ("New: Repeat!") with a one-line how-to. Tap to start.
2. **GAME:** kid flips between the Maze and Code views with the view toggle (or holds it to peek), authors a program.
3. **RUN → EXECUTING:** interpreter steps on a timer; character animates.
4. **WIN:** stars awarded (animation + happy beep), stats updated, profile saved, advance `currentLevel`, back to LEVEL_INTRO for the next.
5. **FAIL:** highlight offending step, sad beep, return to CODE view with the program intact.

Autosave the profile on every WIN and on tab/back transitions (debounced to limit flash wear).

### 11.1 Saving scripts: per-level resume vs. the library
Two separate mechanisms, deliberately not merged:
- **Per-level resume (default, automatic, everyone).** The *current* level's working script autosaves to a single slot (`profile.work`) so a power-off or "come back later" never loses progress; it's overwritten on advance. To keep storage bounded in an endless game, **solved levels keep only their stats** (stars, best instruction count) — not the script — by default.
- **Solution library (deliberate, named, reusable).** Unlocks with the **Functions tier** and is the engine behind the §7.1 generalization levels: write something general (a wall-follower), tap **Save▸Lib**, name it ("Wall Hugger"), then **Load◂Lib** it into a later level — as the whole `main` program *or* dropped into a function slot (`F1`/`F2`), which unifies the library with functions. Capped at ~8–12 entries in `profile.library`.
- **Why split them:** resume is just safety; the library is the *point* — it's where "I wrote an algorithm once and reused it" lives, the core abstraction lesson and the multi-maze payoff. Merging them would either bloat storage (every level saved forever) or bury the reuse concept.

---

## 12. Rendering & memory notes

- ESP32-WROOM has **no PSRAM**; a full 320×240×16bpp framebuffer (~154 KB) is too costly to keep alongside everything. **Do not double-buffer the whole screen.**
- Use **tile-based dirty-rect rendering**: draw the static maze once, then during animation redraw only the few tiles the character moves between (and the character sprite itself). Use a **small LovyanGFX sprite** sized to one tile (or one tile + margins) for flicker-free character moves — that's only a few KB.
- Keep art as compiled-in PROGMEM bitmaps or simple vector-drawn shapes; don't read from SD at runtime. Each roster entry (§4.1) is one small **character sprite + one goal-token sprite**; at ~8 characters and small tile sizes this is a few KB total — trivial for flash, fine for RAM.
- ArduinoJson docs for profiles should be small; prefer streaming to/from LittleFS and keep working buffers modest.

---

## 13. Audio & feedback (optional, nice-to-have)
- GPIO 26 (DAC/`ledc` tone) for: tap blip, step tick, win jingle, fail buzz. Gate behind `settings.sound`.
- RGB LED (4/16/17, active-low) as ambient feedback: green flash on win, red on fail. Optional.

---

## 14. Persistence layout (LittleFS)
```
/calib.json            // touch calibration (write once)
/profiles/index.json   // [{id,name,avatar,level}, ...]
/profiles/u01.json     // full profile + stats (see §5.1)
/profiles/u02.json
...
```
- Mazes: **not stored** (regenerated from seed, §5.2).
- The resume slot and library live **inside** the profile JSON (no separate files) and are bounded: one `work` slot, ~8–12 `library` entries (§11.1).
- Add a LittleFS partition in a custom `partitions.csv` (4MB flash: app + LittleFS data region).

---

## 15. PlatformIO config sketch (for orientation, not final)
```ini
[env:cyd2usb]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
board_build.partitions = partitions.csv   ; carve out LittleFS
lib_deps =
  lovyan03/LovyanGFX
  bblanchon/ArduinoJson
build_flags =
  -DLGFX_USE_V1
  ; panel/touch params live in the LGFX config class, inherited from the RPS project
```
Native-USB CDC: the 2-USB board's USB-C is via a CH340-style bridge on most units (not native ESP32 USB), so standard serial upload/monitor applies — but confirm against the working RPS upload setup.

---

## 16. Open decisions for you (flagged, with my defaults)

1. **Name-entry method on resistive touch.** Default: large-key reduced on-screen keyboard, max 8 chars. Alt: A–Z scroll picker. (Full QWERTY is too small to tap reliably.) — *pick one.*
2. **App name.** "GridBot" is a placeholder.
3. **Collectibles?** Optional coins/stars the character must grab before the goal — adds routing depth and ties into your usual coin/shop-economy motif (à la Theo's Dying Colors). Default: **off for v1**, easy to add later since the tile model reserves the type.
4. **Sensing levels: fog vs multi-maze.** The sensing tier is now in (§7.1, §8). The remaining call is *how* to make sensing meaningful: **fog** (limited view) or **one-program-many-mazes** (my pick — it turns into real algorithm discovery), or both. `ELSE` and `ON_COLOR` stay deferred.
5. **Animation speed / step timing** defaults (I suggested 250–600 ms/step, kid-adjustable).
6. **Profile delete/rename** UX — long-press vs gear icon.
7. **Archive solved-level solutions?** Default keeps only stats per solved level (bounded storage); an optional "favorite this solution" could archive a script for replay. The library (§11.1) already covers deliberate reuse, so this is purely a nostalgia/replay nicety.
8. **Arena match flavor to ship first (§18).** Default **Race** (no weapon, gentlest); Sumo and Zap layer on later through the swappable match-type module — so this is a sequencing call, not a fork.

---

## 17. Phased roadmap

- **MVP (v0.1):** one profile (or hardcoded), Tier-1 commands (Forward/Turn; Backward unlocks at Tier 1.5) via the control-pad Code view, fixed small grids, build → run → win/fail, basic Maze + Code tabs, no persistence. Proves the interpreter + rendering loop on the real board.
- **v0.2:** procedural generation + solvability verify; difficulty curve; star scoring.
- **v0.3:** profile system + LittleFS persistence + stats screen.
- **v0.4:** Jump → Repeat → Functions unlocks (corner slots fill in); character selection; audio/LED feedback.
- **v0.5+:** Sensing tier (`REPEAT_UNTIL` / `IF`) with fog or multi-maze levels (§7.1); collectibles; polish; animation tuning.
- **v0.6+ (Arena mode, §18):** multi-bot match engine + **Race** match type vs. built-in AI bots and hotseat; then Sumo/Zap match types and library-bot opponents. Two-CYD radio (ESP-NOW) is a far-later optional stretch.

---

## 18. Arena mode (battle bots) — post-campaign capstone

A separate **mode** from the campaign, unlocked **after the Sensing tier** — because a bot that fights can't run a memorized sequence, it has to sense and react, which is exactly what the campaign just taught. Two bots run **simultaneously** on a shared board; the kid authors their bot in the same Code view, picks an opponent and a match type, then watches the match play out.

### 18.1 Match engine (shared by every match type)
- **N=2 bots** (extensible). Each bot = one interpreter context (pose + call stack + program AST) over a single shared `Maze`.
- **Tick loop:** each tick, advance every live bot by one primitive `step()`, then run a **resolution pass** (collisions, pushes, hits) and check win/lose; animate the tick. This is the campaign's steppable interpreter, just driven N-up.
- **Determinism:** given both programs + start state + match type, a match is fully deterministic — **no RNG during play**. Free fair rematches, replays, and "spectate two saved bots."
- **Simultaneity rules (define once):** both bots target the same tile → both bounce back; a bot forced onto a PIT or off-board → out; attacks resolve after moves within a tick. Few, legible rules.
- **Step cap → draw** (or score on progress/coins, per type).
- **Fair arenas:** `MazeGen` produces **symmetric boards** (bots equidistant from goal/center, mirrored hazards) so neither side has a positional edge — same fairness idea as §7.1.

### 18.2 Match types (the "mix" = one engine, a swappable rule module)
Build the engine to take a **match-type module** (win condition + any extra verb). Three variants, gentlest → spiciest:
- **Race** *(ship first)* — first bot to the GOAL wins. No weapon, no contact damage; bots still jostle for the path. Pure speed + pathfinding; safest for the youngest.
- **Sumo** — no goal; force the opponent into a PIT or off-board. Adds a `PUSH` verb (shove the tile ahead; an enemy there slides one tile that way — into a pit = out). Physical, weaponless.
- **Zap** — adds a `FIRE` verb: a projectile travels in `facing` until it hits a wall or the enemy; a hit disables (or scores in best-of-N). Most combat-y; gate to older kids.
- All three share the tick loop, sensing, fairness, and determinism — only the win check + extra verb differ. Ship Race, add Sumo/Zap later with **no engine rework**.

### 18.3 New sensing conditions (arena-only)
- `ENEMY_AHEAD` — opponent occupies the tile in `facing`.
- `ENEMY_NEAR` — opponent within N tiles (hunting / avoidance).
- These extend the condition evaluator to read **dynamic occupants**, not just static tiles. Campaign conditions stay static; the arena adds the live ones.

### 18.4 Opponents (v1 = AI bots + hotseat)
- **Built-in AI bots:** a roster of canned opponent programs (stored ASTs) of rising cleverness — a dumb "always forward," a wall-follower, a `ENEMY_NEAR` hunter. Authoring an opponent is just writing a program.
- **Your library bots:** any program saved to the library (§11.1) can enter as a fighter ("Wall Hugger vs. the house Hunter"). The payoff loop: build a bot in the campaign, battle with it. Opponents, library bots, and player programs are **all the same AST**.
- **Hotseat (2 kids, 1 device):** P1 authors and locks in (a "ready / hide" screen so P2 can't peek), hands off, P2 authors, then both run. No networking — the lock-in/handoff screen is the only new UI.
- **Two CYDs over radio — explicitly later/optional.** ESP-NOW would sync both programs to each device and run the *same deterministic match* on both screens — a clean fit for the deterministic engine, but real pairing/networking UX. Parked far down the roadmap.

### 18.5 UI
- Authoring **reuses the Code view unchanged** — you're just writing a program.
- Pre-match screen: pick match type + opponent (AI roster / library bot / hotseat).
- Match view = the Maze view with two avatars + a small scoreboard; Run plays the tick loop. Hotseat adds the lock-in/handoff screen.

### 18.6 Why it's cheap to add
- Multi-bot = N interpreter contexts + a resolution pass; no new core machinery.
- Two sprites + dirty-rect rendering is trivial for the ESP32 — no perf concern.
- Everything stays one AST shape (player, library, AI), honoring the §11.1 invariant.
- Determinism hands you fair rematches and a future spectate/replay feature for free.

---

*End of spec.*