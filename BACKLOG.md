# GridBot — Backlog

The living list of planned/possible work. The README shows only the juiciest
highlights and links here. Items are grouped; checked = done, unchecked = future.

## Campaign polish (deferred niceties)
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
