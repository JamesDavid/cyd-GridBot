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
- [ ] Condition picker UX polish for sense blocks (WALL/PIT/GOAL cycling).

## Engine / generation
- [ ] Richer maze topology (branches/dead-ends) beyond the biased-walk corridor.
- [ ] Multi-maze "generalization" set generation tuned so one hand-rule clears all
      (SPEC §7.1 generation note) — basic version in Phase 5.
- [ ] Par tuning per tier (loops/functions rewarded more explicitly).

## Arena mode (SPEC §18 — post-campaign capstone)
- [ ] Sumo match type (`PUSH` verb) and Zap (`FIRE` verb) on the shared tick engine.
- [ ] Built-in AI bot roster (always-forward, wall-follower, ENEMY_NEAR hunter).
- [ ] Library bots as fighters; hotseat lock-in/handoff screen.
- [ ] Symmetric arena generation (mirrored hazards, equidistant starts).

## Hardware / platform
- [ ] Resolve the §1.1 2-USB panel question on the real unit (ILI9341 vs ST7789,
      invert/BGR). Currently configured for the known-good ILI9341; flags are
      build-time togglable.
- [ ] Two-CYD ESP-NOW radio for networked deterministic matches (SPEC §18.4, far later).
- [ ] 4" ST7796 and ESP32-S3 CrowPanel targets (extra envs).

## Tooling (PIO_DEBUG loop)
- [ ] On-device remote debug API: ElegantOTA `/update`, `/api/screen.jpg` JPEG
      screenshot, `/api/tap|swipe|status`, plus `scripts/ota.ps1` + `scripts/shot.ps1`
      so the screen can be observed/driven headlessly. (Not yet built — verification
      so far is native tests + green builds; flashing requires the user's serial port.)
- [ ] Capture real on-device screenshots for the README once a board is attached.
