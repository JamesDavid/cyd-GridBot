\# GridBot — Implementation Steps (Autonomous Build Playbook)



\## Goal

Implement `spec.md` \*\*phase by phase, autonomously\*\*, targeting the board described in `cyd-esp32-2432s028r.md`, validating every change through the loop in `pio\_debug.md`. Each phase in `spec.md` §17 is a self-contained milestone: build it, prove it (in headless tests and/or on the board), commit, advance — without waiting on human input except at the explicit checkpoints listed at the end.



\## Reference files (this doc adds \*process\*; substance lives in these)

\- \*\*`spec.md`\*\* — the single source of truth for \*what\* to build. Every step cites a section (e.g. §8 = interpreter). If this doc and `spec.md` ever disagree, \*\*`spec.md` wins\*\*.

\- \*\*`cyd-esp32-2432s028r.md`\*\* — the hardware truth for \*this\* board: pins, display driver, touch, backlight, partitions, LGFX config. \*\*Always take board config from here; never re-derive pins or guess the panel driver.\*\* (`spec.md` §1.1 flags the 2-USB display gotcha — resolve it from this file.)

\- \*\*`pio\_debug.md`\*\* — the build → flash → read-serial → diagnose iteration loop. \*\*Run this loop after every meaningful change.\*\* It is how you observe results on the board.



\## How autonomy actually works on this hardware (read first)

You can compile, flash, \*\*read the serial monitor, and observe the display\*\* — `pio\_debug.md` describes how to capture/see the screen. So all three verification channels are autonomous:



1\. \*\*Headless logic tests (`env:native`).\*\* The platform-agnostic core — MazeGen, Maze, AST, Interpreter, scoring, Arena engine (the \*engine\* half of the spec's engine/shell split) — compiles and runs on the host with no board. Write Unity tests here. Fast, deterministic, no flashing. \*\*Most correctness is proven here.\*\*

2\. \*\*On-device serial self-test (`-DSELFTEST`).\*\* A build flag that boots the board into a headless harness: runs the engine against known fixtures and prints `PASS`/`FAIL` lines the `pio\_debug.md` loop reads. Confirms the same logic under real ESP32 memory/types.

3\. \*\*On-screen verification (via `pio\_debug.md`).\*\* For anything whose output is pixels (layout, sprites, rendering, animation), use the screen-capture/observation step in `pio\_debug.md` to check it \*\*yourself\*\* — correct colors (no inversion/mirroring), elements placed and sized per the spec, the character animating across the right tiles. No human needed for routine visual confirmation.



Determinism (`spec.md` §6.2 seeds, §18.1 no-RNG matches) is what makes channels 1 and 2 possible: identical inputs must always produce identical results. Channel 3 closes the loop on rendering.



\## Operating rules

\- Follow `spec.md` §17 \*\*phase order\*\*. Don't start a phase until the previous phase's acceptance criteria pass.

\- \*\*Smallest-slice loop:\*\* implement one slice → verify → commit. Never batch a whole phase before testing.

\- After each slice, run `env:native` tests for pure logic, then the \*\*`pio\_debug.md`\*\* loop for anything on-board.

\- \*\*Commit\*\* at every green slice; \*\*tag\*\* at phase boundaries (`v0.1`, `v0.2`, …).

\- Honor spec invariants: engine/shell separation; one AST shape across program / library / function / AI bot (§11.1, §18); \*\*no full-screen framebuffer — dirty-rect only\*\* (§12); \*\*no RNG in the arena tick loop\*\* (§18.1).

\- Keep all board config in \*\*one\*\* LGFX config unit sourced from `cyd-esp32-2432s028r.md`, so a board revision is a single-file change.

\- \*\*Defer, don't drop:\*\* whenever you skip or only partially implement a spec detail (a polish item, a nice-to-have, a deferred §16 decision), append it to `backlog.md` instead of leaving it implicit. Keep `backlog.md` as the living source of truth for everything not yet built, grouped by area; the README later shows only the highlights and links to it.



\## The per-phase loop (apply to every phase below)

1\. Re-read the cited `spec.md` section(s).

2\. Implement the next smallest slice (engine before shell where possible).

3\. Add/extend `env:native` Unity tests for new engine logic; add `-DSELFTEST` fixtures for on-device logic.

4\. Run host tests; then run the \*\*`pio\_debug.md`\*\* loop to build/flash/read serial.

5\. Compare against the phase's \*\*acceptance criteria\*\*.

6\. On failure, iterate through the loop — the serial output is your debugger.

7\. Commit. If the slice changed on-screen output, verify it yourself with the \*\*`pio\_debug.md` screen capture\*\* before committing; escalate to the human only for subjective judgment (animation \*feel\*, touch ergonomics).

8\. When every criterion for the phase passes, tag it and advance.



\---



\## Phase 0 — Bootstrap \& hardware smoke test

\- \*\*Objective:\*\* a buildable project that proves display + touch on the real board \*before\* any game logic.

\- \*\*Do:\*\* scaffold the repo; write `platformio.ini` per §15 with board/libs and the \*\*LGFX config + pins from `cyd-esp32-2432s028r.md`\*\*; add `partitions.csv` with a LittleFS region (§14); stand up `env:native` + Unity. Draw a test pattern and print touch coordinates to serial.

\- \*\*Autonomous check:\*\* both envs build; native scaffold runs; the `pio\_debug.md` loop flashes cleanly and serial prints boot banner + live touch events.

\- \*\*On-screen check (via `pio\_debug.md`):\*\* capture the screen and confirm colors are correct (settle the §1.1 ST7789 / invert / BGR question from the board file — gross inversion/mirroring shows plainly in the capture), no offset, and touch lands where pressed (serial).



\## Phase 1 — MVP  (spec v0.1)

\- \*\*Objective:\*\* hardcoded small maze, Tier-1 program, build → run → win/fail on device. §3 (state machine), §5 (Maze + AST §5.4), §8 (interpreter, Tier-1 verbs), §10 (Maze view, control-pad Code view, view toggle + hold-to-peek), §11 (loop).

\- \*\*Engine first (native tests):\*\* Maze model; AST; steppable Interpreter for Forward / TurnL / TurnR; outcomes `OK / WIN / BONK / FELL / DONE\_NO\_WIN`.

\- \*\*Shell:\*\* maze render (dirty-rect, §12); control pad; program list; toggle; RUN animates via a timer-stepped interpreter.

\- \*\*Autonomous check:\*\* native tests assert interpreter outcomes against hand-built fixtures (known maze + program → WIN; wall → BONK; pit → FELL). `-DSELFTEST` runs the same on-board, prints PASS/FAIL.

\- \*\*On-screen check (via `pio\_debug.md`):\*\* control pad laid out per §10, maze + character render correctly, animation steps the right tiles. (Toggle/peek \*feel\* is the one subjective item worth a human glance.)

\- \*\*Acceptance:\*\* on device, author a Tier-1 program, RUN, get correct win/fail; engine tests green.



\## Phase 2 — Generation \& scoring  (spec v0.2)

\- \*\*Objective:\*\* deterministic, always-solvable procedural mazes; difficulty curve; star scoring. §6, §7, §9.

\- \*\*Engine:\*\* seeded MazeGen (carve solution path → decorate → BFS verify); par + star calculation.

\- \*\*Autonomous check (core safety property):\*\* native test generates \*\*many seeds across the difficulty curve and asserts BFS-solvable every time\*\* — exercise this hard (hundreds+ of seeds). Determinism test: same seed → identical maze. Star-math fixtures.

\- \*\*Acceptance:\*\* large batches of generated mazes verify solvable in native tests; `-DSELFTEST` confirms on real board RNG/types; stars correct.



\## Phase 3 — Profiles \& persistence  (spec v0.3)

\- \*\*Objective:\*\* profile create/select; LittleFS persistence; stats; resume slot. §4, §5.1, §9, §11.1, §14.

\- \*\*Autonomous check:\*\* round-trip tests — serialize profile → write → reboot/remount → read → assert equal (run as on-device serial self-test since LittleFS is board-side; mock host FS in native). Index-file integrity.

\- \*\*On-screen check (via `pio\_debug.md`):\*\* profile menu + name entry render correctly. (If §16 item 1 — input method — is still open, that's a human \*decision\*, separate from the rendering check.)

\- \*\*Acceptance:\*\* create profile, advance a level, power-cycle, progress + stats restored (serial-verified).



\## Phase 4 — Unlock tiers  (spec v0.4)

\- \*\*Objective:\*\* Backward (Tier 1.5) → Jump → Repeat → Functions; corner growth slots + E-bracket nesting + `MAIN | F1 | F2`; character selection; audio/LED. §7/§8.1, §10, §4.1, §13.

\- \*\*Engine:\*\* extend the interpreter for JUMP, REPEAT (count), CALL/functions (no recursion) via the explicit call stack; sticky per-profile unlocks.

\- \*\*Autonomous check:\*\* native tests with nested fixtures — REPEAT body runs count×; a function call returns correctly; step cap honored; Backward semantics.

\- \*\*On-screen check (via `pio\_debug.md`):\*\* corner growth slots, E-bracket nesting, and character/goal-token pairing (§4.1) render as specced.

\- \*\*Acceptance:\*\* every construct executes correctly (tests green) and is authorable on device.



\## Phase 5 — Sensing \& library  (spec v0.5+)

\- \*\*Objective:\*\* `REPEAT\_UNTIL` / `IF` over WALL/PIT/GOAL; fog or multi-maze generalization; library Save/Load; (optional) collectibles. §8 sensing, §7.1, §11.1.

\- \*\*Engine:\*\* condition evaluator; `REPEAT\_UNTIL` with step-cap guard; library entries as named ASTs (same shape as function bodies).

\- \*\*Autonomous check (the showcase test):\*\* native test where \*\*one general program (a wall-follower) clears a generated multi-maze set\*\* (§7.1) — proves sensing + generalization in a single assertion. Library round-trip persistence.

\- \*\*Acceptance:\*\* wall-follower clears the set in native tests and on-device self-test; Save/Load library survives reboot.



\## Phase 6 — Arena  (spec v0.6+)

\- \*\*Objective:\*\* multi-bot match engine; \*\*Race first\*\*, then Sumo/Zap via a swappable rule module; AI bots + hotseat; `ENEMY\_AHEAD` / `ENEMY\_NEAR`. §18.

\- \*\*Engine:\*\* N interpreter contexts + a tick resolution pass; symmetric arena generation; match-type modules; deterministic outcome.

\- \*\*Autonomous check (key):\*\* \*\*determinism test — same seed + same two programs → byte-identical match log on every run\*\* (proves no stray RNG, and unlocks replays/spectate for free). Race win-detection; push/fire resolution fixtures.

\- \*\*On-screen check (via `pio\_debug.md`):\*\* two-bot match readability, scoreboard, and hotseat lock-in/handoff screens render correctly.

\- \*\*Acceptance:\*\* a Race match resolves deterministically and correctly in tests + on device; the built-in AI roster and a saved library bot can fight; hotseat flow works.



\---



\## When to stop and ask the human

\- Any `spec.md` §16 \*\*open decision\*\* still unresolved when you reach it (name-entry method, app name, collectibles on/off, fog vs multi-maze, archive-solutions, Arena flavor order).

\- The §1.1 \*\*display-driver\*\* question, if `cyd-esp32-2432s028r.md` doesn't fully settle it.

\- Subjective UX calls only — does the animation \*feel\* right, does resistive touch feel responsive — where a screen capture can't settle it. Routine visual correctness you verify yourself via `pio\_debug.md`.

\- Any spec ambiguity, or any action outside building this firmware.



Otherwise: \*\*proceed autonomously, one green slice at a time, phase by phase.\*\*

