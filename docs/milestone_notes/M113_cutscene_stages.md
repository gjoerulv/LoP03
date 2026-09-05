# M113 — Cutscene stages

**Status:** implemented, awaiting manual approval
**Program:** M109–M116 (owner-authorized 2026-09-02 via the approved plan).
**No version motion:** rules 19, generation 24, saves v1, content v1;
manifest v2 with five new texture ids.

## Scope (plan section M113; owner section 20)

The Hooded Goose scenes stop playing over the M97 flat sky/floor fills:
a town-triggered scene (arrivals, the finale, the roadside Stranger, the
new-game prologue) plays over a **mountainous town panorama**, and a
dungeon-triggered scene (the Stranger's story rooms, the M110 patrol
tales) over the **dungeon theme's own stage** — Keep, Mine, Forest, Goosy.
Clean stage context: the caller names the stage, never the scene id.

## What was built

- **`render/CutsceneBackdrop.hpp`** (pure): `CutsceneStage { Panorama,
  Keep, Mine, Forest, Goosy }`, `cutsceneStageForTheme(themeId)`
  (unknown/empty → Panorama, fail-soft), `cutsceneStageTextureId(stage)`
  → `bg.cutscene.<stage>`, `kCutsceneStageCount`.
- **`CutsceneState`**: a trailing `render::CutsceneStage stage =
  Panorama` ctor parameter (every existing site compiles unchanged —
  PartyCreationState, TownState ×4, DebugMenuState, the captures), and
  `render()` draws `ui::drawSceneBackground(resources, id, sky, w, h)` —
  the old sky fill is the fallback colour and the old floor band is drawn
  only when the texture is missing; the horizon keyline stays as the
  readability anchor; a 28 % sky-coloured dim strip over the actor zone
  (y 36..106) keeps party, goose, King and Dragon dominant over the art
  (the framed dialogue panel already backs the text, §7). `CutsceneDef`
  gained no theme field.
- **Stage context**: `DungeonState` passes
  `cutsceneStageForTheme(dungeon_.themeId)` at its two push sites (the
  Stranger's story room, the patrol scene); every town site keeps the
  panorama (owner: normal progression and the finale are town scenes).
- **Art** (`generate_textures.ps1`, a new section under its own
  `$script:rng` reseed, on the M27 `New-Bg` recipe with `GPoly` /
  `Stars` / `FloorSpeckle` helpers): five 426×240 scenes — the panorama
  (night sky, sparse stars, a small moon, three desaturating mountain
  bands, a keep and roofs on the near ridge at the far edges, terraced
  earth floor), the Keep (broken parapets along the top with the gap over
  the actors, two dark arches at the sides, a cracked slab floor with
  rubble), the Mine (crystal clusters hanging from the roof and rising at
  the sides, timber supports and a beam, rails and sleepers across the
  floor), the Forest (trunk columns at the sides with bark bands, a dark
  canopy, roots across the floor, three fireflies), the Goosy (a pale
  moon, reeds at the sides, a mist band, still water with faint ripples
  and two moonlight streaks). The actor band (y 36..106) carries only the
  far, low-alpha silhouettes at the edges. Manifest and credits rows; the
  generator re-run wrote only the five new PNGs.
- **Captures:** `135_cutscene_stage_keep` … `138_cutscene_stage_goosy`
  (a story scene on each theme stage; the Goosy one under a patrol tale
  to prove the same scene plays on any stage); `112`/`113`/`114`/`116`
  re-render on the panorama.

## Deviations from the plan

- The stage helpers are a header-only `render/` unit (no `.cpp`) — two
  pure functions did not warrant a translation unit.
- The dim strip is a single sky-coloured fade at 28 % rather than the
  modal-dim convention's alpha — a routine tuning; the capture lint and
  the owner's eye judge it.

## Tests

`tests/test_cutscene_stage.cpp` [cutscene][stage]: the four themes map to
their stages and anything else to the panorama; the five texture ids are
distinct and every one is a texture in the shipped manifest. The M97
cutscene suites are untouched (content, choices, replay semantics
unchanged).

## Compatibility

No version motion. Manifest: five ids added. A missing stage texture
degrades to the M97 flat fills.

## Automated validation

See the completion report below.

## Manual owner checklist

Matrix rows **223–224**: the panorama behind every town scene (new game,
arrivals, the finale with King and Dragon staged, the roadside Stranger)
and the theme stages behind the dungeon ones (a story room and a patrol
tale in each theme). **Judge restraint: the actors and the text must
stay dominant; the art must never compete with the dialogue panel.**

## Documentation updated

`docs/milestones.md` (rows + section), `docs/game_design.md` (the Hooded
Goose passage), `docs/technical_design.md` (§55), `docs/art_bible.md`
(§6 cutscene stages), `docs/asset_pipeline.md` (the stage backgrounds),
`docs/manual_test_matrix.md` (rows 223–224), `assets/credits.md`, this
note.

## Completion report

### 1. Implementation summary

- **Milestone:** M113 — Cutscene stages.
- **Slices completed:** the stage helpers; the state's textured render
  with fallbacks and the dim strip; the dungeon stage context; the five
  scenes; manifest/credits; captures; tests; docs. All completed.
- **Player-facing changes:** every cutscene now plays over a scene — the
  town panorama or the dungeon theme's stage.
- **Engineering changes:** `render/CutsceneBackdrop.hpp` (new),
  `CutsceneState` (stage parameter, textured render), `DungeonState`
  (two push sites).

### 2. Files changed

- **Source:** `src/render/CutsceneBackdrop.hpp` (new),
  `src/states/{CutsceneState.hpp,CutsceneState.cpp,DungeonState.cpp}`,
  `src/capture/CaptureRunner.cpp`.
- **Tests:** `tests/test_cutscene_stage.cpp` (new), `tests/CMakeLists.txt`.
- **Content/data:** `assets/manifest.json` (+5), `assets/credits.md` (+5),
  `assets/textures/backgrounds/cutscene_{panorama,keep,mine,forest,goosy}.png`
  (new), `tools/asset_gen/generate_textures.ps1`.
- **Documentation:** as listed above.
- **Build/release configuration:** none.

### 3. Plan deviations

See "Deviations from the plan" — routine.

### 4. Compatibility

See "Compatibility".

### 5. Automated validation

- **Build:** `cmake --build --preset debug` — clean, zero project-code
  warnings (VS 2022 developer shell).
- **Targeted tests:** `crystal_tests.exe "[cutscene],[stage],[lint],
  [content]"` — **65 cases, 84 008 assertions green** (the new
  `[stage]` cases among them; the M97 cutscene suites unchanged).
- **Art:** `generate_textures.ps1` re-run — `git status` shows only the
  five new `cutscene_*.png`; every earlier PNG byte-identical (the section
  runs under its own rng reseed).
- **Capture lint:** `ArePGeese.exe --capture <dir>` — exit 0, **138/138
  scenes clean** (`135`–`138` new; `112`/`113`/`114`/`116` re-rendered on
  the panorama).
- **Full suite / Release:** ride the program's closing battery.

### 6. Manual owner validation

Matrix rows 223–224.

### 7. Known limitations

- The scenes are restrained by design; the owner may want more motif in
  the floor band (below the dialogue panel's top it is mostly hidden).

### 8. Documentation updated

As listed above.

### 9. Final status

`implemented, awaiting manual approval`
