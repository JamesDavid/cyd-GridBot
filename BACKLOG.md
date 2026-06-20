# GridBot — Backlog

The living list of planned/possible work. The README shows only the juiciest
highlights and links here. Items are grouped; checked = done, unchecked = future.

## Playtest ideas — make it more fun (from on-device playtesting)
- [ ] **Achievements / badges**: first win, first 3-star, "used a loop", "wrote a
      function", first sensing clear, first arena win, traded a bot, drew a sprite,
      win-streak milestones. Show on the stats screen; celebrate on unlock.
- [ ] **Juice**: smooth tween between tiles, a little dust/trail, particle burst +
      star fly-in on win, gentle screen-shake on bonk (character shake already added).
- [ ] **Character emotes**: happy bounce on win, dizzy/✖-eyes on bonk, splash on a pit.
- [ ] **Drag-to-reorder** program lines; an **Undo** button for edits.
- [ ] **Level biomes/themes**: vary the floor/wall palette per level band (grass,
      cave, ice, space) so progress feels visual.
- [ ] **In-game speed slider** + a one-line **hint** ("par is N") toggle.
- [ ] **Daily/shared-seed challenge**: everyone plays the same maze from a seed code.
- [ ] **Coin collectibles + a tiny shop** for sprite colours / themes (ties to the
      pixel editor and the coin-economy motif).
- [ ] **Music**: a short chiptune loop + win jingle (gate behind settings.sound).

## Campaign polish (deferred niceties)
- [x] Brick-red walls + void (background-colour) pits so hazards read clearly.
- [x] Program list scrolls (auto-follows newest + tappable scrollbar) and the pane
      extends full-height — no command-count limit on screen.
- [x] Bigger chrome titles; compressed unlock curve (sensing at L22, not L55);
      multi-maze challenges interspersed every 5th level past sensing.
- [x] Failure feedback on the maze before returning to code (see below).
- [ ] Smooth tile-to-tile character tween during animation (currently discrete hops).
- [ ] Press animation on buttons (brief depress + release) beyond the colour change.
- [ ] Optional persistent mini-map in the Code view (SPEC §10; per-profile setting,
      default off). Field reserved in `Settings.miniMap`.
- [ ] Collectibles (coins/stars to grab before the goal) — tile types `COIN`/`STAR`
      already reserved in the model (SPEC §16.3). Off for v1.
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
- [x] Radio battle + Pokemon-style trade (ESP-NOW) — hardware-pending.
- [ ] **Puzzle Race (shared maze)**: both players get a timer to write code for the
      SAME maze; whoever's bot gets furthest before bonking/falling (or closest to the
      goal) wins. A code-authoring contest, not a live battle. One of several modes.
- [ ] More mode types: relay, co-op (two bots must both reach goals), king-of-the-hill.

## Arena mode (SPEC §18 — post-campaign capstone)
- [x] Deterministic N=2 tick engine + Race match type + resolution pass (collision
      bounce, fall-out, win check). Determinism proven by byte-identical log test.
- [x] Symmetric arena generation (mirrored hazards, equidistant starts).
- [x] Race demo screen: player's library bot (or wall-follower) vs a house AI bot.
- [ ] Sumo match type (`PUSH` verb) and Zap (`FIRE` verb) on the shared tick engine.
- [ ] Fuller AI bot roster (always-forward, ENEMY_NEAR hunter) + opponent picker UI.
- [ ] Author the arena bot in the Code view (currently uses the latest library entry).
- [ ] Hotseat (2 kids, 1 device) lock-in / handoff screen.

## Hardware / platform
- [ ] Resolve the §1.1 2-USB panel question on the real unit (ILI9341 vs ST7789,
      invert/BGR). Currently configured for the known-good ILI9341; flags are
      build-time togglable.
- [ ] Two-CYD ESP-NOW radio for networked deterministic matches (SPEC §18.4, far later).
- [ ] 4" ST7796 and ESP32-S3 CrowPanel targets (extra envs).

## Tooling (PIO_DEBUG loop)
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
