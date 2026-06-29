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

- [x] Update `docs/CURRICULUM.md` Day 5 to match the newer training findings. **(Done 2026-06-27.)**
  - Rewrote Day 5 step 1: lead with Teach (63%, strong in seconds), **keep the Teach base**, and only adopt a refinement if a **rematch proves it's better** — citing the bake-off (Teach→Evolve 84% helped; Q-Learn 44%/26% often hurt). No longer recommends "Teach then Q-Learn to sharpen" by default.

- [x] Fix the badge hint table length. **(Done 2026-06-27.)**
  - Confirmed: `HINT[ACH_COUNT]` is sized 17 but had only 16 initializers, so `HINT[16]` (Generalist) was a value-initialized `nullptr` — and `BadgesScreen::draw` passes `HINT[i]` straight to `label()` for any *unearned* badge, so an unearned Generalist badge would render a null `char*` (crash/garbage). Added the 17th hint, "ace 10 fresh mazes" (matches `GAUNTLET_MAZES = 10`), and fixed the stale `// ... for 16 badges` row-count comment. Firmware builds.

- [x] Clean up internal "12-input soccer brain" comments. **(Done 2026-06-27.)**
  - Fixed all three stale spots — `src/game/Interpreter.h:20`, `src/game/Interpreter.cpp:239`, and `src/game/Distill.cpp:306` — to say soccer reuses the **same 10 sense slots**, re-meaned (matching `Sensors.h` and the public `10 -> 8 -> 5` shape). Comment-only.

- [x] Re-run or rebuild `tools/bot_eval.cpp` and capture current output. **(Done 2026-06-28.)**
  - The checked-in `bot_eval.exe` exited non-zero with no stdout/stderr in the review environment.
  - The docs lean heavily on these eval numbers, so keeping a reproducible current output log would help prevent future doc drift.
  - Rebuilt with the MSYS2 UCRT64 g++ (`-std=c++17 -O2 -I src -DNATIVE_BUILD`, all `src/game/*.cpp` minus `ProgramJson.cpp`) and captured the deterministic output to **`docs/bot_eval_output.txt`** (with a reproduce-me header). Numbers match the docs: hand-coded soccer 0% win, Teach 20%, Teach→Evolve 47%, Q-Learn 0%/4%, Evolve-from-scratch 0%; maze wall-follower+JUMP 8/16 vs learned-one-maze 1/16; hunter 9-5-2 vs trained fighters.

## General Impressions

The project is coherent and unusually complete for its scope. The architecture has a sensible split between host-testable game logic and device-specific UI/hardware code, and the docs are tied to concrete gameplay, screenshots, and measured results.

The teaching arc is the strongest part: hand-code first, train later, then compare methods with deterministic evaluation. The newer docs are also refreshingly honest about Q-Learn regressions and why one deterministic match is not a representative eval.

The main risks are doc drift and one small UI data bug. The most important conceptual mismatch is neural soccer `zap`: the docs describe a trained-brain behavior that the current interpreter prevents at match runtime.

## Course Improvements for Ages 6+

> **All 10 items below DONE 2026-06-28** — delivered as a single new doc, **`docs/YOUNGER-KIDS-PATH.md`**, a
> prequel/parallel path to the 5-day course, linked from README, COURSE.md, and CURRICULUM.md (with a compact
> younger-kid table added to CURRICULUM.md). Per-item notes inline below.

- [x] Add a dedicated "Younger Kids Path, Ages 6-8" section. **(Done — `docs/YOUNGER-KIDS-PATH.md`: one big idea/day, one sentence to say, one win condition, one optional grown-up explanation per day, using the exact Day 1–5 framing suggested here.)**
  - Keep the same overall arc, but reduce each day to one big idea, one device path, one sentence to say out loud, one win condition, and one optional grown-up explanation.
  - Example framing:
    - Day 1: "The robot follows my list."
    - Day 2: "The robot can check before it moves."
    - Day 3: "Rules need an order."
    - Day 4: "Training means trying and getting better."
    - Day 5: "We test which bot works best."

- [x] Split each day into 15-minute quests. **(Done — every day has 4 named ~15-min "quest cards": Make it move / Fix one mistake / Use jump / Use repeat / Make it react, etc. Full course kept intact for teachers/older kids.)**
  - A younger kid may not stay with a 60-90 minute lesson.
  - Example quest cards:
    - Make it move.
    - Fix one mistake.
    - Use jump.
    - Use repeat.
    - Make it react.
  - Keep the current full course for teachers and older kids, but give parents a shorter path they can run without lesson planning.

- [x] Reduce vocabulary in the youngest path. **(Done — each day delays the formal term to a "Grown-up word:" callout after the behavior: sequence/algorithm/priority/neural network all introduced plain-language first, e.g. "a rule that works again", "a brain with knobs it can change".)**
  - Keep the concepts, but delay formal terms until after the kid sees the behavior.
  - Suggested substitutions:
    - `sequence` -> "a list in order"
    - `condition` -> "check first"
    - `algorithm` -> "a rule that works again"
    - `generalize` -> "works on a new maze"
    - `neural network` -> "a brain with knobs it can change"
  - Add the formal term afterward as "Grown-up word: condition."

- [x] Add exact adult scripts for younger facilitators. **(Done — each day has Say/Do/Ask prompts, plus the printable "Say / Do / Ask" cards section. Includes the "change one block", "point before Run", "did it disobey or follow your rules?" prompts and the "the robot did exactly what the program said" reinforcement up top.)**
  - Short prompts are more useful than explanations for this age group.
  - Examples:
    - "Let's only change one block."
    - "Point where the robot will go before pressing Run."
    - "What did it see?"
    - "Did it follow your rule or did it disobey?"
  - Reinforce that the robot usually did exactly what the program said, even when the result was wrong.

- [x] Make Day 4 less ML-heavy for ages 6-8. **(Done — Day 4 opens with a callout reframing it to ONE idea: "pressing a button makes the robot practise and get better." Teach/Q-Learn/Evolve distinctions explicitly deferred to older-kid / optional "boss level"; quests are press Teach → watch the curve → before-vs-after → save.)**
  - Younger path should focus on pressing Teach, watching improvement, comparing before/after, and saying "it learned from practice."
  - Treat Teach / Q-Learn / Evolve distinctions as older-kid or optional "boss level" material.

- [x] Add a "Parent Rescue Guide." **(Done — a dedicated table covering all five suggested situations (frustrated → easy win; changing everything → one block; heading → be the robot; button-mashing → predict before Run; lost ML interest → funny bot + tournament) plus "robot is cheating" and "stuck on a maze".)**
  - Useful quick fixes:
    - Kid is frustrated -> go back one level and get a quick win.
    - Kid keeps changing everything -> change one block only.
    - Kid does not understand heading -> stand up and become the robot.
    - Kid wants to mash buttons -> require "Prediction Before Run."
    - Kid loses interest in ML -> train a funny bot and tournament it.

- [x] Add non-verbal checkpoints for young learners. **(Done — every day ends with a "Non-verbal checkpoint": trace next move with a finger / make one program work on two mazes / find the wrong block / point to the bot that learned / choose between hand-coded and trained — matching the suggested show-me list.)**
  - Do not rely only on verbal explanation.
  - Show-me checkpoints:
    - Can they predict the next move with a finger?
    - Can they find the wrong block?
    - Can they make one program work on a second maze?
    - Can they choose between a hand-coded bot and a trained bot after watching both?

- [x] Tone down exact eval numbers in the youngest path. **(Done — Day 5 uses the suggested phrasing ("This rule worked on lots of mazes", "The trained soccer bot usually wins", "One match can trick us, so we play a few") and explicitly points the win-rate tables to the teacher layer (TRAINING_FINDINGS.md / COURSE.md).)**
  - Keep the tables in the teacher/adult layer.
  - Use younger-kid phrasing:
    - "This rule worked on lots of mazes."
    - "The trained soccer bot usually wins."
    - "One match can trick us, so we try a few."

- [x] Add printable or slide-friendly "Say / Do / Ask" cards. **(Done — a one-card-per-day section in the suggested Say/Do/Ask + Celebrate format, each a self-contained ~15-min sitting a grown-up can run with no lesson planning.)**
  - One page per day would help parents and teachers run the course quickly.
  - Suggested format:
    - Say: "A program is a list."
    - Do: "Beat levels 1-3."
    - Ask: "What happens if we swap these?"
    - Celebrate: "You debugged it."

- [x] Add a compact younger-kid table to `COURSE.md` and/or `CURRICULUM.md`. **(Done — the "Day / Younger Kid Version / Skip Until Later" table is in `CURRICULUM.md` (new "Same week, for ages ~6–8" subsection) and at the top of `YOUNGER-KIDS-PATH.md`, matching the suggested rows.)**
  - Suggested table:

    | Day | Younger Kid Version | Skip Until Later |
    |---|---|---|
    | 1 | Make a list, fix one wrong turn | CSTA terms |
    | 2 | Check wall, then turn | formal "algorithm" |
    | 3 | First rule wins | win-rate tables |
    | 4 | Press Teach, watch practice improve | trainer taxonomy |
    | 5 | Try bots, keep the one that wins | statistical caveats |

- [x] Scan for and fix any visible "cirriculum" typo. **(Done 2026-06-27.)**
  - Repo-wide case-insensitive scan found no "cirriculum" anywhere in the source/docs (the only occurrence was this checklist's own flag). All 9 files use the correct "curriculum" spelling — nothing to change.
