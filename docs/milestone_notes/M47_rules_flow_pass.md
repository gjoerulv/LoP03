# M47 — Rules & flow pass

## A. Status and authority

- **Status:** ◑ implemented, awaiting manual approval (2026-07-23) — authored
  just-in-time on 2026-07-23 from the
  owner-approved M47–M51 "Court & Comfort" plan (authorized 2026-07-23). The
  first milestone of that program; M48 → M49 → M50 → M51 follow, **then**
  M23 → M24. See §J for the as-implemented record and deviations.
- **Authority:** `CLAUDE.md` → this note → `docs/milestones.md` →
  `docs/game_design.md` → `docs/technical_design.md`.
- **Base checkout:** `8dfbc25` ("Doc update before plam implementation"), which
  contains the M43–M46 merge `e9f7a37`. Working tree clean at authorization.
- **Baseline:** 431/431 tests, `--capture` 51/51 scenes overflow-clean.
- **Versions at baseline:** `battle::kBattleRulesVersion` **6**,
  `dungeon::kGenerationVersion` **10**, `kSaveVersion` **1**,
  `kSettingsVersion` **1**.
- **Terminal state for this session:** `implemented, awaiting manual approval`.

## B. Re-audit against `8dfbc25`

| Plan claim | Verified? |
|---|---|
| `battle::kBattleRulesVersion = 6` | ✅ [Battle.hpp:39](../../src/battle/Battle.hpp:39) |
| Purify is power **0** already — it heals nothing | ✅ [skills.json:59](../../data/skills.json:59): `"power": 0`, `"control": "cleanse"`. **No heal change is needed; this milestone only narrows the cleanse.** |
| `clearNegativeStatuses` strips Poison/Confusion/Silence/Blind **and** ATK-/DEF- | ✅ [Battle.cpp:93–103](../../src/battle/Battle.cpp:93) — it keeps only `AttackUp`/`DefenseUp`. Terrified/Stunned are also stripped by it (the plan said "never cleansed" — they are, because the keep-list is a whitelist). |
| **"Only Purify uses Cleanse"** | ❌ **False.** `generous_mending` (the Goose's M45 heal, [skills.json:68](../../data/skills.json:68)) also carries `"control": "cleanse"`. See §E1 for the decision this forces. |
| Recommended shape: *rename* `clearNegativeStatuses` → `clearAfflictions`, dropping the two debuff cases | ❌ **Not possible as written.** The same function is the body of `ConsumableEffect::Cure` ([Battle.cpp:936](../../src/battle/Battle.cpp:936)) — Remedy/Antidote. Narrowing it in place would silently change cure items, which is out of scope. A **new** function is added instead and `clearNegativeStatuses` keeps its meaning for items. |
| Royal Snacks' `clearStatDebuffs` is separate and untouched | ✅ [Battle.cpp:108–117](../../src/battle/Battle.cpp:108) |
| The Simulator drives the same rule code | ✅ `Simulator.cpp:106` calls `b.useSkill(...)`; the cleanse lives inside `Battle::useSkill`, so sim == live by construction |
| Castle defeat calls `healFull(party)`; escape and defeat both run `finish(false)` | ✅ [CastleChallengeState.cpp:104–106](../../src/states/CastleChallengeState.cpp:104) and `onResume` line 92 (any non-Victory outcome → `finish(false)`) |
| Party HP/MP already persist between castle fights | ✅ `BattleState::finish` writes `hp`/`mp` back for every outcome ([BattleState.cpp:766–773](../../src/states/BattleState.cpp:766)) — so "escape keeps battle-end HP/MP" is achieved by *not healing* |
| Neither `TownMenuState` nor `DungeonMenuState` handles `InputAction::Menu` | ✅ both `handleInput` bodies handle only MoveUp/MoveDown/Cancel/Confirm |
| `ConfirmPromptState` is hard-coded to 2 answers | ✅ `kConfirm`/`kCancel` constants, a 2-item `setItems`, and a panel height using `2 * kRowH` |
| The app exits when the state stack empties | ✅ [Application.cpp:175](../../src/core/Application.cpp:175) (`!stack_.empty()` loop guard) with an early return at line 208 for the frame that emptied it |
| Capture scene `44_quit_confirm` passes the same strings as `TownMenuState` | ✅ [CaptureRunner.cpp:768](../../src/capture/CaptureRunner.cpp:768) — it must be updated in lockstep |
| `clearStates()` is available and already used for Quit to Title | ✅ `StateStack::clearStates` queues a `Clear` change; `TownMenuState` already pairs it with a `MainMenuState` push |
| Settings save on change (nothing to flush at Quit Game) | ✅ `SettingsState` writes on every change; no other subsystem buffers state that quitting would lose beyond the run itself (the same run Quit to Title already discards) |

Facts the plan did not state, found during the re-audit:

1. **Tests cannot construct raylib-backed states.** The whole suite is headless;
   the only `states/` headers any test includes are the *pure* ones
   (`EquipShopFilter.hpp`, `ItemShopFilter.hpp`, `GameState.hpp`,
   `StateStack.hpp`). The quit-answer model must therefore live in a pure
   header to be testable — see §E4.
2. **Two texts outside `CastleChallengeState` promise the free heal** and become
   dishonest with the new rule: the battle defeat line for castle fights
   ([BattleState.cpp:790–792](../../src/states/BattleState.cpp:790), "nothing is
   lost") and the `first_challenge` tutorial beat
   ([Tutorial.hpp:107–111](../../src/tutorial/Tutorial.hpp:107), "Fall, and you
   simply return to the castle to try again"). Both are in scope for this
   milestone: the rule they describe is the rule being changed.
3. **`docs/game_design.md:353–355`** states the full-heal rule as an M43 bullet.
   It must change with this milestone (the owner called this out explicitly).
4. **`docs/manual_test_matrix.md` row 96** asserts "the party is at full HP/MP
   back at the castle" — the same rule, restated in the validation contract.

## C. Goals

Four quick wins at one battle-rules bump (**6 → 7**), no new content, no
generation/save/settings bump:

1. **Purify cleanses afflictions only** — Poison, Blind, Silence, Confusion;
   ATK-/DEF- survive. (It already heals nothing.)
2. **Castle defeat has stakes** — survivors at 1 HP, KO'd members stay KO'd, MP
   untouched, no gold penalty; a full wipe leaves exactly one member standing at
   1 HP. Escape keeps battle-end HP/MP.
3. **The Menu action closes the pause menus** (town and dungeon).
4. **A Quit flow** from both pause menus: Quit to Title / Quit Game / Keep
   Playing, on a `ConfirmPromptState` generalized to N answers.

Out of scope: any content tagging or element work (M48), guard/Boss-Rush changes
(M49), town layout (M50), settings/presentation (M51).

## D. Constraints

- Every battle rule lives in shared `battle::` code called by **both**
  `BattleState` and the `Simulator`. The cleanse change is inside
  `Battle::useSkill`, which is the single path both drivers take — sim == live by
  construction, no duplicated rule.
- No new rolls: nothing here touches `rollCursor` or any seeded stream, so a seed
  reproduces the same dungeon and the same battle inputs. Only the *resolution*
  of a cleanse changes, which is exactly what the rules bump records.
- `kGenerationVersion`, `kSaveVersion`, `kSettingsVersion` unchanged.
- All new/changed UI uses the M46 kit (`src/ui/UiDraw.hpp`, `palette()` roles).

## E. Slices

### E1 — Purify = afflictions only (rules 6 → 7)

New pure helper beside its siblings in `Battle.cpp`'s anonymous namespace:

```
clearAfflictions(Combatant&)   // removes Poison, Blind, Silence, Confusion only
```

`SkillEffect::Cleanse` calls it instead of `clearNegativeStatuses`.
`clearNegativeStatuses` is **kept unchanged** as the body of
`ConsumableEffect::Cure` (Remedy/Antidote still lift everything), and
`clearStatDebuffs` (Royal Snacks) is untouched. `kBattleRulesVersion` **6 → 7**.

**Decision forced by the re-audit (§B):** `generous_mending` also carries
`control: "cleanse"`, so narrowing the control narrows that skill too. Two
readings were available:

- **(a) chosen — narrow the `cleanse` control itself.** One rule, one code path,
  no schema change, and the word "cleanse" means one thing everywhere. The
  Goose's heal no longer lifts ATK-/DEF-; it still mends, still buffs, still
  lifts the four afflictions.
- (b) rejected — give Purify a *new* control value so Generous Mending keeps the
  wide cleanse. That adds an enum value and a second cleanse rule for one skill,
  for no player-legible reason.

This is reported to the owner as a decision, not buried: it is a small,
reversible balance change to one M45 skill. Reversing it means slice (b).

Purify's description is rewritten to state the new scope exactly; Generous
Mending's ("Mends and cleanses the party. Braces the enemies too, alas.") is
still true and is left alone.

Terrified/Stunned: previously `clearNegativeStatuses` *did* strip them (whitelist
keep-list). Under `clearAfflictions` they survive a Purify — consistent with the
owner's rule ("afflictions only") and with M44's intent that a relic's forced
turn is not undone by a cure. Cure items still lift them, unchanged.

### E2 — Castle defeat / escape

New pure function next to `healFull`:

```
void clampCastleDefeat(Party&);   // game/Party.hpp, implemented in Party.cpp
```

Rules, exactly:

- every **living** member drops to `hp = 1` (`hp = min(hp, 1)`); MP untouched;
- every **KO'd** member stays at `hp = 0`;
- if **nobody** survived, member **0** (first in party order) is raised to
  `hp = 1` — the party can always limp to an Inn;
- an empty party is a no-op;
- no gold penalty (unchanged), no forfeited run (there is none), records and
  partial endless rewards unchanged.

`CastleChallengeState::finish(false)` calls it instead of `healFull`. **Escape**
takes the same `finish(false)` path, so the clamp applies to a fled challenge
too — a survivor who ran at 4 HP still ends at 1 HP. That is the honest reading
of the owner's rule ("escape keeps battle-end HP/MP" describes *not being
healed*; the run still ended in the castle). ⚠️ Called out for owner review in
§J because it is the one place the wording admits two readings; the alternative
(clamp on defeat only, leave an escape untouched) is a one-line change.

Dungeon defeat (`DungeonState.cpp:518`, `healFull` + half gold) is **untouched**.

Texts made honest in the same slice:

- `CastleChallengeState::finish` result line;
- `BattleState::outcomeMessage` castle branch;
- the `first_challenge` tutorial beat.

### E3 — Menu closes the pause menus

`TownMenuState::handleInput` and `DungeonMenuState::handleInput` gain
`if (input.pressed(InputAction::Menu)) { popState(); return; }` — the same key
that opened the menu closes it, on both devices (Tab / Start). No new hint text:
the footer already reads Cancel, and this is a discoverable extra, not a
replacement.

### E4 — Quit flow (town + dungeon)

**`ConfirmPromptState` generalized to N answers.** New public
`ConfirmPromptState::Answer { std::string label; std::function<void()> onChoose; }`
and a constructor taking `std::vector<Answer>` plus a `safeIndex` (the row the
cursor starts on and the row Cancel resolves to). The existing 2-answer
constructor delegates to it, so **no caller churn**. `render` measures
`answers.size() * kRowH` instead of `2 * kRowH`. Cancel and the safe row both
just pop; any other row pops and then runs its callback (the pop is queued, so
the callback decides what follows — the existing contract).

**Pure, testable model** — `src/states/QuitPrompt.hpp` (headless, no raylib),
mirroring the `EquipShopFilter.hpp` precedent: the three answer labels in order,
the `kQuitToTitle` / `kQuitGame` / `kKeepPlaying` indices, `kSafeAnswer`, and the
two body strings (town / dungeon). Both menus and the capture scenes read it, so
the strings exist once.

- **Town pause:** the "Quit to Title" row becomes **"Quit"** → prompt titled
  "Quit", same warning body ("Progress since your last save will be lost."),
  answers Quit to Title / Quit Game / Keep Playing, cursor on Keep Playing.
- **Dungeon pause:** gains a **"Quit"** row after "Retreat to Town" → the same
  three answers with a dungeon-honest body: "This run will be lost. Progress
  since the autosave at dungeon entry is kept."
- **Quit to Title** = `clearStates()` + push `MainMenuState` (today's behavior).
- **Quit Game** = `clearStates()` and push nothing → `Application::run`'s
  `!stack_.empty()` guard ends the loop cleanly. Settings already persist on
  change; nothing else needs flushing.

Panel heights: the town pause box grows for no new row (Quit replaces Quit to
Title); the dungeon pause box grows by one row (112 → 130) and its retreat note
moves with it.

## F. Tests (added)

New `tests/test_rules_v7_flow.cpp`:

- **Cleanse scope:** a real `Battle` where a party member carries Poison, Blind,
  Silence, Confusion, ATK- and DEF- plus a buff; Purify (through
  `Battle::useSkill`, the shared path) leaves ATK-/DEF- and the buff and removes
  the four afflictions.
- **Sim == live:** the same battle resolved by the Simulator and by direct
  `useSkill` calls agrees on the resulting status list.
- **Cure items unchanged:** a Remedy still lifts ATK-/DEF- (the regression this
  milestone is most likely to cause).
- **Rules version is 7.**
- **`clampCastleDefeat`:** partial survivors → 1 HP each; KO'd stay 0; MP
  untouched; full wipe → member 0 at 1 HP and the rest at 0; already-1-HP
  members unchanged; empty party no-op; a single-member party.
- **Quit model:** three labels in order, safe index is Keep Playing, and a
  `ui::Menu` built from them navigates and clamps.

Existing tests that must keep passing unchanged: `test_battle_rules_v4.cpp`'s
Purify assertions (power 0, control Cleanse) and every status test.

## G. Capture

- `44_quit_confirm` — rebuilt on the 3-answer prompt (same town pause backdrop).
- **new** `52_dungeon_quit` — the dungeon pause menu with the 3-answer quit
  prompt over it (the new row and the dungeon body string).

Suite goes 51 → **52 scenes**, all overflow-clean.

## H. Documents to update

- `docs/game_design.md` — the M43 castle-defeat bullet (full heal → the new
  stakes) and the Purify bullet (afflictions only); an M47 paragraph.
- `docs/technical_design.md` — a rules-v7 section (cleanse split, the castle
  clamp, the N-answer prompt, the quit-game exit path).
- `docs/manual_test_matrix.md` — row 96 corrected; new rows for the castle
  stakes, the Menu-close, and the two quit flows.
- `docs/milestones.md` — M47 status.
- `README.md` — the castle bullet mentions no free healing on defeat.
- This note's §J.

## I. Acceptance criteria

1. Purify removes exactly Poison/Blind/Silence/Confusion from the party; ATK-,
   DEF- and buffs survive; Cure items are unchanged; proven through the shared
   `battle::` path so sim == live.
2. `kBattleRulesVersion` is 7 and the scoreboard's comparability display flags
   older entries (existing M19 mechanism, no new work).
3. Losing (or fleeing) a castle challenge leaves survivors at 1 HP, KO'd members
   KO'd, MP untouched, no gold taken; a wipe leaves exactly one member at 1 HP;
   every text that describes the outcome says so.
4. Pressing Menu closes the town and dungeon pause menus.
5. Both pause menus offer Quit → three answers; Quit to Title returns to the
   title; Quit Game exits the process cleanly; Keep Playing and Cancel return.
6. 431 + new tests green in **Debug and Release**; `--capture` 52/52 clean.
7. No generation/save/settings version change; old saves and settings load.

## J. As-implemented record

All four slices landed as planned in §E. Verified 2026-07-23 on `8dfbc25`:
**442/442 tests** (431 baseline + 11 new) green in **Debug and Release**, and
`--capture` **52/52** scenes overflow-clean. No warnings introduced (the changed
translation units were force-rebuilt to confirm).

### Deviations from the approved plan

1. **`clearNegativeStatuses` was not renamed/narrowed — a new
   `clearAfflictions` was added instead.** The plan's recommended shape would
   have silently changed `ConsumableEffect::Cure` (Remedy), which shares that
   function and is not in scope. The narrowing is applied at the
   `SkillEffect::Cleanse` call site only. Routine local decision; it delivers the
   approved rule exactly and protects an out-of-scope one. A test now pins the
   item behavior so the two can never be merged back by accident.
2. **The plan's claim "only Purify uses Cleanse" was false.** `generous_mending`
   (the Goose's M45 heal) also carries `"control": "cleanse"`, so narrowing the
   control narrowed that skill too: it no longer lifts ATK-/DEF-. **Owner
   decision point** — the alternative is a second control value so Generous
   Mending keeps the wide cleanse (§E1(b)). Chosen because one word should mean
   one rule and it needs no schema change; it is a small, reversible balance
   change to one skill.
3. **A fled castle challenge takes the same clamp as a lost one.** Escape and
   defeat share `finish(false)`, and the owner's rule ("escape keeps battle-end
   HP/MP") describes the *absence of a heal*. Implemented as: no heal, and the
   1-HP clamp applies. **Owner decision point** — clamping only on defeat is a
   one-line change if the intent was that fleeing preserves exact HP.
4. **Terrified/Stunned now survive a Purify.** The old cleanse was a whitelist
   (keep the two buffs, drop everything else), so it silently undid the Royal
   Relics' turn-control statuses. `clearAfflictions` is an explicit blacklist of
   the four afflictions, so it no longer does. Consistent with the owner's rule
   and with M44's intent; cure items still lift them.
5. **Two texts outside the planned set were corrected** (§B note 2): the castle
   branch of `BattleState::outcomeMessage` and the `first_challenge` tutorial
   beat both promised the free heal. In scope — they describe the rule being
   changed.
6. **`states/QuitFlow.{hpp,cpp}` was added** beyond the planned pure
   `QuitPrompt.hpp`. The pure header cannot bind actions (they need the state
   stack) and the headless tests cannot see a raylib state, so the strings stay
   pure and testable while one small `pushQuitPrompt` builder serves both menus
   and both capture scenes.
7. **The 2-answer `ConfirmPromptState` constructor is now unused.** It was kept,
   per the plan ("no caller churn"), as the documented one-line form for future
   Yes/No prompts — but the town menu, its only caller, moved to three answers.
   Noted so it is a decision, not drift.

### Files changed

- **Source:** `src/battle/Battle.hpp` (rules 7), `src/battle/Battle.cpp`
  (`clearAfflictions` + the Cleanse call site), `src/game/Party.hpp/.cpp`
  (`clampCastleDefeat`), `src/states/CastleChallengeState.cpp`,
  `src/states/BattleState.cpp` (defeat text), `src/tutorial/Tutorial.hpp`
  (`first_challenge` beat), `src/states/ConfirmPromptState.hpp/.cpp` (N answers),
  **new** `src/states/QuitPrompt.hpp`, **new** `src/states/QuitFlow.hpp/.cpp`,
  `src/states/TownMenuState.cpp`, `src/states/DungeonMenuState.cpp`,
  `src/capture/CaptureRunner.cpp`.
- **Tests:** **new** `tests/test_rules_v7_flow.cpp` (11 cases),
  `tests/CMakeLists.txt`.
- **Content:** `data/skills.json` (Purify's description only).
- **Build:** `CMakeLists.txt` (`src/states/QuitFlow.cpp`).
- **Docs:** this note, `docs/milestones.md`, `docs/game_design.md`,
  `docs/technical_design.md`, `docs/manual_test_matrix.md` (row 96 corrected,
  rows 111–117 added), `README.md`.

### Compatibility

Saves, settings, content schemas, and generated dungeons are all untouched
(`kSaveVersion` 1, `kSettingsVersion` 1, `kGenerationVersion` 10, no schema
fields added). Existing scoreboard entries keep their recorded
`battleRulesVersion`; the M19 display flags them as played under older rules and
never renormalizes them — the mechanism already existed, so the bump needed no
new work.

### Known limitations

- Balance is unmeasured by simulation: the narrowed Purify and the harsh castle
  defeat both make the endgame *harder*, and no new balance battery was run for
  them (M49 re-proves Boss Rush and King clearability under exactly these stakes,
  which is where that evidence belongs). Owner feel-testing is the check here.
- Quit Game was verified by construction and by reading the loop guard, not by
  driving the window — the capture harness renders states, it does not press
  keys. Manual check 115 covers it.
