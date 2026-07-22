# M42 — Enrichment: Bestiary, Victory Stats, Achievements

## A. Status and authority

- **Status:** implemented, awaiting manual approval — authored / re-audited and
  implemented 2026-07-21 against the post-M41 checkout. Owner authorized beginning
  M42 after approving M41. As-implemented record in §K. Only the owner sets
  `complete (approved)`. **Final milestone of the M35–M42 endgame program** (M23 →
  M24 run after, re-audited post-M42).
- **Authority:** `CLAUDE.md` > this note > `docs/milestones.md` >
  `docs/game_design.md` > `docs/technical_design.md`. **Presentation + persistence
  only** — no battle, generation, or scoring change; `kBattleRulesVersion` (3),
  `kGenerationVersion` (8), and `kSaveVersion` all unchanged (additive optional save
  fields + one new global file, tutorial.json-style).

## B. Problem statement (verified at re-audit)

- **No codex, run summary, or meta-goals.** The game has depth (ladder, statuses,
  passives, castle, story) but nothing that lets a player review the foes they've
  met, see how a run went beyond the score, or chase cross-run goals.
- **Patterns to reuse:**
  - **Global tutorial.json-style store** — `tutorial::TutorialStore` (a versioned
    JSON of a `std::set<std::string>`, defensive load, headless parse/serialize)
    ([Tutorial.hpp:127](src/tutorial/Tutorial.hpp:127)); the achievements store is a
    near-clone, added to `AppContext` and created in `Application` beside `tutorial_`.
  - **Scrolling list screen** — `ScoreboardState` uses a `ui::ScrollWindow`
    (`scrollBy`/`top`/`visibleCount`/`moreAbove`) ([ScoreboardState.cpp](src/states/ScoreboardState.cpp));
    the bestiary and achievements lists reuse it.
  - **Per-party save fields** — the optional-field/defensive-load pattern
    (`SaveSystem`, `optIntMin`/`optString`/`optStringArray`) carries the bestiary set
    and the personal records.
  - **Battle sprites + profile data** — `enemy.<id>.battle` / `boss.<id>.battle`
    resolve a foe's sprite; `EnemyDef` has `role`/`tier`/`tags`/`passives`, `BossDef`
    has `archetype`/`passives`/`description`. **Note: `EnemyDef` has no description**
    (only `BossDef` does) — see §D flavor choice.
  - **Result flow** — `DungeonResultState` (M39 tightened its panel) + its Details
    overlay; `DungeonState` accumulates a run and pushes the result.
- **Baseline:** 364/364 tests, 37/37 capture, no version changes since M41.

## C. Goals (the owner's three picks; daily-seed was rejected)

- **Bestiary** — a list of every enemy/boss the party has fought (a persisted
  encountered-set), each with sprite, stats, behaviour profile (role/tier or
  archetype), tags, revealed passive, and flavor where available.
- **Victory stats + records** — the result flow gains this-run stats (total damage,
  biggest hit, statuses inflicted, MVP) accumulated in a `RunStats`, plus per-party
  personal records (biggest hit ever, most run damage), display-only (never ranked).
- **Achievements** — a local screen of ~15–20 original achievements spanning the
  whole game, persisted tutorial.json-style (global), with one toast on unlock.
- All three are presentation/persistence only: no battle, generation, or scoring
  change.

## D. Owner decisions & routine choices

Locked at program planning: the three picks above; achievements persist
tutorial.json-style; ~15–20 achievements; one toast on unlock; no network.

Routine choices taken here (owner validates at approval):

- **Access points.** The **Town pause menu** (`TownMenuState`) gains **Bestiary**
  and **Achievements** entries (no town-layout change; both need only the pause hub).
  Victory stats appear on the **result screen's Details overlay** (keeps the main
  result panel — already tightened in M39 — free of overflow).
- **Persistence.** Bestiary encountered-set = a **per-party save field**
  (`Party.encountered`, id set, round-trips). Personal records = **per-party save
  fields** (`Party.recordBiggestHit`, `Party.recordRunDamage`) — round-trip through
  saves, display-only (the acceptance's "round-trip through saves"). Achievements =
  a **global `achievements.json`** (tutorial.json-style), as the owner specified.
- **Bestiary flavor.** Bosses show their existing `description` as the flavor line.
  **Enemies have no description field**, and writing 43+ new original one-liners is a
  disproportionate task; enemies instead show their **role + tier + tags** as the
  profile. Per-enemy M41-voice flavor is a possible polish follow-up (flagged). The
  substantive "behaviour profile" (role/archetype/tier/tags/passive) is shown for
  every entry.
- **Victory-stats tracking is LIVE-only and display-only.** `BattleState`
  accumulates a `RunStats` from HP deltas (enemy damage attributed to the acting
  party member) into an optional `RunStats*` slot; `DungeonState` merges each battle
  and updates the party records on a completed run. The pure **Battle model and the
  Simulator are untouched** — stats never feed a decision or RNG, so resolution and
  sim↔live agreement are byte-identical. Castle challenges pass no slot (their own
  records already cover them).
- **Achievements** (~16, original): a static table (`game/Achievements.hpp`) checked
  by a pure `checkAchievements(party, ctx)` against **party/records state** plus a
  small event context (a just-completed run's summary), called at dungeon
  completion, castle-challenge completion, and town travel. Newly-satisfied → the
  store records them and a toast shows. Ids are stable once shipped.
- **No version bumps.** All additive/optional; the achievements file is new and
  self-contained.

## E. Proposed design & slices

1. **Bestiary.** `Party.encountered` (id set, save round-trip); `BattleState` adds
   each enemy/boss `sourceId` on battle start; `BestiaryState` (scroll list: sprite
   + name + stats + role/tier|archetype + tags + passive + boss flavor);
   `TownMenuState` "Bestiary" entry.
2. **Victory stats + records.** `game/RunStats.hpp`; `BattleState` accumulates into
   an optional `RunStats*`; `DungeonState` merges + updates `Party.recordBiggestHit`
   / `recordRunDamage`; `DungeonResultState` shows this-run stats + records in its
   Details overlay.
3. **Achievements.** `game/Achievements.hpp` (~16 defs + `checkAchievements`);
   `AchievementStore` (global JSON, tutorial.json-style) in `AppContext` +
   `Application`; `AchievementsState` (scroll list, locked/unlocked); an
   `AchievementToastState` (or reused overlay) on unlock; hooks at dungeon/castle/
   travel; `TownMenuState` "Achievements" entry.
4. **Tests / capture / docs.** `[bestiary]`/`[achievement]`/`[runstats]` unit tests
   (encountered round-trip, achievement predicates + store round-trip, RunStats
   merge/MVP); capture scenes (bestiary, achievements, run-stats details); all docs.

## F. Determinism & save compatibility

- **No determinism surface.** Stats are derived display numbers computed live; the
  Battle model and Simulator are untouched; nothing seeds or resolves differently.
- **Save.** `Party.encountered` (string array), `recordBiggestHit`, `recordRunDamage`
  are additive optional fields (old saves → empty/0). Achievements live in a new
  global file (old installs → none unlocked). No `kSaveVersion` bump.

## G. Out of scope

Any new battle/generation/scoring behavior; the rejected daily-seed challenge;
per-enemy M41-voice flavor rewrites (bosses reuse `description`); achievement
popups beyond one toast; M23/M24 (they run after, re-audited).

## H. Balance / acceptance

The encountered-set / records / achievements round-trip through saves (and the
global achievements file); no ranking or determinism change (battle/sim untouched);
~15–20 original achievements; capture overflow-clean for the new screens; full suite
green from the 364 baseline; Debug + Release clean.

## I. Manual validation for the owner

Whether the bestiary reads well and is worth consulting; whether the run stats add
to the post-run payoff; whether the achievements feel rewarding and their toasts land
without nagging; the achievement writing (original, in the game's voice).

## J. Required documentation updates on completion

This note (§K); `docs/milestones.md` (status; the program closes → M23/M24 next);
`docs/game_design.md` (the three features); `docs/technical_design.md` (the systems);
README (features line); `docs/manual_test_matrix.md` rows; the M23/M24 notes'
re-audit pointer (extend to cover M35–M42); a manual checklist per
`docs/milestone_completion_template.md`.

## K. As-implemented record (2026-07-21)

- **Status:** implemented, awaiting manual approval. The §D routine choices were
  taken as written (pause-menu access; per-party bestiary/records + global
  achievements; boss `description` as flavor, no per-enemy flavor; live-only
  RunStats; 16 achievements; no new art).
- **Bestiary.** `Party.encountered` (save round-trip) appended in `BattleState`'s
  constructor per foe faced; `BestiaryState` (scroll list + detail panel: sprite,
  stats, `toString` profile + tags, revealed passive, boss flavor); `TownMenuState`
  "Bestiary" entry.
- **Victory stats + records.** `game/RunStats.hpp`; `BattleState` accumulates into an
  optional `RunStats*` from HP deltas (Battle model + Simulator untouched);
  `DungeonState` passes `&victoryStats_`, updates `Party.recordBiggestHit`/
  `recordRunDamage`, and hands the stats to `DungeonResultState`, whose Details key
  shows this-run stats + personal records.
- **Achievements.** `game/Achievements.hpp` (16 original goals + `achievementMet` +
  `AchievementStore`, a global tutorial.json-style `achievements.json` in `AppContext`/
  `Application`); `pushAchievementToasts` (via the story-dialog overlay) hooked at
  dungeon completion (run context), castle-challenge finish, and `TownState::onResume`;
  `AchievementsState` from `TownMenuState` (locked ones hidden as goals). The pause
  panel grew to fit the two new entries.
- **Validation (this session, VS 2022 / MSVC, `build-msvc`).** Debug **368/368**
  (+4: `[achievement]` roster/predicates/store round-trip, `[runstats]` MVP; +save
  round-trip for the encountered set + records). `--capture` **40/40** overflow-clean
  (new `38_bestiary`, `39_achievements`, `40_run_stats`). Release build + Release suite
  **368/368** clean. `kSaveVersion` / `kBattleRulesVersion` (3) / `kGenerationVersion`
  (8) all unchanged; no new art (the bestiary reuses battle sprites).

### Deviations from the plan / note

1. **Bestiary flavor for bosses only.** `EnemyDef` has no `description` field; rather
   than write 43+ new original one-liners, enemies show their role/tier/tags profile
   and bosses reuse their `description` as flavor. Per-enemy M41-voice flavor is a
   flagged follow-up. The behaviour profile is shown for every entry.
2. **Personal records are per-party save fields** (not a global file "alongside the
   scoreboard"): they round-trip through saves (the acceptance criterion) and are
   display-only. Achievements are global, as specified.

### Known items for owner validation

- Whether the bestiary is worth consulting; whether the run stats add to the post-run
  payoff; whether the achievements feel rewarding and the toasts land without nagging;
  the achievement writing (original, in the game's voice).

### Playtest fixes (2026-07-22, owner round 1)

Four presentation defects the owner found in the M42 build. All are draw-layer
only — no battle, generation, scoring, save, or content-rule change.

1. **Bestiary passives ran off the panel.** They were joined into one line, so a
   foe with three (The Hollow King) lost all but the first. Each passive now gets
   its own line under a `Passive`/`Passives` label, and the boss flavor block
   below picks the body font when it fits the remaining room and the caption font
   when it does not — the King's (longest) text now renders in full.
2. **The bestiary only listed foes already fought.** It now lists the **whole
   roster** (43 enemies then 13 bosses, each group alphabetical so a slot never
   moves as it is discovered); undiscovered entries read `? ? ?` with masked
   stats, the convention `AchievementsState` already used, and the header shows
   `known / total recorded`.
3. **Battle skill rows clipped the MP cost.** The cost was part of the label
   string, so a long skill name pushed it into the clip region. `MenuItem` gained
   an optional `suffix` that `drawMenuScrolled` reserves *first* and right-aligns
   at the row edge (the label is fitted to what is left), the battle panel's
   list/description split moved to fit the widest party skill name plus its cost
   column, and the two lists' hint lines were shortened to `Skill`/`Item` so the
   narrower description column still fits their prompts. A silence-blocked skill
   shows `SIL` in that column and the reason in the description column (it used
   to append `[Silenced]` to the label). Two skill descriptions (`fade`,
   `redirect`) were reworded slightly shorter to keep every party skill inside
   the two-line description region — `redirect` already overflowed it before this
   change.
4. **Party status tags overlapped neighbouring rows.** They were drawn below the
   sprite, where the next party row's sprite reaches. Both sides now read their
   tags in the open field right of the sprite (party rows under their HP/MP
   numerals), packed into at most two lines that fit the room with a trailing
   `+N` if a unit carries more; the party column moved 14px left to give that
   field room.

Validation: `--capture` **42/42** overflow-clean — two new scenes,
`41_bestiary_king` (longest boss name, three passives, longest flavor) and
`42_battle_skills` (the widest party skill names against the MP column), both of
which caught real clipping before this was declared done.

### Playtest fixes (2026-07-22, owner round 2)

Two more draw-layer defects, same constraints.

1. **The achievements description overlapped the Back hint.** Sixteen rows in one
   column left no room under the list, so the description block ran into the
   footer. The roster is now laid out in **two columns** of `(count + 1) / 2` rows
   (left/right jump a column; up/down still walk the whole list in reading order)
   and the description is drawn **centered** under it via a new
   `ui::drawTextWrappedCentered`, with clear air above the hint.
2. **The save/load rows ran off screen.** A party's King title was appended to the
   slot label, which already carried slot, level, party size, and gold — with a
   title the row measured ~630px against ~350px of room, and `drawMenu` neither
   clips nor reports. `SlotMenuState` now draws its own rows (keeping `ui::Menu`
   for cursor/enabled logic): the slot line is `drawTextFitted`, and the title sits
   **on its own indented line beneath it**. The rows moved slightly left/up and the
   status message down, so a four-slot Load list with a title on every row still
   clears it.

Validation: `--capture` **43/43** overflow-clean — `11_slot_menu_save` now writes
an occupied slot carrying the title and maximal gold, and the new
`43_slot_menu_load` covers the deepest list (four occupied rows, each with a
title).

### Playtest fixes (2026-07-22, owner round 3)

**Quit to Title asks a real question.** The M22 destructive-action pattern armed a
second Confirm on the same pause entry and explained itself with a line of text
below the menu, which reads as a dead key press. New reusable
`ConfirmPromptState` (`states/ConfirmPromptState.hpp`): a transparent modal that
dims and freezes the scene, states the consequence, and offers an explicit
choice — cursor starts on the safe answer, Cancel answers no, and a supplied
`std::function` runs on yes. `TownMenuState` drops `quitArmed_` and its warning
line (the pause panel is back to a fixed height) and pushes the prompt with
**Quit to Title / Keep Playing**; the callback clears the stack and pushes
`MainMenuState`, exactly as before. Behaviour is unchanged for every other pause
entry, and `SlotMenuState`'s overwrite confirmation was deliberately left alone —
its warning is a screen-level message, not an overlay.

Validation: Debug **368/368**, Release **368/368**, `--capture` **44/44**
overflow-clean — new `44_quit_confirm` (the prompt over the pause panel it opens
from) is the first capture coverage the pause menu has had. No version bumps, no
new art.

### Program close-out

M42 completes the **M35–M42 endgame program**. Next in the execution order are the
deferred **M23** (validation/playtesting/balance) and **M24** (release packaging),
whose notes already point at a post-expansion re-audit — that re-audit now needs to
cover the M35–M42 systems (statuses, passives, per-town content, drops, the castle &
King, story, and these enrichment features), their capture scenes (40 now), and the
balance batteries (`[sim-report]`, `[economy-report]`, `[castle-report]`).
