# Initial prompt

This file records the prompt that kicked off the autonomous build of GridBot, as
requested ("write this prompt or an improved version to initial_prompt.md").

## Verbatim goal (as given)

> use IMPLEMENTATION_STEPS.md to implement SPEC.md for CYD-ESP32-2432S028R.md using the
> PIO_DEBUG.md loop. check in to git at each step and when done create a readme.md using
> the README.md guidelines. write this prompt or an improved version to initial_prompt.md
> and complete autonomously without input unless absolutely necessary.
> (Clarification: SPEC and IMPLEMENTATION_STEPS were accidentally identical at first; they
> should be different — IMPLEMENTATION_STEPS is the process playbook, SPEC is the substance.)

## Improved / operational version

**Mission:** Build the GridBot firmware — a kid-facing "program your character through the
maze" game — for the **CYD ESP32-2432S028R** board, autonomously, phase by phase.

**Source docs and their roles:**
- `SPEC.md` — the single source of truth for *what* to build. If anything disagrees with
  it, SPEC wins.
- `IMPLEMENTATION_STEPS.md` — the *process* playbook: phase order, the per-slice loop, the
  three verification channels, acceptance criteria, and when to stop for a human.
- `CYD-ESP32-2432S028R.md` — the hardware truth (pins, panel driver, touch, backlight,
  partitions). Always take board config from here; never re-derive pins or guess the panel.
- `PIO_DEBUG.md` — the build → flash → read-serial → see-the-screen iteration loop.
- `README_GUIDELINES.md` — how to write the final `README.md`.

**Working rules:**
1. Follow SPEC §17 / IMPLEMENTATION_STEPS phase order. One small slice → verify → commit.
2. Three verification channels: (a) **native Unity tests** (`env:native`) for engine logic;
   (b) **on-device self-test** (`-DSELFTEST`) printing PASS/FAIL over serial; (c) **on-screen
   capture** for pixels. Without a board attached, (a) + green builds are the standing gate;
   (b)/(c) run once hardware is connected.
3. **Compile only — never upload/flash to a board** until the human supplies the serial port
   (the human has multiple CYDs attached). `platformio run` builds without uploading.
4. Commit at every green slice; tag at phase boundaries (`v0.1`, `v0.2`, …).
5. Honor SPEC invariants: engine/shell separation; one AST shape everywhere; dirty-rect
   rendering only (no full framebuffer); no RNG in the arena tick loop.
6. Take SPEC §16 defaults rather than blocking on open decisions; note each choice.
7. When done, write `README.md` per `README_GUIDELINES.md` (+ `HARDWARE_SETUP.md`,
   `BACKLOG.md`, `LICENSE`).

**Build commands (this machine):**
- Firmware (no upload): `C:/Users/James/.platformio/penv/Scripts/platformio.exe run -e cyd_gridbot`
- Self-test firmware:   `… run -e cyd_gridbot_selftest`
- Host tests:           `export PATH="/c/msys64/ucrt64/bin:$PATH"; … test -e native`
