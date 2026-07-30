# M60 — CrystalForge sim lab, battle observer, test runner

Authorized 2026-07-24 with the M59–M60 program (plan approved by the owner);
implemented 2026-07-25 immediately after M59 on the same base (`7c8f32d` +
M59). Designer-facing manual: `docs/editor_guide.md` (Sim Lab / Test Runner
sections); architecture: `docs/technical_design.md` §17.

## A. Status

**◑ implemented, awaiting manual approval** — set 2026-07-25. Evidence in §F.

## B. Goal

Finish CrystalForge: a **sim lab** (party/opponent builders, seed sweeps with
aggregates and per-skill / per-combatant telemetry, delta comparison, report
export) and a **per-category test runner** (spawn `crystal_tests.exe` with
filters, stream results live) — powered by a **record-only battle observer**
in shared battle code that is provably outcome-neutral (no rules bump).

## C. As implemented

### F1 — battle observer (the one shared-code touch)

- `src/battle/BattleObserver.hpp`: `BattleEvent` (Action / Damage / Heal /
  KO / Revive; **amounts are effective**, post-clamp) + the `BattleObserver`
  interface. `Battle` gains a **non-owning `observer` pointer, default
  null** (deviation from the plan's "optional simulate() parameter" — see
  §E). Every emit is `if (observer) …` and nothing else: no rolls, no state
  reads, no branching.
- Emit sites: `attack`/`useSkill`/`useItem` entries (Action; the item emit
  sits after the `requiresBossId` keep-guard); `applyDamage` (effective
  damage + KO, including the god-mode clamp path); the **poison tick**
  (which bypasses `applyDamage` — the M53 lesson, honoured again); heal
  paths (skill heal, item heal, Lifedrink) with before/after effective
  amounts; revive paths (skill, item, and the King's court-rise).
- `src/editor/BattleRecorder.hpp` tallies events into per-action
  (uses/damage/healing) and per-combatant (dealt/taken/healed/KOs/revives)
  totals with honest attribution: damage landing on the current actor
  (thorns/counter retaliation) and poison stay unattributed rather than
  polluting a skill's numbers.
- **Parity proven** (`tests/test_battle_observer.cpp`): the same seeded
  status-heavy battle (poison, Blind rolls, healers) with and without a
  recorder — identical outcome, rounds, every unit's HP/MP/status count,
  and **`rollCursor`**, across three seeds; plus exact per-unit HP
  reconciliation (`ΔHP == healedReceived − taken`) and a bit-repeatability
  regression case. **No `kBattleRulesVersion` bump** (stays 11).

### F2 — sim lab

- `src/editor/SimLab.{hpp,cpp}` (pure): `SimLabConfig` (≤4 member specs —
  class/weapon/armor/accessory/passive — level, opponent mode, seeds) →
  `runSweep` builds the party through `createCharacter`/`refreshCharacter`,
  the opponent through `buildOpponent`, and resolves N seeded battles via
  the real `battle::simulateInPlace` with a per-run recorder merged into
  the result. Opponent modes: **manual team** (any enemies at 50–600 %
  scale), **boss + authored court**, **Boss Rush fight #N**, **Endless wave
  W**, and **the King** (all castle presets from `game/Castle`). Aggregates:
  win rate, avg/median/min/max rounds, avg party HP, party KOs, danger tier
  (`danger::assess`).
- Shell surface (`EditorShell`, sidebar entry "Sim Lab"): a config pane
  (steppers + the shared filterable pickers, slot-aware for gear; one-key
  bare/median/best presets) and a results pane with **delta columns against
  the previous run** — run, tweak content or config, run again. Export
  writes timestamped markdown (with deltas) or CSV into the git-ignored
  **`reports/`** (added to `.gitignore`).

### F3 — test runner

- `src/platform/Process.{hpp,cpp}`: `ProcessRunner` — CreateProcessW with a
  stdout+stderr pipe, a background reader thread draining into a
  mutex-guarded line queue the shell polls per frame, exit code on
  completion, terminate-on-destroy. Windows-only behind the platform
  interface; **compiled ONLY into `crystal_editor_core`**, which the game
  never links — the "no shell execution from game code" rule holds
  structurally, not by convention.
- `src/editor/TestRunner.{hpp,cpp}`: five designer categories (enemies &
  bosses / skills & status / items & economy / classes & passives / content
  validation) + "Everything", mapped to whole test FILES via Catch2's
  `--filenames-as-tags` (**verified present in the pinned v3.15.1** and
  exercised live: `[#test_danger]` ran exactly its 6 cases). Shell surface:
  category list, live-streaming output pane following the tail, pass/fail
  banner from the exit code, `Del` stops a run.

## D. Files changed (M60)

- **Source:** `src/battle/BattleObserver.hpp` (new), `src/battle/Battle.hpp`
  (+observer member/fwd-decl), `src/battle/Battle.cpp` (emit sites),
  `src/editor/BattleRecorder.hpp`, `src/editor/SimLab.{hpp,cpp}`,
  `src/editor/TestRunner.{hpp,cpp}` (new), `src/platform/Process.{hpp,cpp}`
  (new), `src/editor/EditorShell.{hpp,cpp}` (SimLab/Tests screens),
  `src/editor/EditorValidation.{hpp,cpp}` (party/gear pickers shared).
- **Tests:** `tests/test_battle_observer.cpp`, `tests/test_editor_simlab.cpp`
  (new), `tests/CMakeLists.txt`.
- **Build:** `CMakeLists.txt` (editor-lib sources incl. `Process.cpp`),
  `.gitignore` (`reports/`).
- **Docs:** this note, `docs/editor_guide.md`, `docs/technical_design.md`
  §17, `docs/milestones.md`.

## E. Plan deviations

- **The observer is a `Battle` member, not a `simulate()` parameter.** The
  plan sketched threading an observer pointer through the simulator's
  signatures; implementation showed the chokepoints live in `Battle`'s own
  methods, so a parameter would have re-signatured a dozen shared methods —
  exactly the reviewability risk CLAUDE.md warns about. A non-owning,
  default-null pointer member preserves Battle's value semantics
  (trivially copyable), needs zero signature changes, and delivers the
  identical guarantee — proven by the same parity test the plan required.
- **Sweeps run synchronously** (no progress thread): 1000 seeds resolve in
  well under the annoyance threshold, and the aggregation API kept the
  progress-callback hook for the future.
- No other deviations.

## F. Automated validation (all run in this session, 2026-07-25)

- Debug build clean; **`[editor]` battery 19 cases / 3101 assertions green**
  (M59's 12 + observer parity/reconciliation + sim-lab determinism,
  aggregation soundness, report rendering, filter builder).
- Observer parity: 3 seeds × (outcome, rounds, per-unit HP/MP/statuses,
  `rollCursor`) identical with/without recorder; per-unit HP reconciliation
  exact.
- Live filter check: `crystal_tests.exe "[#test_danger]"
  --filenames-as-tags` ran exactly the danger file's 6 cases, all green.
- Editor smoke: launched with the new screens, alive after 6 s, clean kill.
- **Final program verification** (M59+M60 together): **552/552 Debug** and
  **548/548 Release** tests green (the 4-case gap is the debug-only
  god-mode battery; CrystalForge compiled in both configs); `--capture`
  **75/75** scenes clean — the game's rendering and behavior are untouched;
  zero project-code warnings on a forced recompile of the new TUs
  (EditorShell, Battle, SimLab, Process).

## G. Manual owner checklist

1. Build + run `.\build-msvc\CrystalForge.exe`; open **Sim Lab** from the
   sidebar.
2. Apply the *median gear* preset, level 20, opponent = custom team of 3
   town-1 enemies, 100 seeds → RUN. Expect ~100 % win rate and sane turns.
3. Switch opponent to **the King**, preset *best gear*, level 99, 100
   seeds → RUN. Expect losses (approved M54 balance: no relic counterplay
   in the sim) — the point is the telemetry: check the per-action and
   per-combatant tables read sensibly.
4. Change one number in content (e.g. a skill's power), save, re-run the
   same sweep, and confirm the delta column reflects it. Export the .md
   report and open it; confirm it landed in `reports/` and git ignores it.
5. Open **Test Runner**; run *Content validation* (fast) and watch it
   stream; then *Everything* if you have the minutes; `Del` mid-run stops.
6. Confirm the game itself is unchanged: play one ordinary battle and one
   boss battle; then `ctest --preset debug` if you want the full referee.
7. On failure: console output, the exported report, and what you clicked.

## H. Known limitations

- Sweeps block the UI for their duration (a 1000-seed King sweep is a few
  seconds); the progress callback exists but the shell does not repaint
  mid-sweep.
- Telemetry attribution leaves poison and retaliation damage deliberately
  unattributed (honest, but a "thorns" row is absent from per-action
  tables).
- The test runner shows raw Catch2 output (no per-case parsing beyond the
  final verdict); failures are read in the pane, fixed in the editor.
- The sim party AI is the simulator's (no items, no player creativity) —
  sweeps measure the curve, not the ceiling.

## I. Documentation updated

`docs/milestones.md` (M60 status + evidence), `docs/editor_guide.md`
(Sim Lab + Test Runner sections), `docs/technical_design.md` §17 (observer /
sim lab / process runner), this note. `README.md` checked — its
"Development tools" section already points at the guide.

## J. Final status

`implemented, awaiting manual approval` — with M59, this completes the
authorized M59–M60 program. M23 → M24 are next, after their re-audit against
this checkout.
