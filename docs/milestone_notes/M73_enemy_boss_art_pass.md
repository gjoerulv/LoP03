# M73 — Enemy & boss sprite art pass (owner brief, 2026-07-30)

> Status: **complete (approved by the owner 2026-08-05).**
> Scope: every PNG under `assets/textures/enemies/` (67 sprites) plus the
> authoring mechanism that produces them. Party sprites, environments, props
> and UI are explicitly out of scope and are verified byte-identical.

---

## 1. Implementation summary

- **Milestone:** M73 — Enemy & boss sprite art pass
- **Slices completed:** all three phases of the owner brief, in order.
  - **Phase 1 — review harness.** `tools/asset_gen/preview.ps1` (new).
    Completed and used after every batch.
  - **Phase 2 — authoring change.** Ellipse-primitive drawing replaced with
    explicit ASCII pixel grids + a `Draw-Grid` helper. Completed.
  - **Phase 3 — redraw.** All 67 sprites redrawn across nine family batches,
    each previewed and visually inspected before being accepted. Completed.
- **Player-facing changes:** every enemy and boss in the game looks different.
  Enemies now read as what they are at 24×24 (a goblin grunt is a goblin
  grunt); bosses are 36×36 instead of 32×32 and carry a structural five-point
  crown, a wider planted stance and absurdly oversized equipment. Danger tier
  is carried by shape and posture, not hue.
- **Engineering changes:**
  - Enemy/boss art is authored as one reviewable ASCII block per sprite with a
    single-character palette key bound to the art-bible ramps.
  - The whole enemy/boss section now consumes **no RNG at all** — every speckle
    pixel is placed by hand. This retires the fragility the M49 note warned
    about, where one stray `Speckle` silently re-rolled every sprite generated
    after it. Adding, removing or reordering a sprite can no longer shift any
    other file's bytes.
  - `Draw-Grid` validates every grid (ragged rows, unknown palette keys) and
    reports *all* faults in one throw.
  - `Save-EnemyGrid` hard-fails any `boss_*` sprite that is not 36×36, so the
    clip-safe boss canvas cannot drift unnoticed.
  - The five enemy/boss generation blocks previously scattered through the
    generator (M26, M29, M38, M40, M62) are consolidated into one contiguous,
    RNG-free section.

## 2. Files changed

- **Source:** none. No C++ was touched. `BattleState::drawUnit` already
  anchors bottom-centre off `tex.width`/`tex.height`, so the boss canvas change
  is a pure asset change.
- **Tests:** none added or modified. The existing
  `tests/test_presentation_lint.cpp` guard (every content id must have its own
  `enemy.<id>.battle` / `boss.<id>.battle` sprite) continues to pass.
- **Content/data:** none. No sprite ids, manifest roles or `data/*.json`
  entries changed — every sprite is a drop-in replacement.
- **Assets:** all 67 PNGs under `assets/textures/enemies/` regenerated. The
  other 129 generated PNGs are byte-identical (verified, §5).
- **Tools:**
  - `tools/asset_gen/preview.ps1` — **new** review harness.
  - `tools/asset_gen/generate_textures.ps1` — palette ramp completions;
    grid infrastructure; all 67 sprites rewritten; five old enemy blocks
    removed; the now-dead `Ell`/`Eyes`/`Horns`/`Crown` helpers removed.
    `Outline`, `Speckle`, `SaveImg`, `FR`, `P`, `New-Img` are unchanged and
    still used by the non-enemy sections.
- **Documentation:** `docs/milestones.md`, this note, `docs/art_bible.md`,
  `docs/asset_pipeline.md`, and the review sheets under
  `docs/sprite_review/`.
- **Build/release configuration:** none.

## 3. Plan deviations

**One deviation, and it is the important one in this note.**

- **What changed:** bosses are **36×36**, not the 36×44 the brief suggested
  testing.
- **Why:** `BattleState::drawUnit` anchors bottom-centre at
  `sy = enemyBaseY() + 16 - tex.height`, and `BattleState::enemyBaseY()`
  drops from 36 to **20** once a fight fields five or more enemies. Three
  authored boss teams do exactly that — `rush_tyrant` and `abyssal_tyrant`
  (4 minions, 5 units) and `deadly_duck` (5 minions, 6 units). At those
  fights a 44-row sprite computes `sy = 20 + 16 - 44 = -8` and loses its top
  **8 rows — precisely the crown** — off the top of the screen. 36 rows gives
  `sy = 0` exactly: the tallest sprite that never clips in any authored fight.
  Width is bounded by the 40px unit footprint (x 36..76 at native 426×240);
  36 wide spans 38..74, inside the HP-meter edge and clear of the enemy status
  column at `x + 44` = 80.
- **Impact:** bosses gained 27 % more canvas area rather than 55 %. The "reads
  as a boss" work is therefore carried mostly by design — structural crown,
  wider stance, oversized weapon, body plans that differ per boss — rather
  than by size alone. No C++ change, no layout change, nothing overlaps.
- **Owner approval required?:** **Not for what was shipped** (36×36 is
  conservative, reversible and needs no code change), **but yes if you want to
  go taller.** Exceeding 36 rows requires changing `enemyBaseY()`, which moves
  the enemy column for every 5+ enemy fight — a player-facing battle-layout
  change, and therefore your call under the CLAUDE.md escalation rules. Say
  the word and I will bring you a proposal rather than doing it silently.

**A second, smaller decision that needs your explicit sign-off:** the art
bible palette was **extended**, per the brief's instruction to "propose the
bible update in the same PR rather than sneaking the colour in". Details in §4.

## 4. Palette extension (needs owner sign-off)

The generator has, since M26, used four colours that were never in art bible
§2 — `maroon`/`maroonD` (brute flesh) and `bossBody`/`bossD` (boss violet-black)
— plus two undocumented whites added with the M62 geese. Each was a **two-step**
ramp, which cannot carry the bible's own mandated 3-band shadow/base/highlight
rule. M73 completes them and records them in §2:

| Ramp | Steps | Status |
|---|---|---|
| Flesh/brute | `#2E1820` `#43242B` `#5C3038` `#7A4650` | middle two already shipped since M26; **darkest + highlight are new** |
| Boss void | `#1E1628` `#2A2038` `#3A2C4E` `#4E3C68` | middle two already shipped since M26; **darkest + highlight are new** |
| Neutral white | `#B8B8BC` `#D8D8D4` `#F2F2F0` | lighter two already shipped since M62; **shadow step is new** |

No previously shipped hue changed value — the additions are the missing ends of
ramps the game already uses. **Six new hex values total.** If you would rather
not grow the palette, the alternative is dropping brute flesh and boss violet
entirely and re-tinting those sprites into the stone/earth/veg ramps, which
would cost the enemy roster a lot of its colour separation. I recommend
accepting the extension.

## 5. Compatibility

- **Save files:** no impact. No save field touched.
- **Settings files:** no impact.
- **Content schemas:** no impact. No `data/*.json` file changed.
- **Deterministic seeds:** no impact. Art is not gameplay content; the
  generator's own determinism is verified below.
- **Score records:** no impact.
- **Packaged assets:** file **names, ids and manifest roles are unchanged** —
  every sprite is a drop-in replacement. `assets/manifest.json` was not
  edited. The only packaging-relevant change is that 15 boss PNGs are 36×36
  instead of 32×32.

## 6. Automated validation

- **Configure command:** not re-run (no CMake input changed); the existing
  `build-msvc` tree from `cmake --preset msvc-debug` was reused.
- **Build command:**
  `cmd /c "call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --build --preset debug"`
- **Test command:**
  `cmd /c "call "...\vcvars64.bat" >nul && ctest --preset debug"`
- **Results:** build succeeded. **607/607 tests passed, 0 failed** (365 s).
  This includes `[lint]`, which fails the suite if any content enemy/boss id
  lacks its own sprite.
- **Asset verification (run in this session):**
  - **Non-enemy PNGs byte-identical:** SHA-256 over all 196 generated PNGs
    before and after. 67 enemy PNGs changed; **0 non-enemy PNGs changed**; 0
    added; 0 removed.
  - **Generator byte-stable:** two consecutive full reruns produced
    byte-identical output across all 196 PNGs, and matched the earlier run.
  - **Completeness:** `assets/textures/enemies/` was deleted and regenerated
    from scratch — exactly **67** PNGs were written, so no sprite is a stale
    leftover.
- **Warnings:** none introduced. (`vcvars64.bat` prints a benign
  `'vswhere.exe' is not recognized` line in this environment; the build and
  test runs completed normally regardless.)
- **Skipped validation and reason:** the Release preset was not built or
  tested — this milestone changes no compiled code, so Debug and Release
  compile identically. If you want it confirmed, run
  `cmake --build --preset release` then `ctest --preset release` from the
  VS2022 developer shell; expect 603/603 green. The `--capture` scene sweep
  was also not run; expect the enemy-bearing scenes to differ visually by
  design.

## 7. Manual owner validation

Automated tests cannot judge art. These are the checks that matter:

1. **Look at the sheets first.** `docs/sprite_review/sprites_contact.png`,
   `sprites_silhouette.png`, and `sprites_strip4x.png`. The baseline set is
   kept alongside as `baseline_*.png` for a direct before/after.
2. **The silhouette sheet is the binding artefact.** Scan it for two enemies
   that read as the same shape. My own reading: none collide, but the closest
   remaining cluster is `boss_battle` / `boss_rush_tyrant` /
   `boss_obsidian_colossus` (three crowned hulks with side slabs) — note that
   `boss_battle` is an unreachable fallback in a shipping build, since `[lint]`
   guarantees every content id has bespoke art.
3. **In-game, at native resolution.** Run a dungeon and confirm enemies read
   at 426×240 without leaning on the name panel.
4. **The three tall-fight boss battles** — these are the ones the 36-row limit
   exists for. Fight **Rush Tyrant**, **Abyssal Tyrant** and the **Deadly
   Duck** (6 units) and confirm: no boss sprite is clipped at the top of the
   screen, nothing overlaps the party rows, the HP numerals, or the focus
   brackets when you target each unit in turn.
5. **Grayscale/high-contrast check.** Turn on High Contrast in Settings and
   confirm tier still reads — normal vs elite vs boss should be obvious from
   shape, size and posture alone.
6. **Palette decision.** Accept or reject the §4 bible extension.
7. **Boss height decision.** Tell me whether you want me to propose the
   `enemyBaseY()` change that would allow taller bosses (§3).
8. **On failure:** send the sprite name and a screenshot; each sprite is now a
   single ASCII block in `generate_textures.ps1` and can be edited row by row.

## 8. Known limitations

- **Bosses are 36×36, not 36×44.** Deliberate, with the arithmetic in §3.
- **Weakest remaining silhouette cluster:** the three crowned hulks named in
  §7.2, and — more mildly — `boss_blight_matron` / `boss_crystal_sorcerer` /
  `boss_deep_king`, which share a pot-bodied caster stance and are told apart
  by their staff heads (round / diamond / axe).
- **`venom_spider` and `war_drummer`** are the two normal enemies I am least
  happy with; both are distinct and legible, but neither has the caricature
  punch of `goblin_grunt` or `corpse_hound`.
- **The five geese deliberately share a body.** They are a gang; each is told
  apart above the neck and at the wing. `evil_goose_bogfeather` is the "plain"
  one and so has the least distinctive outline of the five.
- **The review sheets embed system-font labels.** Regenerating them on a
  machine without Consolas will produce a slightly different PNG. They are
  documentation artefacts, not game assets, and nothing depends on their bytes.
- Art quality, readability in motion, and whether these actually feel like the
  M46 mood are owner-validation items and cannot be automated.

## 9. Documentation updated

- `docs/milestones.md` — M73 row and section added; program text updated to
  place M73 after M72 and before M23/M24. **Also fixed a pre-existing gap:**
  the status table stopped at M66 while the prose documented M67–M72, so
  adding an M73 row would have orphaned it. Rows for M67–M72 were added with
  the statuses the surrounding prose already states
  (`implemented, awaiting manual approval`). No status was invented or changed.
- `docs/milestone_notes/M73_enemy_boss_art_pass.md` — this note.
- `docs/art_bible.md` — §2 ramp completions (with the M73 provenance note);
  §3 battle-sprite sizes corrected to "bosses 36×36" with the clip arithmetic;
  §5 enemy silhouette paragraph rewritten for the M73 rules; §11 production
  notes updated for grid authoring and the preview harness.
- `docs/asset_pipeline.md` — the enemy battle-sprite section updated for the
  36×36 boss canvas, the ASCII-grid authoring model, the RNG-free guarantee,
  and the `preview.ps1` review loop.
- `docs/sprite_review/` — contact, silhouette and 1× strip sheets for all 67
  sprites, plus the pre-M73 baseline set.
- `assets/credits.md` — an M73 provenance row added. The earlier
  M15/M26/M29/M38/M40/M49/M62 enemy rows recorded bosses as 32×32, which the
  36×36 canvas made stale; the new row states that it supersedes their **art**
  (same file names, ids and roles, new pixels) and records the 36×36 canvas,
  the ASCII-grid authoring model and the RNG-free guarantee. The historical
  rows are kept — this file is a provenance ledger, not a snapshot.
- Checked and intentionally unchanged: `docs/game_design.md` (no behaviour
  change), `docs/technical_design.md` (no architecture change),
  `assets/manifest.json` (no new asset roles, paths or files — every sprite is
  a drop-in replacement for an entry that already exists).

## 10. Final status

`complete (approved 2026-08-05)`
