# M38 — Per-Town Enemies & Bosses

## A. Status and authority

- **Status:** implemented, awaiting manual approval — authored / re-audited and
  implemented 2026-07-21 against HEAD `c9e78a4` ("M37"). Owner authorized beginning
  M38 after approving M37. As-implemented record in §K. Only the owner sets
  `complete (approved)`.
- **Authority:** `CLAUDE.md` > this note > `docs/milestones.md` >
  `docs/game_design.md` > `docs/technical_design.md`. Fourth milestone of the
  M35–M42 endgame program. Changes generation output → `dungeon::kGenerationVersion`
  **7 → 8**. Produces the full boss roster (~12) that M40's gauntlet enumerates.

## B. Problem statement (verified at re-audit, HEAD `c9e78a4`)

- **Enemy/boss content does not track the ladder.** The roster (22 normal, 9
  elite, 6 bosses) is available from town 1; higher towns only re-scale the same
  foes. M37 gave gear a per-town progression; the foes need one too.
- **The gating hook exists.** `EnemyDef`/`BossDef` need a `minTown` (optional,
  default 1) mirroring `ItemDef.minTown` (M37). `buildPools(db, theme, town)`
  already filters items by town; the enemy pools it builds from the theme (and
  `pickBoss`) filter the same way ([DungeonGenerator.cpp:33](src/dungeon/DungeonGenerator.cpp),
  :76). Sorted-id determinism is preserved.
- **Every enemy/boss id must resolve its own battle sprite.** The M26 lint guard
  (`test_presentation_lint.cpp`, [:90](tests/test_presentation_lint.cpp:90)) fails
  if any content `enemy.<id>.battle` / `boss.<id>.battle` is missing from the
  manifest. Sprites are 24×24 (`enemies/<id>.png`) / 32×32 bosses
  (`enemies/boss_<id>.png`) produced by `tools/asset_gen/generate_textures.ps1`
  (deterministic, rerun byte-identical; provenance in `assets/credits.md`).
- **Statuses (M35) and passives (M36) are ready to build kits from.** New enemies
  weave in Confusion/Silence/Blind where the role fits; new bosses carry passives.
- **Baseline:** 336/336 tests, 30/30 capture, `kGenerationVersion` 7,
  `kBattleRulesVersion` 3, `kSaveVersion` unchanged.

## C. Goals

- `EnemyDef.minTown` + `BossDef.minTown` (default 1); generation pools filter by
  the dungeon's town before team/boss build (sorted order preserved →
  deterministic).
- **≥ 2 new standard enemies + 1 new boss per town for towns 2–7** (+12 enemies,
  +6 bosses), each with: its own sprite (lint-enforced), an `EnemyRole`/archetype
  behaviour profile (M28), a kit built around the M35 statuses, and (bosses) M36
  passives. Roles fill gaps, not quantity (M20 rule).
- `dungeon::kGenerationVersion` **7 → 8** (the town-gated enemy/boss pools change
  a seed's roster).
- Balance battery extended with per-town rows; the town × depth clearability grid
  re-validated with the new pools and M37 gear assumed available.

## D. Owner decisions & routine choices

Locked at program planning (not re-opened): ≥ 2 new standard enemies + 1 new boss
per town; new bosses have passives; kits built on M35 statuses.

Routine choices taken here (owner validates distinctness/balance at approval):

- **Theme assignment: all three themes, gated by `minTown`.** The new enemies/
  bosses are added to every theme's pools and gated by town, so they reliably
  appear at their town in any theme (tougher foes roam every region as the ladder
  deepens). Theme identity is carried by flavor and the original roster; a
  per-theme split would risk a theme having too few town-appropriate foes.
- **Roster (12 enemies + 6 bosses):** 2 enemies + 1 boss per town (towns 2–7),
  each with a distinct role/kit and base stats a notch above the base roster for
  its tier (the town multiplier scales them further). Bosses reuse the 4
  archetypes across towns with fresh stats/kits/passives. Sim-validated so the
  town × depth grid stays clearable with M37 gear.
- **Sprites:** new procedural 24×24 / 32×32 sprites composed from the existing
  art-bible primitives (distinct silhouettes, town/role-appropriate palettes),
  in the already-approved M15 direction — production, not a new art direction.
- **No battle/save change:** `minTown` is content; `kBattleRulesVersion` 3 and
  `kSaveVersion` unchanged. Old scoreboards read their stored generation version.

## E. Proposed design & slices

1. **Schema + gating.** `EnemyDef.minTown` / `BossDef.minTown` (parse
   `optIntMin("minTown", 1, 1)`); `buildPools` filters theme/fallback enemy pools
   by `minTown ≤ town`; `pickBoss(rng, theme, db, town)` filters bosses the same
   way; `kGenerationVersion` **7 → 8**. Determinism tests updated.
2. **Content.** `enemies.json` (+12), `bosses.json` (+6), `dungeon_themes.json`
   (new ids added to each theme's pools). Kits use M35 statuses; bosses carry M36
   passives; roles/tags/archetypes assigned for M28 behaviour.
3. **Sprites.** 18 new procedures in `generate_textures.ps1`; run it (byte-
   identical for existing art); 18 manifest rows (`enemy.<id>.battle` /
   `boss.<id>.battle`); `assets/credits.md` entries. Lint enforces coverage.
4. **Tests/balance/capture/docs.** Extend `[lint]` (auto-covers new ids),
   `[content]` counts, a town-gated-generation determinism test; re-run the
   `[economy-report]` town × depth grid; a capture scene showing a high-town team;
   update `game_design.md`/`technical_design.md`/README.

## F. Determinism & save compatibility

- **`kGenerationVersion` 7 → 8** (owner-approved): the town-gated enemy/boss pools
  change a seed's roster. Same seed + town + version still reproduces the same
  dungeon; the scoreboard tags the version (M19). Town-1 pools are unchanged (all
  new content is `minTown ≥ 2`), but the version bump reshuffles room realizations
  for every town.
- **No `kSaveVersion` bump.** `minTown` is content. New enemy/boss ids referenced
  by a save (none are — saves store party, not encounters) are irrelevant.
- **No `kBattleRulesVersion` change** (new kits use existing status/passive
  mechanics; battle resolution is unchanged).

## G. Out of scope

Boss legendary/token drops (M39); the castle & the King (M40, though this
milestone finalizes the boss roster it uses); story (M41); any battle-rule or
save-schema change; new statuses/passives beyond M35/M36.

## H. Balance / acceptance

Every new enemy/boss has its own sprite (lint green) and a declared role/
archetype; generation stays deterministic (same seed + town + v8 → same dungeon);
the town × depth clearability grid stays clearable with M37 gear assumed; the new
kits are legible (statuses/passives read in play); the full boss roster (~12) is
enumerable in sorted order for M40.

## I. Manual validation for the owner

Whether the new foes feel varied and distinct (art + behaviour) without reading
names; whether per-town difficulty escalates fairly; whether the new bosses'
passives/kits read in play; balance across towns 2–7.

## J. Required documentation updates on completion

This note (§K); `docs/milestones.md` (status); `docs/game_design.md` (per-town
foes + content counts); `docs/technical_design.md` (`minTown` gating,
`kGenerationVersion` 8); `assets/credits.md` + `docs/asset_pipeline.md` (18 new
sprites); README (content line); `docs/manual_test_matrix.md` rows; a manual
checklist per `docs/milestone_completion_template.md`.

## K. As-implemented record (2026-07-21)

- **Status:** implemented, awaiting manual approval. Implemented / audited against
  HEAD `c9e78a4`. The §D routine choices were taken as written (all-themes gating;
  12 enemies + 6 bosses; new procedural sprites in the approved direction).
- **Schema + gating.** `EnemyDef.minTown` / `BossDef.minTown` (parsed
  `optIntMin("minTown", 1, 1)`). `buildPools(db, theme, town)` filters the theme/
  fallback enemy pools by `minTown ≤ town`; `pickBoss(rng, theme, db, town)` filters
  bosses the same way. `dungeon::kGenerationVersion` **7 → 8**.
- **Content.** `enemies.json` +12 (4 normal: dune_reaver, hex_wisp, grave_wight,
  shardling; 8 elite: ironclad_reaver, blight_chanter, war_caller, void_stalker,
  dread_knight, soul_render, titan_guard, archon_of_ruin) and `bosses.json` +6
  (sand_warlord, frost_monarch, obsidian_colossus, hollow_sovereign, abyssal_tyrant,
  dread_sovereign — one per town 2–7, across the 4 archetypes, each with M36
  passives and a status kit). All added to every theme's pools in
  `dungeon_themes.json`, gated by `minTown` at generation.
- **Sprites.** 18 new procedural battle sprites in `generate_textures.ps1`
  (12 × 24×24, 6 × 32×32 bosses), **appended last** so their speckle RNG never
  shifts earlier files — the generator reran with **0 modified / 18 new** PNGs.
  18 manifest rows (`enemy.<id>.battle` / `boss.<id>.battle`) + `assets/credits.md`
  entry; the M26 lint auto-covers the new ids.
- **Validation (this session, VS 2022 / MSVC 14.44, `build-msvc`).** Debug build
  clean; full suite **338/338** (336 M37 baseline + 1 new `[townladder]` gating
  test + 1 new `[balance]` town-7 clearability test; updated the count guards
  `enemyCount` 31→43 and `bossCount` 6→12, and the cross-town `[townladder]`
  invariant to reflect the town-gated pools). `--capture` **31/31**
  overflow-clean (new `31_battle_high_town` scene; the boss scene now shows a new
  boss). Release build + Release suite **338/338** clean. `[economy-report]`
  town-1 grid **unchanged** (new foes are `minTown ≥ 2`); the new **town-7 boss
  content is clearable** by a maxed legendary party (5–6/6 seeds), and the
  black-market no-trivialization test still holds. Lint green (all new sprites).

### Deviations from the plan / note

1. **All-themes gating** (each new foe added to every theme, gated by `minTown`)
   rather than a per-theme split, so the content reliably surfaces at its town in
   any theme; flagged for owner veto.
2. **M32's "town does not change enemy composition" invariant is intentionally
   retired** — town now selects from a town-gated pool. Topology and
   per-(seed,depth,town) determinism are unchanged; the `[townladder]` test was
   updated to the new behavior and a positive gating test added.
3. **New elites at towns 4–7** — "standard enemies" was read as "non-boss foes";
   the tougher high-town ones are elite so the composition system caps their
   numbers (a normal-tier hp-150 foe appearing in fives would be punishing).

### Known items for owner validation

- Sprite distinctness/readability of the 18 new foes (art is a manual-validation
  item); whether per-town difficulty escalates fairly through towns 2–7; whether
  the new boss kits read in play.
