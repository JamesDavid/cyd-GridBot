# GridBot — Backlog

The living list of planned/possible work. The README shows only the juiciest
highlights and links here. Items are grouped; checked = done, unchecked = future.

## Playtest ideas — make it more fun (from on-device playtesting)
- [x] **Achievements / badges** (17): First Steps, Bright Spark, Hopper, Looper,
      Architect, Sixth Sense, Champion, On Fire, Unstoppable, Explorer, Veteran,
      Artist, Star Collector, **Brainiac** (train a brain), **Mind Over Maze** (clear a
      level with a brain), **Battle-Ready** (save a fighter), and **Generalist** (one
      brain clears the held-out gauntlet). Sticky bitmask in the profile, count on the
      stats header, celebrated on the level-intro when earned, **+ a badges gallery
      screen with icons** (reachable from Stats).
- [x] **Juice**: smooth tile-to-tile tween, breadcrumb trail, character shake on bonk,
      and a win celebration with a **multicolour confetti burst + animated star fly-in**
      (each earned star pops into the YOU WIN overlay over dim placeholders).
- [x] **Character emotes**: happy face on win, dizzy/✖-eyes on bonk. (Future: pit splash.)
- [x] **Reorder program lines** — done via **Up/Dn** buttons on the selected line
      (`moveSelected`); drag-to-reorder is the unbuilt nicety.
- [ ] An **Undo** button for edits.
- [x] **Level biomes/themes**: floor/wall palette varies per level band (Meadow,
      Cavern, Glacier, Circuit, Nebula) so progress feels visual.
- [x] One-line **par hint** ("par N") shown in the game chrome so kids can aim for the
      3-star efficiency target. (Future: an **in-game speed slider** for animation pace.)
- [x] **Shared-seed challenge**: a 4-digit code picks one fixed-difficulty board, so
      friends entering the same code race the *identical* maze. Reached from the Arena
      menu; wins award no campaign progress/coins (not farmable) — just the star rating.
- [x] **Coin collectibles + a shop** for sprite colours **and emojis** (ties to the
      pixel editor and the coin-economy motif). Coins sit on the path; **STAR gems**
      are a rarer bonus on a detour off it (see Campaign polish).
- [x] **Music**: non-blocking chiptune menu theme (loops on select/intro) + a win
      fanfare + a badge chime; SFX briefly override and the melody resumes, **plus an
      in-game mute toggle** on the home screen **and a distinct minor-key battle theme on
      the Arena / Puzzle Race menus** (stops cleanly before a match's step-tick SFX).

## Campaign polish (deferred niceties)
- [x] Brick-red walls + void (background-colour) pits so hazards read clearly.
- [x] Program list scrolls (auto-follows newest + tappable scrollbar) and the pane
      extends full-height — no command-count limit on screen.
- [x] Bigger chrome titles; compressed unlock curve (sensing at L15, not L55);
      multi-maze challenges interspersed every 5th level past sensing.
- [x] Failure feedback on the maze before returning to code (see below).
- [x] Smooth tile-to-tile character tween during animation (glides between tiles).
- [ ] Press animation on buttons (brief depress + release) beyond the colour change.
- [ ] Optional persistent mini-map in the Code view (SPEC §10; per-profile setting,
      default off). Field reserved in `Settings.miniMap`.
- [x] Collectibles (coins/stars to grab before the goal) — tile types `COIN`/`STAR`.
      **Coins** sprinkle on the solution path (passive bonus currency). **STAR gems**
      are a rarer bonus placed on a reachable detour OFF the path, so you must steer to
      grab them; collecting every gem on a board pays an all-clear coin bonus on the
      win. Both spend in the shop. Generation is gem-reachability unit-tested.
- [ ] **Gem Hunt / gems-before-goal** *(backlogged 2026-06-25)*. Today gems are a purely
      **optional** bonus — you win by reaching the GOAL regardless. Make them a **required**
      objective: the goal won't complete until every gem on the board is collected. Most of
      the plumbing already exists (placement, render, `_gemTaken`/`_gemTotal`/`recountGems`,
      all-clear payout) — the work is gating the win in `GameScreen::settleOutcome` behind a
      per-level "gems required" flag (so it doesn't retroactively harden existing levels),
      plus, for bot support, a **`GEMS_LEFT?` condition** + gem **sensing** reusing the
      soccer objective-sensing pattern (target = nearest uncollected gem, then the goal).
      Scope fork (user's call): per-level flag (recommended) vs every-gem-level vs a separate
      "Gem Hunt" level type/mode.
- [ ] Fog levels (limited view) as an alternative sensing backdrop (SPEC §7.1). We
      ship the one-program-many-mazes variant instead.
- [ ] `ELSE` branch and `ON_COLOR` condition (intentionally deferred, SPEC §16.4).
- [ ] Archive/"favourite" a solved-level solution for replay (SPEC §16.7).
- [ ] Profile rename (delete is done via long-press; rename UI TBD, SPEC §16.6).
- [ ] Animation-speed + sound toggles surfaced in an in-game settings/pause panel
      (stored in `Settings` and honoured at runtime, but not yet user-editable on device).
- [ ] Debounced periodic autosave of the resume slot mid-edit (currently saved on
      level transitions; power-off mid-edit can lose the in-progress script).
- [ ] Star celebration animation (fly-in stars) + per-level best-stars display.
- [x] Condition picker for sense blocks (WALL/PIT/GOAL cycling) — basic cycling done;
      UX polish (a popup picker) still open.
- [ ] Library picker UI: name entry on save + choose-which-entry on load (currently
      auto-named "Lib N", load pulls the most recent only).
- [ ] Richer multi-ring spiral / varied sensing mazes (current sensing levels use a
      clean single-ring "C-spiral" that the wall-follower provably clears).

## Engine / generation
- [ ] Richer maze topology (branches/dead-ends) beyond the biased-walk corridor.
- [ ] Multi-maze "generalization" set generation tuned so one hand-rule clears all
      (SPEC §7.1 generation note) — basic version in Phase 5.
- [ ] Par tuning per tier (loops/functions rewarded more explicitly).

## Multiplayer modes (the "mix")
- [x] Race (reach goal first) and Sumo (PUSH into a pit) vs House AI or Hotseat.
- [x] **Soccer** (shove a ball into your goal) vs House AI or Hotseat — see Arena mode below.
- [x] Radio battle + Pokemon-style trade (ESP-NOW) — hardware-pending.
- [x] **Puzzle Race (shared maze)**: both players get a 90s timer to write code for the
      SAME maze; whoever's bot ends up closest to the goal (BFS distance, 0 = reached)
      wins. A code-authoring contest, not a live battle. Hotseat lock-in handoff.
- [ ] More mode types: relay, co-op (two bots must both reach goals), king-of-the-hill.

## Arena mode (SPEC §18 — post-campaign capstone)
- [x] Deterministic N=2 tick engine + Race match type + resolution pass (collision
      bounce, fall-out, win check). Determinism proven by byte-identical log test.
- [x] Symmetric arena generation (mirrored hazards, equidistant starts).
- [x] Race demo screen: player's library bot (or wall-follower) vs a house AI bot.
- [x] Sumo match type on the shared tick engine — the attack is **`zap` (`FIRE`)**;
      the old `PUSH` verb was removed and the hunter bot fixed to zap. A trained brain's
      zap shoves too (the interpreter exposes the effective `lastCmd()` so the arena can
      resolve a brain's `N_NEURO` action).
- [x] Fuller AI bot roster — **Rusty** (always-forward), **Bolt** (dasher), **Vex**
      (ENEMY_NEAR hunter), **Ace** (solves the board on the fly) — + an opponent picker UI.
- [x] **Soccer match type** — a walled pitch (`MazeGen::generateSoccerPitch`) with a
      **symmetric 4-tile goal mouth** at each end; bots shove a ball one tile when they step
      onto it; **timed multi-goal matches** with a live **scoreline** (most goals at the cap
      wins; level → **pressure-accumulator tie-break → no draws**: `_pressure` sums ball-to-net
      progress each tick and decides a level scoreline). **Own-goals are impossible**: a push
      that would put the ball in the mover's OWN net auto-triggers a **zap-swap** (bot turns
      180°, ball repositioned goalward) and a jammed ball deflects toward the attacker's goal —
      verified 0% own-goals across the roster (`tools/owngoal_check`). Fully deterministic (ball
      + score folded into the match hash): a random-scatter **referee** rehomes a long-stalled
      loose ball, a **random kickoff** after each goal, and **faster deadlock resolution**
      (8-tick rehome when both bots pin the ball to a wall, vs 28 otherwise) break bot-vs-bot
      deadlocks. Sensing reuses the
      10-input brain layout via `EnemyView.target` — the brain senses the **ball** as its
      objective (aheadness/rightness/distance), its **goal** bearing, and the **rival**
      bearing — so no net-shape or saved-brain change. Hand-codeable too: **`BALL_*`/`NET_*`
      conditions** + **AND/OR compound `if`s** (`if A & B` / `if A | B`). A pre-trained house
      team with soccer names — **Strika / Dribbla / Volley / Nutmeg / Boots**. Trainers: Teach
      (`distillSoccer`/`distillSoccerRnn`), Evolve (soccer fitness), Q-Learn
      (`qTrainSoccer`/`qTrainSoccerRnn`, with own-goal-penalising reward shaping). Wired into
      **vs-Computer, Hotseat, Tournament (Cup/Ladder), and the networked Room**, with **fighter
      names shown in the match header** (`P1 (Strika) / P2 (player v4)`, `You (…)` vs-CPU).
      Demo GIF: `docs/img/soccer.gif`.
- [x] **Loser-levels-up loop.** After a match, **Train** opens the trainer pre-set to the SAME
      sport sparring the SAME opponent (`beginVs` / `_pendTrain*`), the **soccer house team is
      sparrable** in the trainer roster (mode-aware `SOCCER_OPP`), and each trainer screen shows
      a **base-brain label** (`base: a fresh brain / your code's brain  vs  Strika`) so it's
      clear what the training builds on.
- [x] **Pick-screen UX** — the bot/foe pickers got a **Back button + scroll arrows** and
      tap-guards (visible-rows-only, buttons checked first) so scrolling no longer mis-selects.
- [x] **Networked tournament (Room) — VERIFIED on two boards (COM3 + COM5).** ESP-NOW
      multi-device Cup: every board broadcasts its fighter, all ingest into a uuid-sorted
      roster (identical order, no central authority), the host broadcasts a shared SEED, and
      each device replays the SAME bracket locally from that seed (deterministic Arena → same
      champion everywhere). The SEED packet carries a **discipline byte** (Race/Sumo/Soccer),
      so a Room Cup can be any discipline. **BotCards are chunked** (CBEGIN/CCHUNK/CEND +
      per-sender-MAC reassembly), so each board brings its **real saved fighter** — any
      size — not a compact stand-in (only a player with an empty library falls back to a coded
      hunter); the Cup **auto-advances in lockstep** on every device. IA: **Room moved into the
      Radio menu**; **Train-a-fighter promoted to the Arena top menu**.
- [ ] Author the arena bot in the Code view (currently uses the latest library entry).
- [x] Hotseat (2 kids, 1 device) lock-in / handoff screen (Arena + Puzzle Race + Soccer).

## NeuroBot / on-device ML
- [x] **Draw-the-path training** (imitation learning by demonstration). DONE: the "Train
      the brain" page now has a **Draw** button beside Teach/Evolve. The kid taps tiles to
      lay a forward/jump route from the start, then **Learn it** distills the brain to copy
      *that* path (`distillPath` / `pathToProgram` reuse the distill backprop loop). Because
      it trains the working brain in place, picking a saved base first = **fine-tune the old
      net on the newly drawn path** for a level it struggled with.
- [x] **Enemy senses exposed in maze mode** — the campaign `if/until` cycler now offers
      `enemy`/`near` too (no-op without a foe), so a kid can author & test arena-bot logic
      while playing the campaign.
- [x] **"Generalist" prize badge.** DONE (redesigned to be winnable). A frozen reactive
      brain can't plan whole campaign mazes (verified: best ~1-2/50), so the prize uses a
      **Generalization Gauntlet** instead: `game/Gauntlet.cpp` makes reactive-solvable open
      mazes (verified by a memoryless navigator), a **Generalize** button distills the brain
      to imitate that navigator across a training set, and the badge is earned when the
      FROZEN brain clears all `GAUNTLET_MAZES` (10) HELD-OUT test mazes it never trained on.
      Winnability is regression-tested (`test_gauntlet_is_winnable`). Best run shown on Stats.
- [ ] More ML topics the same engine could host (from the lessons discussion):
      regularization/overfitting, batching, reward shaping, exploration-vs-exploitation
      sliders, and a tiny recurrent/memory cell.

## Code architecture / refactor
- [ ] **Migrate `GameScreen` onto the shared `ProgramEditor`.** The authoring UI
      (control pad + nested program list + tap handling) was extracted into
      `src/screens/ProgramEditor.{h,cpp}` and adopted by **Puzzle Race** and **CodeLab**,
      but `GameScreen` still carries its own copy of that code — so the editor currently
      lives in two places. Have `GameScreen` delegate to `ProgramEditor` to remove the
      duplication; do it carefully with full on-device re-verification (all blocks,
      nesting, save/load, neuro train, scrolling, fail-highlight).

## Hardware / platform
- [ ] Resolve the §1.1 2-USB panel question on the real unit (ILI9341 vs ST7789,
      invert/BGR). Currently configured for the known-good ILI9341; flags are
      build-time togglable.
- [x] Two-CYD ESP-NOW radio for networked deterministic matches (SPEC §18.4) — the Room
      tournament is **verified on two boards** (COM3+COM5); the 1:1 Radio battle/trade path
      is built but still wants a focused two-board pass.
- [ ] 4" ST7796 and ESP32-S3 CrowPanel targets (extra envs).

## Tooling (PIO_DEBUG loop)
- [x] **`tools/bot_eval.cpp` host harness + captured output.** Hand-coded vs trained bots across
      maze/battle/soccer + the soccer recipe bake-off, on the real Arena engine. Deterministic;
      the committed run lives in **`docs/bot_eval_output.txt`** (with a reproduce-me header) so the
      eval numbers the docs quote can't silently drift — regenerate after any brain/recipe/physics
      change. (The `.exe` is gitignored; build with MSYS2 g++, see the file header.)
- [x] USB-serial PIO_DEBUG loop: `scripts/shot.py` (screenshot via panel readback ->
      PNG), `scripts/drive.py` (tap + screenshot driver), serial commands S/T/L/X in
      firmware. Verified on the real CYD over COM3 — no WiFi needed.
- [ ] Optional WiFi remote debug API (ElegantOTA `/update`, `/api/screen.jpg`,
      `/api/tap|swipe|status`) as an alternative to the serial loop.
- [x] Real on-device screenshots captured for verification (and available for README).

## Hardware-testing fixes (found by flashing to the real CYD)
- [x] Back/pause button in the game returns to the profile menu (SPEC §10).
- [x] Failure feedback: reddened + shaken character + message on the maze, then a tap
      to the Code view with the failing instruction flashed red (SPEC §8.3) — no
      longer snaps straight back.
- [x] Per-line delete: select a row + a big under-pad DEL button (40px target);
      fixed a dangling fail-highlight pointer after delete.
- [x] Profile edit (name/avatar) from the stats screen, keeping all stats/progress.
- [x] Per-profile UUID for cross-session friend recognition (in the radio friend-card).
- [ ] Touch reliability tuning on the resistive panel (lower Z-threshold / re-cal flow);
      `X` serial command clears calibration to force a re-cal.

## Distribution
- [x] **Online flasher**: a GitHub Pages page (`docs/`) with ESP Web Tools so anyone can
      flash the current build from the browser over USB — no clone/toolchain needed.
      `docs/firmware/gridbot-merged.bin` + `docs/manifest.json` regenerated per release.
- [x] CYD purchase link in the README + flasher page for people who need the hardware.
- [ ] Tagged GitHub releases with versioned merged-firmware artifacts + a changelog.
- [ ] Animated gameplay/arena GIFs auto-regenerated from on-device capture in CI.
