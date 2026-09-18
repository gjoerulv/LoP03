# M117 — Owner fix batch

**Status:** complete (approved 2026-09-18)
**Program:** M117–M119 (owner-authorized 2026-09-14 via the approved plan;
branch `oyb11`, baseline `72f4c3e`).
**No version motion:** settings v1 (default values only), rules 19,
generation 24, save v1, content v1, manifest v2.

## Scope (plan section M117, slices 1–4; the owner's items 1–4)

Four owner-directed corrections from the post-M116 manual pass, plus the
special-patrol audit the Jester defect prompted:

1. CRT defaults: Strength 0 → 2, Curvature 3 → 0.
2. The Controls page (Main Menu → Controls) must not advertise F1 = debug
   overlay in a Release build.
3. Blackjack gains 250 / 500 / 1000 gold stakes.
4. Special patrol fights broke when a Jester acted on the chests or the
   answers.

## What was built

### Slice 1 — CRT defaults

- `settings::Settings::crtIntensity` 0.0 → **0.2** (2/10) and
  `crtCurvature` 0.3 → **0.0** (flat glass). The loader's absent-field paths
  follow the struct default (comments rewritten); the legacy `crtEffect`
  migration (`true → 0.3` strength) is untouched.
- Files written by earlier builds keep their explicit values — the writer
  always serializes both keys — so only a fresh file or **Settings → Reset
  settings and bindings** shows the new defaults (routine assumption from
  the plan; no migration by design; the owner must reset or delete the file
  to see the change).
- Capture is unaffected by construction (`exportImage` reads the pre-shader
  target).

### Slice 2 — The Controls page

- `helpShownActions()` (pure, `states/HelpState.hpp`) replaces the
  file-local array; `InputAction::ToggleDebug` is listed only under
  `CRYSTAL_DEBUG_OVERLAY` — the PUBLIC `crystal_core` definition the exe and
  the tests share per preset — and the frame height follows the list, so a
  Release page shows eight rows with no empty slot. Dev builds keep the row
  (the key works there); the `02_help` capture (Debug) is unchanged.
- `tools/package.ps1` was NOT extended with a marker: the "Toggle debug
  overlay" display name is compiled into `actionDisplayName` regardless.

### Slice 3 — Blackjack stakes

- `gamble::kBlackjackBets` = `{10, 25, 50, 100, 250, 500, 1000}`. Every
  consumer is table-driven (the affordability filter and rows, the
  table-minimum message, the footer prompt), so no state changed. All seven
  rows fit the `EventChoiceState` window (nine rows before scrolling; box
  height 181 px). A won top stake pays 2000 back through the unchanged
  ledger route.
- Capture `149_blackjack_stakes`: the seven rows at maximal content.

### Slice 4 — The Jester in decision mode + the special-patrol audit

- **Root cause (confirmed):** `BattleState::executeUncontrolled` carried no
  decision-mode check — only `executePending` handed a hostile action to
  `resolveDecision`. The Jester's hashed pick (`battle::uncontrolledChoice`,
  aimed at a living foe = a 1-HP placeholder) landed as a real attack or
  skill: nothing resolved, no reward paid, the Mimic never woke, the answer
  was never judged, and the M109 observer tallied the felled placeholder as
  an enemy KO.
- **Owner rule (2026-09-14):** a Jester **waits** (no turn) while the
  encounter stands and a controllable living member can decide; in an
  all-Jester party the Jesters act and the acting Jester's own pick **is**
  the decision under the controlled-pick rules — a swing or single-foe skill
  at placeholder N chooses N, an all-foes skill is the sweep, an ally-facing
  pick is cast normally and the encounter keeps waiting. In the shipped
  Jester kit only `mend` is ally-facing (`weaken` is a single-foe debuff,
  `radiance` the all-foes sweep).
- **Mechanism A (waiting):** `game::waitsForDecision(battle, unit)` (pure,
  `SpecialEncounter.hpp`) — a party-side uncontrolled unit while a living
  controllable party unit exists; `BattleState::pruneOrder` (the M112 inert
  filter, run at `beginTurns` and at every order rebuild) drops such units
  while `decisionPending()`. The first controllable member's turn always
  resolves the encounter (its menu offers only Attack / offensive Skill /
  Escape), a reward or mock ends the battle with the Jester having done
  nothing, and a Mimic reveal rebuilds the order through
  `orderAfterDecision`, which seats the Jester (the decision is resolved).
  Nothing is logged for the wait; the prompt strip explains the encounter.
- **Mechanism B (the all-Jester pick):** `game::uncontrolledDecisionFor(
  battle, actor, choice, placeholderFirst, skill)` → `{decides, ordinal,
  aoe}` (decides = the choice's target is enemy-side; ordinal/aoe mirror
  `resolveDecision`'s formula for headless tests); `executeUncontrolled`
  mirrors the `executePending` guard: with a decision pending and a deciding
  pick it sets `pendingKind_`/`pendingSkillId_` from the choice, calls
  `resolveDecision(choice.target)` and returns before the quip block (only
  `revealMimic` sets the telegraph line). A `mend` pick falls through and is
  cast normally. `buildBattle` re-derives `uncontrolled` for the Mimic
  battle; `actedOnce`/`ownTurnsTaken` are set before the branch exactly as
  for a controlled decider; the punishment `koUnit(actor)` hits the Jester's
  own unit. `battle::uncontrolledChoice` (shared with the Simulator) and
  every pure rule are untouched.
- **Audit verdicts (the other actor paths):** `executeConfused` —
  unreachable in decision mode (`Character` carries no statuses,
  `writeBackParty` writes only hp/mp, `buildBattle` gives party units empty
  statuses, so `forcedActionFor` is `None` before the decision resolves);
  enemy turns — placeholders are pruned at `beginTurns`, `advanceTurn` and
  `captureEnterTargeting`; summons and class sweeps — `hostileTargetCount`
  is target-agnostic and pinned across every shipped skill; the sparring
  mirror and the Simulator — neither ever receives a `SpecialEncounter*`;
  the Golden Goose against a Jester party — a normal battle through
  `startBattle`, the guard is skipped; the M109 telemetry observer —
  exposed before the fix (a felled placeholder counted as an enemy KO; a
  Lore-Jester placeholder could reach the defeat ledger), closed by the fix
  because no placeholder is ever struck (display-only counters; existing
  saves are not repaired); `maybeApplySpoils` — `startSpecialBattle` passes
  no spoils, so even a stray sweep that wiped the placeholders paid
  nothing. Nothing else found.
- **Reproduction (dev build):** Debug menu → `Next patrol: Chests` (or
  `Lore`) → `Trigger patrol now` with a Jester in the party.

## Deviations from the plan

None. (The plan's Jester rule, the mechanisms and the audit list were
implemented as approved; `weaken` was already recorded as a hostile pick.)

## Tests

- `[settings]`: the absent / malformed / legacy / reset pins moved to the
  new defaults; new case "M117 owner defaults are strength 2 and curvature
  0" (fresh values, the steps, both keys serialized, an earlier build's
  explicit 0.0 / 0.3 surviving a load).
- `[gamble][m117]`: the table is strictly ascending from the 10 g minimum,
  carries 250 / 500 / 1000, and the doubled top stake is 2000.
- `[help][m117]` (`tests/test_help_page.cpp`, new): ToggleDebug listed only
  with the overlay — nine rows in Debug, eight in Release; the everyday rows
  in the page's order.
- `[lore][chest][m117]`: `waitsForDecision` (a Jester beside a living
  ranger waits; the ranger, a placeholder, an out-of-range index and an
  all-Jester party never do; every controllable member down → the Jester
  acts); `uncontrolledDecisionFor` (a swing or single-foe skill at N →
  ordinal N without a sweep, cross-checked through `resolveSpecial`; a
  `radiance` pick → the sweep, `AoePunished` / `MimicRevealed`; a `mend`
  pick at an ally, a targetless pick and a missing placeholder base → no
  decision; the field untouched, no roll consumed); the production
  `uncontrolledChoice` classified consistently over 64 rounds (every
  enemy-side pick decides, every party-side pick is `mend`); the Mimic morph
  seats the mixed party's Jester.

## Compatibility

- **Save files:** no impact (v1).
- **Settings files:** v1; two default values changed; old files keep their
  explicit values; malformed handling unchanged.
- **Content schemas:** no impact.
- **Deterministic seeds:** no impact (generation 24; the stakes are a
  runtime menu; the Jester rule is screen-side bookkeeping).
- **Score records:** no impact.
- **Packaged assets:** no new files; one new capture scene (dev builds).

## Automated validation

See the completion report below.

## Manual owner checklist

Matrix rows **232–236**: the new CRT defaults after a reset or on a fresh
file; the Release Controls page; the seven stakes; the Jester waiting in a
mixed party; the all-Jester whim.

## Known limitations

- A save that met the defect before this fix keeps whatever placeholder KOs
  its lifetime ledger tallied (display-only; not repaired).
- A lone-Jester party's `mend` turns can delay the decision a round or more
  (the pick varies per round; no cap — the same probabilistic behaviour the
  M45 Jester already has everywhere).

## Documentation updated

`docs/milestones.md` (rows 117–119, the program paragraph and section, row
112's post-approval parenthetical), `docs/game_design.md` (the Lore passage,
the Jester class passage, the blackjack stakes), `docs/technical_design.md`
(the CRT default sentences, §54's M117 bullet, §58), `README.md` (the status
line, the control table row and note), `docs/control_standard.md`,
`docs/release_hardening_manual_checklist.md`, `docs/manual_test_matrix.md`
(row 130 amended; rows 232–236), `docs/milestone_notes/M112_lore_and_chests.md`
(the post-approval section), `M45_kings_classes.md`, `M57_crt_strength.md`,
`M70_crt_curvature.md`, `M104_gambling_den.md` (dated pointers / sections),
this note.

## Completion report

### 1. Implementation summary

- **Milestone:** M117 — Owner fix batch.
- **Slices completed:** slice 1 CRT defaults; slice 2 the Controls page;
  slice 3 blackjack stakes; slice 4 the Jester's decision rule + the
  special-patrol audit. All completed.
- **Player-facing changes:** a fresh install shows CRT Strength 2 over flat
  glass; a Release build's Controls page lists no F1 row; the card table
  offers 250 / 500 / 1000 gold; a Jester waits out a Lore or Chest patrol
  unless the party is all Jesters, whose whim then decides.
- **Engineering changes:** `helpShownActions()`; `kBlackjackBets` (7);
  `game::waitsForDecision`, `game::uncontrolledDecisionFor`;
  `BattleState::pruneOrder` / `executeUncontrolled`; capture scene 149.

### 2. Files changed

- **Source:** `src/settings/Settings.hpp`, `src/settings/Settings.cpp`,
  `src/states/HelpState.hpp`, `src/states/HelpState.cpp`,
  `src/game/Gamble.hpp`, `src/game/SpecialEncounter.hpp`,
  `src/states/BattleState.cpp`, `src/capture/CaptureRunner.cpp`.
- **Tests:** `tests/test_settings.cpp`, `tests/test_gamble.cpp`,
  `tests/test_help_page.cpp` (new), `tests/test_lore_encounter.cpp`,
  `tests/test_chest_encounter.cpp`, `tests/CMakeLists.txt`.
- **Content/data:** none.
- **Documentation:** as listed under "Documentation updated".
- **Build/release configuration:** `tests/CMakeLists.txt` (one test file).

### 3. Plan deviations

None.

### 4. Compatibility

As under "Compatibility" above: settings defaults only; no save, content,
seed, score or packaging impact.

### 5. Automated validation

- **Configure command:** the existing `build-msvc` Debug configuration
  (re-run automatically by the build after the test-list edit).
- **Build command:** `cmake --build --preset debug` (VS 2022 amd64 dev shell).
- **Test command:** `.\build-msvc\crystal_tests.exe
  "[settings],[gamble],[help],[lore],[chest]"`, then the program's closing
  battery (`ctest --preset debug`, `--capture`, Release build + `ctest
  --preset release`).
- **Results:** Debug configure + build clean (VS 2022 `cl` 19.44 amd64; the
  test-list edit re-ran the configure); the targeted tags
  `[settings],[gamble],[help],[lore],[chest]` **28/28 cases, 11,075
  assertions**; `ctest --preset debug` **894/894 passed** (445.6 s);
  `ArePGeese.exe --capture docs\screenshots\m117_captures` **149/149 scenes
  clean**, zero overflow events (new: `149_blackjack_stakes` — all seven
  rows in one window; `60_settings_display` reads Strength 2 / Curvature 0;
  `02_help` keeps its nine rows in Debug); Release build clean +
  `ctest --preset release` **890/890 passed** (191.2 s; the Debug-only cases
  are compiled out there — the Controls-page pin ran in both presets and
  asserted eight rows in Release).
- **Warnings:** none in project code (both presets).
- **Skipped validation and reason:** none. `CrystalForge --canonicalize`
  was not run because no `data/*.json` file changed in this milestone.

### 6. Manual owner validation

Matrix rows 232–236 (above).

### 7. Known limitations

As under "Known limitations" above.

### 8. Documentation updated

As under "Documentation updated" above.

### 9. Final status

`complete (approved 2026-09-18)`
