# M49 — The King's Court

## A. Status and authority

- **Status:** ☑ complete (approved) — approved by the owner **2026-07-23** after
  manual testing, committed as `ee079d4`. Approval covers the base milestone plus
  the two owner-directed changes made during review: the castle raised above the
  ladder (Boss Rush 580 %, King 500 %, Endless 500 %+10 %/wave) and the level cap
  50 → 99. Authored just-in-time on 2026-07-23 from the
  owner-approved M47–M51 "Court & Comfort" plan. Third milestone of that
  program; M50 → M51 follow, **then** M23 → M24. See §J for the as-implemented
  record.
- **Authority:** `CLAUDE.md` → this note → `docs/milestones.md` →
  `docs/game_design.md` → `docs/technical_design.md`.
- **Base checkout:** `d9d9960` (message reads "M47"; contents are M48), working
  tree clean at authorization.
- **Baseline:** 462/462 tests, `--capture` 56/56 scenes overflow-clean.
- **Versions at baseline:** `battle::kBattleRulesVersion` **8**,
  `dungeon::kGenerationVersion` **10**, `kSaveVersion` **1**,
  `kSettingsVersion` **1**.
- **Terminal state for this session:** `implemented, awaiting manual approval`.

## B. Re-audit against `d9d9960`

| Plan claim | Verified? |
|---|---|
| `kingTeam()` is solo at `kKingScalePct = 340`; `bossRushTeam()` is solo bosses at `kBossRushScalePct = 330` | ✅ [Castle.cpp:23–70](../../src/game/Castle.cpp:23) — neither sets `team.enemyIds` |
| Every regular boss already has a `minions[]` list; the King's is empty | ✅ all 12 have 1–4 minions; `the_hollow_king` has `[]` |
| `iron_will`, `evasion`, `spell_ward` all exist | ✅ `data/passives.json` — magnitudes 1 / 25 / 25; `applyPassives` already resolves enemy and boss passive lists in `buildBattle` |
| No per-turn boss mechanics exist — only HP-threshold flags | ✅ confirmed. Both drivers do `tickStatuses(actor)` → alive check → `clearGuard(actor)` → act, so there is a clean shared seam for a per-turn rule (§E3). |
| **"Referenced ONLY from the King's `minions[]` … never in composition pools, so dungeons are untouched"** | ❌ **Not sufficient.** Two paths sweep the *entire* enemy database regardless of theme: `endlessWaveTeam` ([Castle.cpp:42](../../src/game/Castle.cpp:42)) draws every wave from it, and `buildPools`' fallback ([DungeonGenerator.cpp:52–64](../../src/dungeon/DungeonGenerator.cpp:52)) uses it whenever a theme's town-gated pool comes up empty. Guards added to `enemies.json` **would** appear in the Endless Rush. §E1 adds an explicit exclusion flag rather than relying on nobody listing them. |
| **"King-targeted buffs (ATK+/DEF+ via existing statuses)"** | ⚠️ **Not reachable as described.** `chooseEnemyAction`'s Support branch aims an ally-facing skill at `actor` — *itself* ([Battle.cpp:1443](../../src/battle/Battle.cpp:1443)); there is no "buff a chosen ally" rule. Delivered instead as **`all_allies`** buffs, which `resolveTargets` expands to the whole enemy side, so the King is buffed without touching the AI. The existing `battle_cry` (ATK+25) and `guard_aura` (DEF+30) do exactly this — **no new skills needed**. |
| Two new 24×24 sprites via the deterministic generator + manifest + credits + lint | ✅ `SaveEnemy` in `tools/asset_gen/generate_textures.ps1:511`; the manifest is hand-authored; **`test_presentation_lint.cpp:91` requires `enemy.<id>.battle` for every enemy in the database**, so the sprites are mandatory, not optional |
| The balance bar to protect | ✅ `relics: the doubled King demands the counterplay` — with **1 Tax Sheets + 1 Evil Goose + 20 Royal Snacks** the maxed party must **win**, and unaided must **lose**. `runScripted` + `counterplayHook` in `test_royal_relics.cpp` is the harness to reuse. |
| Boss Rush baseline | ✅ `[castle-report]`: cleared 12/12 in **36 rounds** at 330 %, solo bosses |

## C. Goals

The King stops fighting alone. Two **Royal Guards** join him, and a
deterministic **revive clock** (rules **8 → 9**) means killing them is not a
one-time chore. The **Boss Rush** fields each boss's own dungeon minions, which
is the same fight the dungeon already teaches. Clearability is re-proven with
sim evidence under the M47 defeat stakes, and the scale constants may be retuned
inside this milestone.

No generation, save, or settings bump.

## D. Constraints

- The revive clock is **shared `battle::` code** called by both drivers — never
  in `BattleState` alone.
- Deterministic: the clock is a counter, not a roll; `rollCursor` is untouched.
- Content-driven: **no boss id is branched on in code.** The clock is a
  `BossDef` field any boss could carry, exactly as `immuneToConfusion` (M40) is.
- New UI (the revive announcement) uses the existing battle message path.

## E. Slices

### E1 — The Royal Guards (content + an exclusion flag)

Two new enemies in `data/enemies.json`, elite tier:

| | `royal_guard_sword` | `royal_guard_staff` |
|---|---|---|
| Name | Throne Blade | Throne Stave |
| Role | protector | sniper |
| Kit | `arcane_burst`, `smite`, `battle_cry` | `shadow_bolt`, `frost_lance`, `guard_aura` |
| Passives | `iron_will`, `evasion` | `spell_ward`, `evasion` |
| Weakness | lightning | earth |
| Immunities | none | none |

Shared temper: **high DEF and MAG, decent HP, low SPD** — they outlast and
out-cast rather than out-run. Base stats are set modestly because
`kKingScalePct` multiplies them by 3.4 (§E5 tunes the exact numbers against the
sim). Neither is immune to anything: the M48 no-dead-weapon rule holds, and a
guard that shrugs off an element would make an already-long fight opaque.

Both carry an **`all_allies`** buff (§B), so the King is braced by his court —
the mechanically interesting part — without an AI change.

**New optional `EnemyDef` field `bossOnly`** (default false): an enemy that may
appear **only** through a boss's `minions[]`. Filtered out of `buildPools` (both
the theme path and the whole-database fallback) and out of `endlessWaveTeam`.
This exists because "just don't list them in a theme" is not enough — two code
paths sweep the whole database (§B). The bestiary still lists them (a mystery
worth having), and they are still gated behind actually meeting them.

**Sprites:** two 24×24 entries in `generate_textures.ps1` via the same
`New-Img` / `SaveEnemy` DSL, regenerated into `assets/textures/enemies/`, plus
two `manifest.json` entries and `assets/credits.md` rows. Regal, plated,
gold-accented, readable as a matched pair distinct from the King.

### E2 — Minions for the Boss Rush and the King

`bossRushTeam()` and `kingTeam()` both populate `team.enemyIds` from
`BossDef.minions`, exactly as a dungeon boss encounter does. Bosses with an
empty list stay solo. The King's list becomes the two guards, so one change
serves both features.

### E3 — The revive clock (rules 8 → 9)

**Schema:** `BossDef.reviveMinionTurns` (optional, default **0 = never**). The
King gets **5**. Any future boss can have the mechanic without new code.

**Model:** `Combatant` gains `reviveMinionTurns` (resolved from the BossDef in
`buildBattle`) and a `reviveMinionCounter`. New shared method:

```
std::string Battle::beginUnitTurn(int actor);   // "" when nothing happens
```

called by **both** drivers immediately after `clearGuard(actor)`. Its rule, for
an actor with `reviveMinionTurns > 0`:

1. if there are **no** non-boss enemy units at all → do nothing (a King with no
   court cannot count toward reviving one);
2. if **any** of them is alive → reset the counter to 0 and do nothing;
3. otherwise increment; when it reaches `reviveMinionTurns`, **revive every
   fallen minion to full HP**, reset the counter, and return the announcement.

Counted on **the King's own turns** — the deterministic anchor, chosen because a
round counter would drift with speed and turn order while a unit's own turn
sequence will not. **Repeats indefinitely** (owner decision). "His guards" is
"every non-boss enemy unit", which needs no id branching: in a boss battle those
are exactly the boss's minions.

### E4 — Presentation

The announcement rides the existing message path (`BattleState` prepends the
returned line to the turn's message; the Simulator ignores it, as it does every
log line). Telegraph-style, in the King's voice. The revived guards' HP bars
refill through the existing `koFade_` revive path (M42 already handles a unit
returning from 0 HP).

### E5 — Balance evidence (mandatory)

Two batteries, both tabulated in §J:

- **(a) The King + guards + clock at 340 %** — must stay **beatable by a maxed
  party with obtainable counterplay** (1 Tax Sheets + 1 Evil Goose + 20 Royal
  Snacks — the M44 approved bar) and **unwinnable without it**. This is an
  existing test that must keep passing, not a new claim.
- **(b) The full Boss Rush with minions**, no free healing, under the M47
  harsh-defeat stakes — must remain **clearable**.

`kBossRushScalePct` and, if needed, `kKingScalePct` are **retuned inside this
milestone** with the evidence table showing what was tried. Adding 1–4 minions
to all 12 rush fights is a large difficulty increase; a scale reduction is
expected and is a tuning change, not a design change.

## F. Tests

New `tests/test_kings_court.cpp`:

- **Revive clock:** fires on the 5th King turn with both guards down; **repeats**
  a second time; **resets** when a guard is alive (revived by any means); never
  fires with a guard alive; never fires for a boss with `reviveMinionTurns == 0`;
  no-op for a boss with no minions at all; **sim == live** (the same battle driven
  by `applyChoice` and by direct calls agrees on revive timing and HP).
- **Team composition:** `kingTeam()` fields the two guards; `bossRushTeam(i)`
  fields boss *i*'s authored minions; a minion-less boss stays solo.
- **The guards never leak:** `bossOnly` enemies appear in **no** generated
  dungeon across a mass sample of seeds/towns/themes, and in **no** endless wave.
- **Guard resolution:** both guards' passives land on their `Combatant`s through
  `buildBattle`.
- **Content lint:** both guards have sprites in the manifest (the existing lint
  covers this automatically once they are in the database).

## G. Capture

Two scenes (56 → **58**): the King's court mid-battle (King + two guards), and
the revive announcement frame.

## H. Documents to update

`docs/game_design.md` (the King's Court; Boss Rush minions),
`docs/technical_design.md` (rules v9: the clock, `bossOnly`, the turn seam),
`docs/manual_test_matrix.md` (new rows), `assets/credits.md` (two sprites),
`docs/milestones.md`, `README.md`, this note's §J.

## I. Acceptance criteria

1. The King fights with two Royal Guards; the Boss Rush fields every boss's
   authored minions; a minion-less boss stays solo.
2. The revive clock fires on the King's 5th turn with both guards down, repeats,
   resets when one is alive, and is proven sim == live.
3. The guards appear in **no** dungeon and **no** endless wave (mass-sample test).
4. The King remains beatable with the approved counterplay and unbeatable
   without it; the Boss Rush remains clearable under the M47 stakes — both with
   an evidence table naming the final scale constants.
5. Both guards have original generated sprites; presentation lint green.
6. `kBattleRulesVersion` 9; generation/save/settings unchanged.
7. Full suite green in Debug **and** Release; capture 58/58 clean.

## J. As-implemented record

All slices landed. Verified 2026-07-23 on `d9d9960`: **477/477 tests**
(462 baseline + 15 new court cases) green in **Debug**, `--capture` **58/58**
scenes overflow-clean, no warnings introduced.

### Deviations from this note

1. **`kKingScalePct` 340 → 310 and `kBossRushScalePct` 330 → 260.** §E5
   anticipated this and the plan authorized it; the numbers are measured, not
   estimated (table below). The **bars are unchanged** — the two tests that
   define them were not touched.
2. **The guards' base stats were tuned down before the scale was touched.** The
   first pass (hp 90/84, mag 24/28) made the approved counterplay lose at every
   scale down to 260 %. Weakening the guards first (hp 70/64, mag 14/16) kept
   the King's own difficulty — an owner decision from M44 — carrying the fight,
   with the court as pressure rather than as a second boss.
3. **A rush scale sweep was added to `[castle-report]`.** The King already had
   one (`[king-report]`); the rush did not, and tuning it by hand would have been
   guesswork. It is on-demand (`[.]`), so it costs the normal suite nothing.
4. **Capture scene `34_king_battle` was narrowed to the boss.** It exists to
   prove the King's immune statuses do *not* render as labels; with the court
   present, applying those statuses to every enemy would have buried the very
   thing it checks. The guards are not immune, so the statuses now go on the
   boss only.
5. **The guards were renamed and the King's telegraph shortened**, both because
   the presentation lint failed on them: an enemy name must be ≤ 16 characters
   (the battle HUD's budget) and "The Sword of the Throne" is 23, while the new
   telegraph wrapped to 3 lines against a 2-line region. Renamed to **Throne
   Blade** / **Throne Stave** and the telegraph trimmed to 101 characters. The
   lint doing its job is the reason this was caught before the owner saw it.
6. **The guard sprites carry no `Speckle`.** The generator advances one
   script-wide RNG stream, so a random call appended mid-file silently re-rolls
   every sprite generated after it — the first run changed all six per-town boss
   PNGs. Placed pixels are used instead, and the reason is recorded in both the
   script and `assets/credits.md` so the next person does not rediscover it.

### ⚠️ Owner override, 2026-07-23 — the castle floor (supersedes the tuning below)

After reviewing the scaling report, the owner directed that **every castle
challenge must start above the strongest multiplier a dungeon can produce**
(town 7 at the depth cap = **570 %**), explicitly overriding the previously
approved balance bars: *"It is supposed to be very hard, so if the battles are
simulated, we must adjust them to make room for the Castle being harder
(regardless of what has been approved before). Only adjust the castle, the Towns
stays as they are."*

Applied: `kBossRushScalePct` **600**, `kCastleBaselineScalePct` **600**,
`kKingScalePct` **700**, with `castleFloorScalePct()` deriving the 570 % ceiling
from the live ladder + composition rules so the invariant cannot drift. Towns,
dungeons, party, and relics untouched.

**Measured consequence — the owner must weigh this.** At the new scales the
simulator's maxed party does not merely lose; it loses with **zero survivors at
every scale above the floor, carrying the maximum counterplay the game
contains**:

| King scale | eff. HP | unaided | plan (1+1+20) | MAX (3+3+30+crown+spoon) |
|---|---|---|---|---|
| 580 % (floor + 10) | 4350 | loss, 10 rounds | loss, 16, **0 alive** | loss, 34, **0 alive** |
| 600 % | 4500 | loss, 8 | loss, 14, 0 alive | loss, 33, **0 alive** |
| 650 % | 4875 | loss, 9 | loss, 15, 0 alive | loss, 31, **0 alive** |
| **700 % (chosen)** | **5250** | loss, 8 | loss, 11, 0 alive | loss, 28, **0 alive** |
| 800 % | 6000 | loss, 6 | loss, 9, 0 alive | loss, 16, 0 alive |

Boss Rush at 600 %: **1 of 12** fights survived (was 12/12 at 260 %).
Endless from 600 %: best wave **6** (was 11).

The simulator is a *floor* on player skill — it never aims at an element, never
times an item well, never guards, never manipulates turn order — so a competent
player does strictly better. But the gap here is not marginal, and the
consequence is concrete: **beating the King is what unlocks the Dragon, Jester
and Goose for every future New Game (M45)**, plus the Sovereign's Regalia and the
title. If he is unbeatable in practice, that content is unreachable.

**Resolution (owner tuning pass + level-cap raise, 2026-07-23).** After seeing
the table above the owner set Boss Rush **580 %**, King **500 %**, endless
**500 %** climbing **+10 %pts/wave**, AND raised the level cap **50 → 99** (its
own change — see `kMaxLevel` in `game/Party.hpp`) so the endgame party has real
answers. The King and the endless opening are below the 570 % *multiplier*
ceiling, so the "above the ladder" rule was re-expressed in **effective stats**
(the King's base stats make him a 3750 HP fight at 500 %, larger than any dungeon
boss's 2280) — see `castle: the castle outclasses the deepest dungeon`.

All numbers below are re-measured with the **level-99** maxed party (the cap the
sims run at). The King at 500 % is now beatable well short of the maximum
loadout:

| King counterplay (level-99 party, 500 %) | outcome |
|---|---|
| nothing | loss, 18 rounds |
| 1 sheets + 1 goose + 20 snacks (the M44 plan bar) | loss, 22 |
| 2 + 2 + 30 snacks | loss, 26 |
| **3 + 3 + 30 snacks** | **win, 3 alive @33 % HP** |
| 3 + 3 + 30 + Crown | win, 4 alive @44 % |
| 3 + 3 + 30 + Crown + Spoon | win, 4 alive @75 % |

So the fight rewards a full relic haul (three of each 40 % relic) without
demanding the absolute maximum, and a bare party still loses. The class-unlock
reward is reachable by a player who has farmed the relic set, with headroom for
the Crown/Spoon to make it comfortable. **The feel of that gate is matrix
row 125.**

Boss Rush at 580 %: the sim survives **4 of 12** (was 1/12 at level 50; a real
player does better still — manual row 126). Endless from 500 %: best wave **19**
(was 6), a genuine push that passes the ladder ceiling around wave 8.

For reference, the pre-cap-raise level-50 table (now superseded) had the King
winnable only with the full arsenal at 500 % (2 survivors at 16 % HP); raising
the cap is what turned the gate from "everything the game contains" into "a full
relic haul", which is the healthier bar the owner was after.

### Balance evidence (superseded — recorded for the tuning history)

**The King** (bar: unaided must lose; 1 Tax Sheets + 1 Evil Goose + 20 Royal
Snacks must win) — with the court present:

| scale | unaided | approved counterplay |
|---|---|---|
| 420 % | loss (11 rounds) | **loss** (22) |
| 380 % | loss (14) | **loss** (24) |
| 340 % (pre-M49 value) | loss (15) | **loss** (24) |
| 320 % | loss (15) | **loss** (21) |
| **310 % (chosen)** | **loss** | **win** |
| 300 % | loss (17) | win (16, 3 of 4 alive) |
| 260 % | loss (16) | win (14, 4 alive) |
| 220 % | **win (9)** — bar broken | win (10) |

310 % is the highest multiplier at which the approved bar holds in both
directions.

⚠️ **A tuning hazard worth recording:** `buildBattle` seeds the targeting
tie-break from the combatants' **names**. Renaming the guards (forced by the
lint, deviation 5) moved the sim's outcome at the margin and invalidated a
sweep taken before the rename — 320 % passed with the long names and fails with
the short ones. The numbers above are all post-rename. Re-run the sweep after
any rename in this fight, not before.

**The Boss Rush** (bar: a maxed party clears all 12 with no free healing) — with
each boss's minions:

| scale | cleared | fights survived | rounds |
|---|---|---|---|
| 330 % (pre-M49 value) | no | 4 / 12 | 22 |
| 300 % | no | 4 / 12 | 19 |
| 280 % | no | 5 / 12 | 22 |
| 270 % | no | 6 / 12 | 31 |
| **260 % (chosen)** | **yes** | **12 / 12** | **44** |
| 240 % | yes | 12 / 12 | 37 |
| 200 % | yes | 12 / 12 | 31 |

260 % is the highest clearing value, and 44 rounds against the old 36 makes the
gauntlet longer and more attritional — the intended shape.

Endless is unchanged (best wave 11), as expected: it never used boss minions.

### Files changed

- **Source:** `src/content/Definitions.hpp` (`bossOnly`, `reviveMinionTurns`),
  `src/content/ContentLoader.cpp`, `src/battle/Battle.hpp/.cpp`
  (`beginUnitTurn`, the clock fields, `buildBattle` resolution, rules 9),
  `src/battle/Simulator.cpp` (the shared call),
  `src/states/BattleState.hpp/.cpp` (`turnOpenLine_`, `captureCourtRevival`),
  `src/dungeon/DungeonGenerator.cpp` (`spawnable`), `src/game/Castle.hpp`
  (both scale constants), `src/game/Castle.cpp` (minions in both builders, the
  endless filter), `src/capture/CaptureRunner.cpp` (2 scenes + scene 34).
- **Tests:** **new** `tests/test_kings_court.cpp` (15 cases),
  `tests/test_castle.cpp` (the rush sweep), `tests/CMakeLists.txt`.
- **Content:** `data/enemies.json` (2 guards), `data/bosses.json` (the King's
  minions, telegraph, `reviveMinionTurns`).
- **Assets:** 2 generated sprites, `assets/manifest.json`, `assets/credits.md`,
  `tools/asset_gen/generate_textures.ps1`.
- **Docs:** this note, `docs/milestones.md`, `docs/game_design.md`,
  `docs/technical_design.md`, `docs/manual_test_matrix.md` (rows 123–127),
  `README.md`.

### Known limitations

- **The sim is the only evidence for a fight that is mostly about tempo.** The
  revive clock rewards bursting the King down inside a window; the sim's party AI
  does not plan around windows at all, so it almost certainly under-plays the
  fight. The chosen 320 % is therefore a floor on difficulty for a competent
  player, and the owner's feel-test is the real check.
- **The Boss Rush is now a long sitting** — 12 escorted fights, ~44 simulated
  rounds. Clearable, but the time cost is a design question the sim cannot answer.
- The guards appear in the Bestiary as unmet foes before you ever reach the
  castle. Deliberate (the roster's size is meant to be visible), but it does
  reveal that the King has a court.
