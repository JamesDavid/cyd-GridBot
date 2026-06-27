# ChatGPT Review Feedback

Date: 2026-06-27

This is a working checklist from a read-only review of the codebase, `docs/COURSE.md`, `docs/slides/*`, and the other `docs/*.md` files. Check items off as they are addressed.

## Action Items

- [x] Fix the neural soccer `zap` documentation. **(Done 2026-06-27 — fixed by changing the *code* to match the docs, not vice-versa.)**
  - Resolved together with the item below: neural soccer `zap` now executes at match runtime, so the "a trained brain can use the swap" wording is now **true**.
  - Brain Cam output 4 relabelled from `-` to `swap` (`src/screens/BrainGraph.cpp` + `.h`); fixed the stale "greys to `-` / no zapping on the pitch" lines in `README.md` and `docs/AI-CODING-GUIDE.md`, and the day4 slide note.
  - Clarified `src/game/Arena.cpp` comment ("no SUMO zap (no damage) — its zap is the ball-swap, §2c").

- [x] Decide whether neural soccer should support `zap` at runtime. **(Done 2026-06-27 — chose: allow neural soccer zap.)**
  - The ball-swap is a real, desirable mechanic, so we kept `NA=5` in `qTrainSoccer`/`qTrainSoccerRnn` and **removed the interpreter remap** (`src/game/Interpreter.cpp`) that nullified it. Now training (`qSoccerStep` models the swap) and inference (Arena §2c executes it) agree.
  - Safety verified: a stray soccer fire is a harmless wasted tick (no sumo damage in soccer; bounded by the match step cap), so the old "freeze forever" concern doesn't apply.
  - Re-ran `tools/bot_eval.cpp` (deterministic, byte-identical across two runs): only the reward-refined recipes moved — Q-vs-cone 37%→**44%**, Q-vs-live 29%→**26%**, evolve-from-scratch 23%→**13%**; Teach (63%) / Teach→Evolve (84%) and all hand-coded-vs-trained numbers unchanged. Every conclusion holds; updated the numbers in TRAINING_FINDINGS, AI-CODING-GUIDE, COURSE, README, and slides day4/day5. 109/109 native tests pass; firmware builds.

- [ ] Update `docs/CURRICULUM.md` Day 5 to match the newer training findings.
  - It still recommends: Teach a dribbler, then Q-Learn to sharpen it.
  - `COURSE.md`, `AI-CODING-GUIDE.md`, `TRAINING_FINDINGS.md`, and `slides/day5.html` now say Q-Learn often hurt in the measured bake-off, and Teach -> Evolve was best in that eval.
  - The one-page curriculum should recommend keeping the Teach base and using refinement only when rematches prove it helps.

- [ ] Fix the badge hint table length.
  - `ACH_COUNT` is 17 and includes `Generalist`.
  - `HINT` in `src/screens/BadgesScreen.cpp` has only 16 entries.
  - Add a Generalist hint so unearned badge rendering never reads past the initialized hint list.

- [ ] Clean up internal "12-input soccer brain" comments.
  - Public docs mostly state the correct `10 -> 8 -> 5` shape.
  - Comments in `src/game/Interpreter.h` and `src/game/Interpreter.cpp` call soccer a richer "12-input" brain.
  - `src/game/Sensors.h` correctly says soccer reuses the same 10 slots with different meanings.

- [ ] Re-run or rebuild `tools/bot_eval.cpp` and capture current output.
  - The checked-in `bot_eval.exe` exited non-zero with no stdout/stderr in the review environment.
  - The docs lean heavily on these eval numbers, so keeping a reproducible current output log would help prevent future doc drift.

## General Impressions

The project is coherent and unusually complete for its scope. The architecture has a sensible split between host-testable game logic and device-specific UI/hardware code, and the docs are tied to concrete gameplay, screenshots, and measured results.

The teaching arc is the strongest part: hand-code first, train later, then compare methods with deterministic evaluation. The newer docs are also refreshingly honest about Q-Learn regressions and why one deterministic match is not a representative eval.

The main risks are doc drift and one small UI data bug. The most important conceptual mismatch is neural soccer `zap`: the docs describe a trained-brain behavior that the current interpreter prevents at match runtime.

## Course Improvements for Ages 6+

- [ ] Add a dedicated "Younger Kids Path, Ages 6-8" section.
  - Keep the same overall arc, but reduce each day to one big idea, one device path, one sentence to say out loud, one win condition, and one optional grown-up explanation.
  - Example framing:
    - Day 1: "The robot follows my list."
    - Day 2: "The robot can check before it moves."
    - Day 3: "Rules need an order."
    - Day 4: "Training means trying and getting better."
    - Day 5: "We test which bot works best."

- [ ] Split each day into 15-minute quests.
  - A younger kid may not stay with a 60-90 minute lesson.
  - Example quest cards:
    - Make it move.
    - Fix one mistake.
    - Use jump.
    - Use repeat.
    - Make it react.
  - Keep the current full course for teachers and older kids, but give parents a shorter path they can run without lesson planning.

- [ ] Reduce vocabulary in the youngest path.
  - Keep the concepts, but delay formal terms until after the kid sees the behavior.
  - Suggested substitutions:
    - `sequence` -> "a list in order"
    - `condition` -> "check first"
    - `algorithm` -> "a rule that works again"
    - `generalize` -> "works on a new maze"
    - `neural network` -> "a brain with knobs it can change"
  - Add the formal term afterward as "Grown-up word: condition."

- [ ] Add exact adult scripts for younger facilitators.
  - Short prompts are more useful than explanations for this age group.
  - Examples:
    - "Let's only change one block."
    - "Point where the robot will go before pressing Run."
    - "What did it see?"
    - "Did it follow your rule or did it disobey?"
  - Reinforce that the robot usually did exactly what the program said, even when the result was wrong.

- [ ] Make Day 4 less ML-heavy for ages 6-8.
  - Younger path should focus on pressing Teach, watching improvement, comparing before/after, and saying "it learned from practice."
  - Treat Teach / Q-Learn / Evolve distinctions as older-kid or optional "boss level" material.

- [ ] Add a "Parent Rescue Guide."
  - Useful quick fixes:
    - Kid is frustrated -> go back one level and get a quick win.
    - Kid keeps changing everything -> change one block only.
    - Kid does not understand heading -> stand up and become the robot.
    - Kid wants to mash buttons -> require "Prediction Before Run."
    - Kid loses interest in ML -> train a funny bot and tournament it.

- [ ] Add non-verbal checkpoints for young learners.
  - Do not rely only on verbal explanation.
  - Show-me checkpoints:
    - Can they predict the next move with a finger?
    - Can they find the wrong block?
    - Can they make one program work on a second maze?
    - Can they choose between a hand-coded bot and a trained bot after watching both?

- [ ] Tone down exact eval numbers in the youngest path.
  - Keep the tables in the teacher/adult layer.
  - Use younger-kid phrasing:
    - "This rule worked on lots of mazes."
    - "The trained soccer bot usually wins."
    - "One match can trick us, so we try a few."

- [ ] Add printable or slide-friendly "Say / Do / Ask" cards.
  - One page per day would help parents and teachers run the course quickly.
  - Suggested format:
    - Say: "A program is a list."
    - Do: "Beat levels 1-3."
    - Ask: "What happens if we swap these?"
    - Celebrate: "You debugged it."

- [ ] Add a compact younger-kid table to `COURSE.md` and/or `CURRICULUM.md`.
  - Suggested table:

    | Day | Younger Kid Version | Skip Until Later |
    |---|---|---|
    | 1 | Make a list, fix one wrong turn | CSTA terms |
    | 2 | Check wall, then turn | formal "algorithm" |
    | 3 | First rule wins | win-rate tables |
    | 4 | Press Teach, watch practice improve | trainer taxonomy |
    | 5 | Try bots, keep the one that wins | statistical caveats |

- [ ] Scan for and fix any visible "cirriculum" typo.
  - Use "curriculum" consistently in teaching docs and navigation text.
