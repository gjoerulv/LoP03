# M45 — The King's classes: Dragon, Jester, Goose

## A. Status and authority

- **Status:** ◑ implemented, awaiting manual approval (2026-07-22) — the
  **final milestone of the King's Gambit program**; M23 → M24 run after it. See
  §J for the as-implemented record and deviations (one of them, the Simulator's
  round counter, fixes a pre-existing sim/live disagreement).
- **Authority:** `CLAUDE.md` → this note → `docs/milestones.md` →
  `docs/game_design.md` → `docs/technical_design.md`.
- **Base checkout:** `5608528` ("M44"), working tree clean at authorization.
- **Baseline:** 405/405 tests, `--capture` 46/46 scenes overflow-clean.
- **Terminal state for this session:** `implemented, awaiting manual approval`.

## B. Re-audit against `5608528`

| Plan claim | Verified |
|---|---|
| `AchievementStore` is the atomic user-data-file pattern to copy | [Achievements.hpp:68](../../src/game/Achievements.hpp:68) — versioned JSON, defensive load, atomic save, created in `Application`, held on `AppContext` |
| Per-slot `kingDefeated` cannot gate New Game | it lives on `Party.castleRecords` ([Party.hpp:44](../../src/game/Party.hpp:44)), which does not exist before a party does |
| Class selection is a cycling list | `PartyCreationState::onEnter` builds `classes_` in canonical order then appends extras; `cycleClass` wraps ([PartyCreationState.cpp:30](../../src/states/PartyCreationState.cpp:30)) |
| `ClassDef` is small and additive | id/name/role/baseStats/growth/startingSkills/learnset ([Definitions.hpp:42](../../src/content/Definitions.hpp:42)) |
| Scoring is a pure breakdown over `RunSummary` | `scoreBreakdown` applies the town bonus and stakes penalty as percentages of the subtotal ([Scoring.cpp:22](../../src/score/Scoring.cpp:22)); `ScoreEntry` carries M19 comparability tags ([ScoreEntry.hpp:9](../../src/score/ScoreEntry.hpp:9)) |
| Forced/automatic actions have one home | `ForcedAction` / `forcedActionFor` / `forcedChoice`, consulted by `BattleState`, the Simulator's `choosePartyAction`, and `chooseEnemyAction` (M43/M44) |
| Presentation-only rolls must not touch `rollCursor` | `targetJitter` is the pure-hash precedent ([Battle.cpp](../../src/battle/Battle.cpp)) |
| Every class needs battle art | the presentation lint requires `actor.<classId>.battle` for **every** class in the database ([test_presentation_lint.cpp:67](../../tests/test_presentation_lint.cpp:67)); sprites come from `New-Humanoid` + `Save-Actor` in the generator |
| Equipping is a slot assignment | `EquipShopState` writes `slotRef(c, slot)` then `refreshCharacter` ([EquipShopState.cpp:237](../../src/states/EquipShopState.cpp:237)) |
| Versions | `kBattleRulesVersion` **5**, `kGenerationVersion` **10**, `kSaveVersion` **1** |

One fact the plan did not state: **`PartyCreationState::begin()` resets the whole
party** (gold, town, stakes, tokens, market) — so the unlock must live outside
`Party`, exactly as the plan's profile store says.

## C. Goals

Three unlockable joke-but-real classes as the King's reward, **schema-driven**:
no class-specific behavior anywhere outside data plus generic flags. Rules
**5 → 6** (the uncontrolled-turn rule and the AoE basic attack change how
identical inputs resolve). No generation bump — classes are not a generation
input. No save bump.

## D. Slices

### D1 — The cross-save unlock (profile store)

New `game/Profile.hpp/.cpp`: a versioned `profile.json` in the user data dir
(the `AchievementStore` pattern — defensive load, atomic save), holding
`kingDefeated`. Set when the King falls (`CastleChallengeState::finish`), and
**retroactively** when any save whose `castleRecords.kingDefeated` is true is
loaded, so existing winners are not asked to do it again. `PartyCreationState`
lists the three classes always, marked **Locked** with a one-line hint until the
flag is set, and refuses to start with one selected.

### D2 — `ClassDef` schema additions (validated, all optional)

| Field | Meaning |
|---|---|
| `equipBan` | slots this class may never equip (`weapon` / `armor` / `accessory`) |
| `attackHitsAll` | the basic attack strikes every living foe |
| `attackStatuses[]` | statuses the basic attack applies per connecting hit (`{type, magnitude, duration}`) |
| `uncontrolled` | the class takes no player input — its turn is decided by a seeded rule |
| `scoreModPct` | per-member additive score modifier (may be negative) |

### D3 — Dragon

Very high stats, **no skills**, **no armor**. Its basic attack hits every living
enemy and applies **poison + blind** per connecting hit (to-hit rolled per
target through the existing Blind/Evasion path). Score **−20 % per member**.

### D4 — Jester

**Uncontrolled:** each turn a seeded pick among its affordable, castable known
skills plus the basic attack, aimed at a random living enemy — decided in shared
`battle::` code (`uncontrolledChoice`) used by `BattleState` **and** the
Simulator, so sim and live agree by construction. No weapon. Score **+5 %**.
A **15 % jest chance** shows one of twelve original dry one-liners in the M41
Jester's voice — **presentation only**, from a pure hash of
(`rngSeed`, turn, actor) that never advances `rollCursor`.

### D5 — Goose

Very low stats, **nothing equippable**. Its heal/cure skills also **buff every
enemy** (the owner's comedy tradeoff, expressed as a generic
`SkillDef.alsoBuffsEnemies` flag), and its level-30 ultimate costs **30 MP** and
applies all six debuffs to all enemies. Score **+5 %**.

### D6 — Scoring

`RunSummary.classModPct` (additive across the party) → a visible
`ScoreBreakdown.classMod` row, applied like the town bonus (a percentage of the
subtotal), plus an optional `ScoreEntry.classModPct` tag shown on the scoreboard.
M19 holds: tagged and visible, **never normalized**, ranking untouched.

### D7 — Equip flow, presentation, onboarding

The equip shop still sells anything, but equipping a banned slot is refused with
the reason; class select states each class's quirks honestly. Three generated
battle sprites + manifest + credits; capture scenes for locked/unlocked class
select, a jest line, and the score breakdown with the class row.

## E. Determinism & compatibility

- The Jester's turn is a pure function of (`rngSeed`, `rollCursor`, board); the
  jest line is a pure hash that never advances the stream. Same seed ⇒ same
  actions in sim and live.
- Saves: no schema change (`kSaveVersion` 1). The unlock lives in its own
  profile file; a missing file means "locked", never a crash.
- Content: five optional `ClassDef` fields and one optional `SkillDef` flag, all
  inert for the six existing classes.
- `kBattleRulesVersion` **5 → 6**; `kGenerationVersion` unchanged.

## F. Out of scope

M23/M24; new dungeons, bosses, or towns; any change to the six original classes.

## G. Acceptance criteria

1. The unlock round-trips, grants retroactively, and cannot be reached before the
   King falls.
2. Equip bans are enforced wherever equipping happens, with a stated reason.
3. A Jester's turns are identical in sim and live for a given seed; jest lines
   never perturb the battle stream.
4. The Dragon's attack hits every living foe and applies poison + blind per
   connecting hit, rolled per target.
5. The Goose's heals really do buff the enemies; its ultimate applies all six.
6. Score modifiers are additive, visible, tagged, and never change ranking.
7. Full suite green, capture overflow-clean, presentation lint green (including
   the three new class sprites).

## H. Manual validation for the owner

Whether the classes are funny **and** playable; the jest-line tone; whether the
score modifiers feel fair; whether the locked-class hint reads clearly.

## I. Required documentation updates on completion

`docs/game_design.md`, `docs/technical_design.md`, `docs/manual_test_matrix.md`,
`assets/credits.md`, `docs/milestones.md`, this note's as-implemented record, and
the completion report per `docs/milestone_completion_template.md`.

## J. As-implemented record (2026-07-22)

Implemented against `5608528`. **426/426 tests** green in Debug and Release
(405 baseline + 21 new), `--capture` **49/49 scenes overflow-clean** (three new),
presentation lint green including the three new class sprites, both
configurations build with no project warnings.

**What shipped**

- `game/Profile.hpp/.cpp` — the cross-save `profile.json`, wired through
  `AppContext`/`Application`, written on a King kill and retroactively on loading
  a save that already beat him.
- `ClassDef`: `unlockedByKing`, `equipBans` + `canEquip`, `attackHitsAll`,
  `attackStatuses[]`, `uncontrolled`, `scoreModPct` — validated, optional, inert
  for the six originals (a test pins that).
- Battle: `attackOne` / `attackAll` / `applyAttackStatuses`;
  `uncontrolledChoice` and `jestThisTurn` as pure hashes; `SkillDef.alsoBuffsEnemies`
  for the Goose; class traits resolved onto `Combatant` at `buildBattle`.
- Party/equip: `partyClassModPct`, `canEquipSlot`, the equip shop's stated refusal.
- Scoring: `RunSummary.classModPct` → `ScoreBreakdown.classMod` → a result row and
  the `ScoreEntry.classModPct` scoreboard tag.
- Content: three classes and three Goose skills; three generated 24×24 sprites +
  manifest + credits; twelve jest lines in the M41 Jester's voice.

**Deviations from the plan / note**

1. **The Simulator now tracks `turnsTaken`.** Not in the plan: found while making
   the Jester's per-round hash safe. The simulator left the counter at 0 while
   `BattleState` counted rounds, and `chooseEnemyAction`'s tie-break jitter reads
   it — so the two drivers could already disagree on targeting in a near-tie.
   Fixing it changes some battle outcomes, which the rules bump (6) covers. This
   is the same class of bug M43 was chartered to remove, so it was fixed rather
   than filed.
2. **The uncontrolled turn is a pure hash, not a roll-stream draw.** The plan left
   the mechanism open. A hash needs no ordering contract between the drivers,
   which is strictly safer — and it is what deviation 1 enables.
3. **`Combatant::attackStatuses` stores the model's own `StatusInstance`** rather
   than `content::AttackStatus`, so `battle/Battle.hpp` still needs no content
   definitions; `buildBattle` converts.
4. **The Jester keeps a small skill set** (4 starting + 3 learned) rather than the
   plan's silence on the matter — an uncontrolled unit with no options is just a
   worse basic attack, and with too many it never repeats. Reversible data.
5. **A valueless-item rule from M44 was not needed here**; no new items ship.

**Evidence — the class battery** (`[classes-report]`, level-30 parties vs the
depth 1 / 5 / 10 bosses of seed 4242; outcome 1 = win):

| party | class mod | d1 | d5 | d10 |
|---|---|---|---|---|
| baseline (knight/ranger/mage/cleric) | 0 % | 1 / 2 rounds | 1 / 2 | 1 / 2 |
| all Dragon | −80 % | 1 / 1 | 1 / 1 | 1 / 1 |
| all Jester | +20 % | 1 / 3 | 1 / 3 | 1 / 2 |
| all Goose | +20 % | 1 / 8 | 1 / 10 | 1 / 11 |
| one of each + knight | −10 % | 1 / 2 | 1 / 2 | 1 / 2 |

The shape is the intended one: Dragons end fights fastest and pay the most for
it; Geese survive but bleed turns (the score's dominant penalty) and are paid a
little for the trouble; Jesters land in between and are simply unpredictable.

**Known items for owner validation**

Whether the three classes are funny **and** playable; the twelve jest lines'
tone; whether −20 / +5 / +5 are the right prices; whether the locked-class hint
reads clearly.

**Build-hygiene observation (not introduced here):** the build copies `data/`
next to the exe as a post-build step, so a **data-only** edit does not refresh
`build-msvc/data/` until something relinks. Tests read the source `data/`
directly and were unaffected, but a `--capture` run can use stale content — worth
a follow-up if it bites again.
