# M26 — Enemy Visual Identity

## A. Status and authority

- **Status:** implemented, awaiting manual approval
- **Authored / audited:** 2026-07-20 against the current checkout (HEAD is the
  M25 approval commit; working tree otherwise clean). Every code/data reference
  below was read at that point.
- **Authority:** `CLAUDE.md` > this note > `docs/milestones.md` >
  `docs/game_design.md` > `docs/technical_design.md`. Art direction is governed
  by `docs/art_bible.md` (approved at the M15 gate).
- **Authorization:** the owner authorized beginning M26 on 2026-07-20 after
  approving M25.

## B. Problem statement

The battle-sprite resolution already prefers a per-id sprite before the tier
fallback — `BattleState::drawUnit` builds `enemy.<sourceId>.battle` /
`boss.<sourceId>.battle` and only falls back to `enemy.normal.battle`,
`enemy.elite.battle`, or `boss.generic.battle` when the specific id has no
manifest texture. But the shipped manifest provides **only** those three
generic sprites, so all 23 enemies collapse onto two images and all 4 bosses
onto one. Every enemy "looks the same" in battle.

**This milestone needs no gameplay code** — it is assets + manifest rows + a
lint guard + docs.

## C. Roster (27 ids — the definition-of-done set)

Audited from `data/enemies.json` and `data/bosses.json`:

- **16 normal:** `goblin_grunt`, `skeleton_archer`, `cave_bat`, `stone_golem`,
  `venom_spider`, `dark_acolyte`, `kobold_scout`, `zombie`, `wild_boar`,
  `bandit`, `frost_imp`, `mud_crawler`, `wisp`, `forest_wolf`, `bog_shaman`,
  `war_drummer`.
- **7 elite:** `ogre_marauder`, `troll_berserker`, `crystal_guardian`,
  `shadow_stalker`, `plague_bearer`, `iron_sentinel`, `grave_chanter`.
- **4 boss:** `keep_warden`, `crystal_sorcerer`, `hollow_commander`,
  `rush_tyrant`.

## D. Goals

- Every shipped enemy and boss id resolves to its **own** battle sprite with a
  distinct silhouette; danger reads through shape + size + accent (art bible §5),
  never hue alone.
- The generic tier sprites are **retained** as fallbacks for future ids that do
  not yet have bespoke art (e.g. mid-M29).
- Generation stays deterministic (byte-identical reruns).
- A missing per-id sprite becomes a **failing test**, not a silent runtime
  fallback.

## E. Non-goals

- Any gameplay, stat, AI, generation, scoring, or save change.
- Overworld/dungeon enemy markers (already shape-distinct from M17) and
  animation (enemies are static in battle).
- New enemies/bosses (M29) — but the M26 lint guard will force those to ship
  with art when they arrive.

## F. Design (within the approved art bible)

- **Footprints unchanged:** normal & elite 24×24, boss 32×32 (art bible §3), so
  battle layout is untouched (`drawUnit` anchors bottom-center on the 40×16
  footprint). Enemies face right; 1px `#0E0C14` outline; 3-band shading from the
  §2 ramps; top-left light.
- **Tier signalling:** normal = plain hunched/bestial shape; elite = larger +
  horns/crest + a violet (`#9C6CE8`) accent band; boss (32×32) = largest, a
  gold crown/crest + a violet/cyan crystal glow.
- **Per-id silhouette** keyed to name + role/tag so each is individually
  recognisable (e.g. skeleton archer = bow + pale bone; stone golem = blocky
  rock; venom spider = round body + legs; cave bat = spread wings; wisp =
  floating glow orb; wolf/boar = quadrupeds; casters = hood + staff; drummer =
  drum; ogre/troll = big brutes; iron sentinel = heavy shield construct; etc.).
- Signal colours reserved for meaning (poison-green/venom, frost-cyan, magic
  glints) per §2 — not decorative.

## G. Implementation plan

1. Extend `tools/asset_gen/generate_textures.ps1` with a per-enemy section
   (reusing its palette, deterministic RNG, `FR`/`P`/`Speckle`/`Outline`/
   `SaveImg` helpers). Emit 23 `enemies/<id>.png` (24×24) and 4
   `enemies/boss_<id>.png` (32×32). Keep the three generic sprites.
2. Add 27 manifest rows: `enemy.<id>.battle` (23) and `boss.<id>.battle` (4).
3. **Definition-of-done guard:** extend `tests/test_presentation_lint.cpp` so
   every `db.enemies()` id has `enemy.<id>.battle` and every `db.bosses()` id
   has `boss.<id>.battle` in the shipped manifest — a missing one fails `[lint]`.
4. `assets/credits.md` provenance row; `docs/asset_pipeline.md` and
   `docs/art_bible.md` notes; capture spot-check (the existing battle scenes and
   `23_battle_targeting` now render bespoke enemy art).

## H. Data, schema, save compatibility

None touched. No content schema, save format, settings, or scoreboard field
changes. No generation-version implication (art is presentation, not seed
output). Battle and simulator tests are untouched.

## I. Automated validation

- Full suite green, including the extended `[lint]` per-id sprite guard (which
  fails if any enemy/boss id lacks a sprite — verify by deliberately removing a
  manifest row).
- `generate_textures.ps1` reruns byte-identical.
- `--capture` run overflow-clean (unchanged scene set; enemies now render their
  own art).

## J. Manual validation for the owner

- At native 426×240, enter battles across themes/depths and confirm every enemy
  and boss reads as its **own** creature; tier (normal/elite/boss) is obvious
  from shape + size + accent; a grayscale glance still distinguishes tiers.
- Art-direction acceptance of the sprite set (owner-only call).

## K. Acceptance criteria

- Every shipped enemy/boss id resolves to its own sprite; the lint fails on a
  deliberately removed row; generators rerun byte-identical; battle layout
  unchanged; no gameplay/behaviour change.

## L. Required documentation updates on completion

- This note (as-implemented record + deviations); `docs/milestones.md`
  (status); `assets/credits.md`; `docs/asset_pipeline.md`; `docs/art_bible.md`
  (per-id enemy identity realised).

## M. As-implemented record (2026-07-20)

Implemented on top of the M25 approval commit. No gameplay/simulation code
changed.

- **Assets:** `tools/asset_gen/generate_textures.ps1` extended with a per-enemy
  section producing 23 `textures/enemies/<id>.png` (16 normal + 7 elite, 24×24)
  and 4 `textures/enemies/boss_<id>.png` (32×32). The three generic tier sprites
  are retained as fallbacks. 27 `enemy.<id>.battle` / `boss.<id>.battle`
  manifest rows added; `assets/credits.md` provenance row added.
- **Lint guard:** `tests/test_presentation_lint.cpp` now checks that every
  `db.enemies()` and `db.bosses()` id resolves to its own sprite. Verified it
  **fails** when a row is removed (dropped `enemy.wisp.battle` →
  `test_presentation_lint.cpp` FAILED on `enemy.wisp.battle`), then restored.
- **Validation (all run this session):** full suite **252/252**
  (`ctest --preset debug`); `[lint]` green with the new guard (2914 assertions);
  `--capture` **23/23 overflow-clean** with no missing-texture/placeholder
  warnings — the 5-enemy and boss battle scenes now render five/one distinct
  creatures instead of repeated tier blobs; `generate_textures.ps1` reruns
  **byte-identical** (30 enemy PNGs), and committed non-enemy textures are
  unchanged.

### Deviations from the plan

- **Extended `generate_textures.ps1` rather than a sibling script** (the plan
  allowed either). The new section is **appended** after all existing art and
  **reseeds a local RNG**, so the shared speckle stream entering it is
  unperturbed and every previously-committed PNG regenerates byte-identically.
- **Four dark-palette bodies raised for contrast.** `dark_acolyte`,
  `shadow_stalker`, `grave_chanter`, and the `crystal_sorcerer` robe were too
  close to the battle background (`{18,16,24}`) to read; their body/trim values
  were lifted one band (staying within the §2 ramps) while keeping the dark
  theme — the art bible's "readable first" rule (§1). Verified against the
  battle background.
- Boss sprite files are named `boss_<id>.png` (manifest id `boss.<id>.battle`)
  to sit beside the enemy sprites in `textures/enemies/`.
- Art quality is "deliberate simple pixel art" (art bible §11) — distinctness
  and tier readability at 426×240 are the owner's manual art-direction call.
